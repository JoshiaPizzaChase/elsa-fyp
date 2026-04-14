#!/usr/bin/env python3

import argparse
import errno
import json
import logging
import socket
import stat
import shutil
import subprocess
import time
import tomllib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent
TEMPLATE_DIR = REPO_ROOT / "template"
BUILD_SERVICES_DIR = REPO_ROOT / "build" / "services"
INSTALL_BASE_DIR = Path("/usr/local/bin")
SYSTEMD_DIR = Path("/etc/systemd/system")
LOGGER = logging.getLogger("deployment_server")
PORT_RELEASE_TIMEOUT_SECONDS = 20.0
SERVICE_READINESS_TIMEOUT_SECONDS = 30.0

SUPPORTED_SERVICES = {
    "mdp": {
        "build_rel": Path("market_data_processor/market_data_processor"),
        "executable": "market_data_processor",
        "template_toml": "mdp.toml",
        "template_service": "mdp.service",
    },
    "me": {
        "build_rel": Path("matching_engine/matching_engine"),
        "executable": "matching_engine",
        "template_toml": "me.toml",
        "template_service": "me.service",
    },
    "oms": {
        "build_rel": Path("order_manager/order_manager"),
        "executable": "order_manager",
        "template_toml": "oms.toml",
        "template_service": "oms.service",
    },
    "gateway": {
        "build_rel": Path("gateway/gateway"),
        "executable": "gateway",
        "template_toml": "gateway.toml",
        "template_service": "gateway.service",
    },
}

LISTEN_PORT_KEY_BY_SERVICE = {
    "mdp": "ws_port",
    "me": "matching_engine_port",
    "oms": "order_manager_port",
    "gateway": "fix_server_port",
}


class DeploymentError(Exception):
    pass


def _validate_server_name(name: str) -> None:
    if not name:
        raise DeploymentError("server_name is required")
    for ch in name:
        if not (ch.isalnum() or ch in {"-", "_"}):
            raise DeploymentError(
                "server_name can only contain letters, digits, '-' and '_'"
            )


def _render_toml(data: dict[str, Any]) -> str:
    lines: list[str] = []
    for key, value in data.items():
        if isinstance(value, str):
            escaped = value.replace("\\", "\\\\").replace('"', '\\"')
            lines.append(f'{key} = "{escaped}"')
        elif isinstance(value, list):
            rendered_items: list[str] = []
            for item in value:
                if isinstance(item, str):
                    escaped_item = item.replace("\\", "\\\\").replace('"', '\\"')
                    rendered_items.append(f'"{escaped_item}"')
                elif isinstance(item, bool):
                    rendered_items.append("true" if item else "false")
                elif isinstance(item, (int, float)):
                    rendered_items.append(str(item))
                else:
                    raise DeploymentError(
                        f"Unsupported TOML list value type for key '{key}': {type(item).__name__}"
                    )
            lines.append(f"{key} = [{', '.join(rendered_items)}]")
        elif isinstance(value, bool):
            lines.append(f"{key} = {'true' if value else 'false'}")
        elif isinstance(value, (int, float)):
            lines.append(f"{key} = {value}")
        else:
            raise DeploymentError(
                f"Unsupported TOML value type for key '{key}': {type(value).__name__}"
            )
    return "\n".join(lines) + "\n"


def _convert_value_to_template_type(template_value: Any, incoming_value: Any) -> Any:
    if isinstance(template_value, str):
        if not isinstance(incoming_value, str):
            return str(incoming_value)
        return incoming_value

    if isinstance(template_value, bool):
        if isinstance(incoming_value, bool):
            return incoming_value
        if isinstance(incoming_value, str):
            lowered = incoming_value.strip().lower()
            if lowered in {"true", "1", "yes", "y"}:
                return True
            if lowered in {"false", "0", "no", "n"}:
                return False
        raise DeploymentError(f"Expected boolean value, got: {incoming_value!r}")

    if isinstance(template_value, int) and not isinstance(template_value, bool):
        if isinstance(incoming_value, int) and not isinstance(incoming_value, bool):
            return incoming_value
        if isinstance(incoming_value, str):
            try:
                return int(incoming_value.strip())
            except ValueError as exc:
                raise DeploymentError(
                    f"Expected integer value, got: {incoming_value!r}"
                ) from exc
        raise DeploymentError(f"Expected integer value, got: {incoming_value!r}")

    if isinstance(template_value, float):
        if isinstance(incoming_value, (float, int)) and not isinstance(
            incoming_value, bool
        ):
            return float(incoming_value)
        if isinstance(incoming_value, str):
            try:
                return float(incoming_value.strip())
            except ValueError as exc:
                raise DeploymentError(
                    f"Expected float value, got: {incoming_value!r}"
                ) from exc
        raise DeploymentError(f"Expected float value, got: {incoming_value!r}")

    raise DeploymentError(f"Unsupported TOML template type: {type(template_value).__name__}")


def _parse_csv_string(raw_value: Any, field_name: str) -> list[str]:
    if isinstance(raw_value, str):
        items = [part.strip() for part in raw_value.split(",")]
    elif isinstance(raw_value, list):
        items = []
        for entry in raw_value:
            if not isinstance(entry, str):
                raise DeploymentError(
                    f"'{field_name}' list entries must be strings, got {type(entry).__name__}"
                )
            items.append(entry.strip())
    else:
        raise DeploymentError(
            f"'{field_name}' must be a comma-separated string or list of strings"
        )

    values = [item for item in items if item]
    if not values:
        raise DeploymentError(f"'{field_name}' must contain at least one value")
    return values


def _run_cmd(*cmd: str) -> None:
    LOGGER.info("Running command: %s", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        stderr = result.stderr.strip() or "<no stderr>"
        raise DeploymentError(f"Command failed: {' '.join(cmd)}; stderr: {stderr}")
    stdout = result.stdout.strip()
    if stdout:
        LOGGER.info("Command output (%s): %s", " ".join(cmd), stdout)


def _is_service_active(service_unit: str) -> bool:
    result = subprocess.run(
        ("systemctl", "is-active", "--quiet", service_unit),
        capture_output=True,
        text=True,
        check=False,
    )
    return result.returncode == 0


def _parse_port(value: Any, field_name: str) -> int:
    if isinstance(value, bool):
        raise DeploymentError(f"'{field_name}' must be a valid TCP port")

    if isinstance(value, int):
        port = value
    elif isinstance(value, str):
        try:
            port = int(value.strip())
        except ValueError as exc:
            raise DeploymentError(f"'{field_name}' must be a valid TCP port") from exc
    else:
        raise DeploymentError(f"'{field_name}' must be a valid TCP port")

    if not (1 <= port <= 65535):
        raise DeploymentError(f"'{field_name}' must be in range 1..65535")
    return port


def _resolve_listen_port(
    service_name: str, params: dict[str, Any], final_config: dict[str, Any]
) -> int | None:
    key = LISTEN_PORT_KEY_BY_SERVICE.get(service_name)
    if key is None:
        return None

    if key in params:
        return _parse_port(params[key], key)
    if key in final_config:
        return _parse_port(final_config[key], key)
    return None


def _wait_until_port_released(port: int, timeout_seconds: float) -> None:
    deadline = time.monotonic() + timeout_seconds
    while True:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.bind(("0.0.0.0", port))
            return
        except OSError as exc:
            if exc.errno != errno.EADDRINUSE:
                raise DeploymentError(
                    f"Failed to probe port {port} availability: {exc}"
                ) from exc
            if time.monotonic() >= deadline:
                raise DeploymentError(
                    f"Port {port} is still in use after waiting {timeout_seconds:.1f}s"
                ) from exc
            time.sleep(0.2)
        finally:
            sock.close()


def _wait_until_service_ready(port: int, timeout_seconds: float) -> None:
    """
    Poll the given port until a connection can be established (service is listening)
    or timeout is reached.
    
    Raises:
        DeploymentError: If the service doesn't become ready within the timeout.
    """
    start = time.monotonic()
    last_error = None
    
    while time.monotonic() - start < timeout_seconds:
        try:
            # Try to connect to the service
            with socket.create_connection(("127.0.0.1", port), timeout=1.0):
                LOGGER.info("Service is ready on port %d", port)
                return  # Connection successful, service is ready!
        except (socket.error, OSError) as exc:
            last_error = exc
            time.sleep(0.5)  # Wait 500ms before retry
    
    # Timeout reached
    raise DeploymentError(
        f"Service did not become ready on port {port} within {timeout_seconds}s. "
        f"Last error: {last_error}"
    )


def _try_relabel_path(path: Path, recursive: bool = False) -> None:
    restorecon_bin = shutil.which("restorecon")
    if restorecon_bin is None:
        LOGGER.warning("restorecon not found; skipping SELinux relabel for %s", path)
        return

    cmd: list[str] = [restorecon_bin]
    if recursive:
        cmd.append("-R")
    cmd.append(str(path))

    LOGGER.info("Relabeling path for SELinux: %s", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        stderr = result.stderr.strip() or "<no stderr>"
        raise DeploymentError(
            f"Failed to relabel path '{path}' with restorecon: {stderr}"
        )


def deploy_service(service_name: str, server_name: str, params: dict[str, Any]) -> dict[str, Any]:
    LOGGER.info(
        "Starting deployment: service_name=%s server_name=%s param_keys=%s",
        service_name,
        server_name,
        sorted(params.keys()),
    )
    if service_name not in SUPPORTED_SERVICES:
        raise DeploymentError(
            f"Unsupported service_name '{service_name}'. "
            f"Supported values: {', '.join(sorted(SUPPORTED_SERVICES))}"
        )

    _validate_server_name(server_name)

    spec = SUPPORTED_SERVICES[service_name]
    executable_src = BUILD_SERVICES_DIR / spec["build_rel"]
    toml_template_path = TEMPLATE_DIR / spec["template_toml"]
    service_template_path = TEMPLATE_DIR / spec["template_service"]
    LOGGER.info(
        "Resolved sources: executable=%s toml_template=%s service_template=%s",
        executable_src,
        toml_template_path,
        service_template_path,
    )

    if not executable_src.is_file():
        raise DeploymentError(f"Executable not found: {executable_src}")
    if not toml_template_path.is_file():
        raise DeploymentError(f"TOML template not found: {toml_template_path}")
    if not service_template_path.is_file():
        raise DeploymentError(f"Service template not found: {service_template_path}")

    with toml_template_path.open("rb") as fh:
        template_data = tomllib.load(fh)

    if not isinstance(template_data, dict) or any(
        isinstance(v, dict) for v in template_data.values()
    ):
        raise DeploymentError(
            f"Template TOML must be a flat key/value document: {toml_template_path}"
        )

    gateway_special_keys = {"fix_server_port", "whitelist"}
    me_mdp_oms_special_keys = {"active_symbols"}
    
    if service_name == "gateway":
        allowed_non_template_keys = gateway_special_keys
    elif service_name in {"me", "mdp", "oms"}:
        allowed_non_template_keys = me_mdp_oms_special_keys
    else:
        allowed_non_template_keys = set()
    
    unknown_keys = [
        k for k in params.keys() if k not in template_data and k not in allowed_non_template_keys
    ]
    if unknown_keys:
        raise DeploymentError(
            f"Unknown config fields for service '{service_name}': {', '.join(sorted(unknown_keys))}"
        )

    final_config = dict(template_data)
    server_name_placeholder = "<server name placeholder>"

    for key, value in params.items():
        if key == "active_symbols" and service_name in {"me", "mdp", "oms"}:
            final_config["active_symbols"] = _parse_csv_string(value, "active_symbols")
            LOGGER.info(
                "%s active_symbols parsed: count=%d",
                service_name.upper(),
                len(final_config["active_symbols"]),
            )
            continue
        if key in template_data:
            final_config[key] = _convert_value_to_template_type(template_data[key], value)
            LOGGER.info("Applied config override: %s=%r", key, final_config[key])

    instance_name = f"{service_name}-{server_name}"
    install_dir = INSTALL_BASE_DIR / instance_name
    LOGGER.info("Ensuring install directory exists: %s", install_dir)
    install_dir.mkdir(parents=True, exist_ok=True)

    executable_dest = install_dir / spec["executable"]
    LOGGER.info("Copying executable: %s -> %s", executable_src, executable_dest)
    shutil.copy2(executable_src, executable_dest)
    current_mode = executable_dest.stat().st_mode
    executable_dest.chmod(
        current_mode
        | stat.S_IXUSR
        | stat.S_IXGRP
        | stat.S_IXOTH
    )
    LOGGER.info("Set executable bit on binary: %s", executable_dest)

    config_dest = install_dir / spec["template_toml"]
    LOGGER.info("Writing rendered TOML config: %s", config_dest)
    config_dest.write_text(_render_toml(final_config), encoding="utf-8")

    gateway_cfg_dest: Path | None = None
    if service_name == "gateway":
        if "fix_server_port" not in params:
            raise DeploymentError("Gateway deployment requires 'fix_server_port'")
        if "whitelist" not in params:
            raise DeploymentError("Gateway deployment requires 'whitelist'")

        gateway_cfg_template = TEMPLATE_DIR / "gateway_server.cfg"
        if not gateway_cfg_template.is_file():
            raise DeploymentError(f"Gateway cfg template not found: {gateway_cfg_template}")

        fix_server_port = str(params["fix_server_port"]).strip()
        if not fix_server_port:
            raise DeploymentError("'fix_server_port' cannot be empty")

        whitelist = _parse_csv_string(params["whitelist"], "whitelist")
        LOGGER.info(
            "Gateway cfg settings: fix_server_port=%s whitelist_count=%d",
            fix_server_port,
            len(whitelist),
        )
        cfg_text = gateway_cfg_template.read_text(encoding="utf-8")
        cfg_text = cfg_text.replace("<port placeholder>", fix_server_port)
        cfg_text = cfg_text.replace("<server id>", server_name)
        cfg_text = cfg_text.replace("<server name>", server_name)

        session_blocks = [
            "[SESSION]\n"
            "BeginString=FIX.4.2\n"
            f"SenderCompID={server_name}\n"
            f"TargetCompID={target_comp_id}"
            for target_comp_id in whitelist
        ]
        cfg_text = cfg_text.rstrip() + "\n\n" + "\n\n".join(session_blocks) + "\n"

        gateway_cfg_dest = install_dir / "gateway_server.cfg"
        LOGGER.info("Writing gateway cfg: %s", gateway_cfg_dest)
        gateway_cfg_dest.write_text(cfg_text, encoding="utf-8")

    service_template_text = service_template_path.read_text(encoding="utf-8")
    service_text = service_template_text
    service_text = service_text.replace("<service name>", service_name)
    service_text = service_text.replace(server_name_placeholder, server_name)
    service_text = service_text.replace(f"/usr/local/bin/{service_name}", str(install_dir))
    service_text = service_text.replace(
        f"Alias={service_name}.service", f"Alias={instance_name}.service"
    )

    service_dest = SYSTEMD_DIR / f"{instance_name}.service"
    LOGGER.info("Writing systemd unit file: %s", service_dest)
    service_dest.write_text(service_text, encoding="utf-8")
    listen_port = _resolve_listen_port(service_name, params, final_config)

    # On SELinux-enabled systems, copied files can inherit an incorrect context
    # (e.g., user_home_t), causing systemd EXEC permission failures.
    _try_relabel_path(install_dir, recursive=True)
    _try_relabel_path(service_dest, recursive=False)

    LOGGER.info("Reloading and starting service: %s.service", instance_name)
    _run_cmd("systemctl", "daemon-reload")
    _run_cmd("systemctl", "enable", f"{instance_name}.service")
    service_unit = f"{instance_name}.service"
    if _is_service_active(service_unit):
        LOGGER.info("Stopping active service before start: %s", service_unit)
        _run_cmd("systemctl", "stop", service_unit)
    if listen_port is not None:
        LOGGER.info(
            "Waiting up to %.1fs for port %d to be released",
            PORT_RELEASE_TIMEOUT_SECONDS,
            listen_port,
        )
        _wait_until_port_released(listen_port, PORT_RELEASE_TIMEOUT_SECONDS)
    _run_cmd("systemctl", "start", service_unit)

    # Wait for service to be ready to accept connections
    if listen_port is not None:
        LOGGER.info(
            "Waiting up to %.1fs for service to be ready on port %d",
            SERVICE_READINESS_TIMEOUT_SECONDS,
            listen_port,
        )
        _wait_until_service_ready(listen_port, SERVICE_READINESS_TIMEOUT_SECONDS)

    result = {
        "service_name": service_name,
        "server_name": server_name,
        "instance_name": instance_name,
        "install_dir": str(install_dir),
        "executable": str(executable_dest),
        "config_file": str(config_dest),
        "gateway_server_cfg": str(gateway_cfg_dest) if gateway_cfg_dest else None,
        "unit_file": str(service_dest),
    }
    LOGGER.info("Deployment completed: %s", result)
    return result


def remove_service(service_type: str, server_name: str) -> dict[str, Any]:
    LOGGER.info(
        "Starting removal: service_type=%s server_name=%s",
        service_type,
        server_name,
    )
    if service_type not in SUPPORTED_SERVICES:
        raise DeploymentError(
            f"Unsupported service_type '{service_type}'. "
            f"Supported values: {', '.join(sorted(SUPPORTED_SERVICES))}"
        )

    _validate_server_name(server_name)

    instance_name = f"{service_type}-{server_name}"
    service_unit = f"{instance_name}.service"
    service_dest = SYSTEMD_DIR / service_unit
    install_dir = INSTALL_BASE_DIR / instance_name

    service_file_existed = service_dest.is_file()
    install_dir_existed = install_dir.exists()

    if service_file_existed:
        LOGGER.info("Stopping and disabling service unit: %s", service_unit)
        _run_cmd("systemctl", "stop", service_unit)
        _run_cmd("systemctl", "disable", service_unit)
        LOGGER.info("Removing unit file: %s", service_dest)
        service_dest.unlink()
    else:
        LOGGER.info("Unit file not found, skipping service stop/remove: %s", service_dest)

    if install_dir_existed:
        LOGGER.info("Removing install directory: %s", install_dir)
        shutil.rmtree(install_dir)
    else:
        LOGGER.info("Install directory not found, skipping: %s", install_dir)

    LOGGER.info("Reloading systemd daemon after removal")
    _run_cmd("systemctl", "daemon-reload")

    result = {
        "service_type": service_type,
        "server_name": server_name,
        "instance_name": instance_name,
        "unit_file": str(service_dest),
        "install_dir": str(install_dir),
        "removed_unit_file": service_file_existed,
        "removed_install_dir": install_dir_existed,
    }
    LOGGER.info("Removal completed: %s", result)
    return result


class DeployRequestHandler(BaseHTTPRequestHandler):
    def _write_json(self, status: int, payload: dict[str, Any]) -> None:
        encoded = json.dumps(payload, ensure_ascii=True).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self) -> None:  # noqa: N802
        LOGGER.info("Received GET request: path=%s client=%s", self.path, self.client_address)
        if self.path == "/health":
            self._write_json(200, {"status": "ok"})
            return
        self._write_json(404, {"error": "Not found"})

    def do_POST(self) -> None:  # noqa: N802
        LOGGER.info("Received POST request: path=%s client=%s", self.path, self.client_address)
        if self.path not in {"/deploy", "/remove_server"}:
            self._write_json(404, {"error": "Not found"})
            return

        content_length_header = self.headers.get("Content-Length")
        if content_length_header is None:
            self._write_json(400, {"error": "Content-Length header is required"})
            return

        try:
            content_length = int(content_length_header)
        except ValueError:
            self._write_json(400, {"error": "Invalid Content-Length header"})
            return

        try:
            raw_body = self.rfile.read(content_length)
            payload = json.loads(raw_body.decode("utf-8"))
        except json.JSONDecodeError:
            self._write_json(400, {"error": "Invalid JSON body"})
            return

        if not isinstance(payload, dict):
            self._write_json(400, {"error": "JSON body must be an object"})
            return
        LOGGER.info("POST request payload keys: %s", sorted(payload.keys()))

        try:
            if self.path == "/deploy":
                service_name = payload.get("service_name") or payload.get("service")
                server_name = payload.get("server_name")
                config_params = payload.get("params")

                if not isinstance(service_name, str) or not service_name.strip():
                    self._write_json(
                        400,
                        {
                            "error": "service_name (or service) is required",
                            "supported_services": sorted(SUPPORTED_SERVICES.keys()),
                        },
                    )
                    return

                if not isinstance(server_name, str) or not server_name.strip():
                    self._write_json(400, {"error": "server_name is required"})
                    return

                if config_params is None:
                    config_params = {
                        k: v
                        for k, v in payload.items()
                        if k not in {"service_name", "service", "server_name", "params"}
                    }

                if not isinstance(config_params, dict):
                    self._write_json(400, {"error": "params must be a JSON object"})
                    return

                result = deploy_service(
                    service_name=service_name.strip(),
                    server_name=server_name.strip(),
                    params=config_params,
                )
                self._write_json(200, {"status": "deployed", "result": result})
                return

            server_name = payload.get("server_name")
            service_type = payload.get("service_type")

            if not isinstance(server_name, str) or not server_name.strip():
                self._write_json(400, {"error": "server_name is required"})
                return

            if not isinstance(service_type, str) or not service_type.strip():
                self._write_json(
                    400,
                    {
                        "error": "service_type is required",
                        "supported_services": sorted(SUPPORTED_SERVICES.keys()),
                    },
                )
                return

            result = remove_service(
                service_type=service_type.strip(),
                server_name=server_name.strip(),
            )
            self._write_json(200, {"status": "removed", "result": result})
            return
        except DeploymentError as exc:
            LOGGER.exception("Deployment error")
            self._write_json(400, {"error": str(exc)})
            return
        except PermissionError as exc:
            LOGGER.exception("Permission error")
            self._write_json(500, {"error": f"Permission denied: {exc}"})
            return
        except OSError as exc:
            LOGGER.exception("OS error")
            self._write_json(500, {"error": f"OS error: {exc}"})
            return

    def log_message(self, fmt: str, *args: Any) -> None:
        return


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Deployment REST server")
    parser.add_argument("--host", default="0.0.0.0", help="Bind host")
    parser.add_argument("--port", type=int, default=8080, help="Bind port")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
    )
    httpd = ThreadingHTTPServer((args.host, args.port), DeployRequestHandler)
    LOGGER.info("Deployment server listening on http://%s:%d", args.host, args.port)
    httpd.serve_forever()


if __name__ == "__main__":
    main()
