#!/usr/bin/env python3

import argparse
import json
import shutil
import subprocess
import tomllib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent
TEMPLATE_DIR = REPO_ROOT / "template"
BUILD_SERVICES_DIR = REPO_ROOT / "build" / "services"
INSTALL_BASE_DIR = Path("/usr/local/bin")
SYSTEMD_DIR = Path("/etc/systemd/system")

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
    "mm": {
        "build_base_dir": BUILD_SIMULATION_DEPLOYMENT_DIR,
        "build_rel": Path("create_n_market_makers"),
        "executable": "create_n_market_makers",
        "template_toml": "mm_config.toml",
        "template_service": "mm.service",
    },
    "nt": {
        "build_base_dir": BUILD_SIMULATION_DEPLOYMENT_DIR,
        "build_rel": Path("create_n_noise_traders"),
        "executable": "create_n_noise_traders",
        "template_toml": "nt_config.toml",
        "template_service": "nt.service",
    },
    "it": {
        "build_base_dir": BUILD_SIMULATION_DEPLOYMENT_DIR,
        "build_rel": Path("create_n_informed_traders"),
        "executable": "create_n_informed_traders",
        "template_toml": "it_config.toml",
        "template_service": "it.service",
    },
    "oracle": {
        "build_base_dir": BUILD_SIMULATION_DEPLOYMENT_DIR,
        "build_rel": Path("create_oracle"),
        "executable": "create_oracle",
        "template_toml": "oracle_config.toml",
        "template_service": "oracle.service",
    },
}

LISTEN_PORT_KEY_BY_SERVICE = {
    "mdp": "ws_port",
    "me": "matching_engine_port",
    "oms": "order_manager_port",
    "gateway": "fix_server_port",
    "oracle": "oracle_ws_port",
}


class DeploymentError(Exception):
    pass


def _render_toml(data: dict[str, Any]) -> str:
    lines: list[str] = []
    for key, value in data.items():
        if isinstance(value, str):
            escaped = value.replace("\\", "\\\\").replace('"', '\\"')
            lines.append(f'{key} = "{escaped}"')
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


def _run_cmd(*cmd: str) -> None:
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


def _parse_non_negative_int(value: Any, field_name: str) -> int:
    if isinstance(value, bool):
        raise DeploymentError(f"'{field_name}' must be a non-negative integer")
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, str):
        try:
            parsed = int(value.strip())
        except ValueError as exc:
            raise DeploymentError(f"'{field_name}' must be a non-negative integer") from exc
    else:
        raise DeploymentError(f"'{field_name}' must be a non-negative integer")
    if parsed < 0:
        raise DeploymentError(f"'{field_name}' must be a non-negative integer")
    return parsed


def _validate_group_tag(tag: str) -> None:
    if not tag:
        raise DeploymentError("'group_tag' cannot be empty")
    for ch in tag:
        if not (ch.isalnum() or ch in {"-", "_"}):
            raise DeploymentError("'group_tag' can only contain letters, digits, '-' and '_'")


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
    if service_name not in SUPPORTED_SERVICES:
        raise DeploymentError(
            f"Unsupported service_name '{service_name}'. "
            f"Supported values: {', '.join(sorted(SUPPORTED_SERVICES))}"
        )

    spec = SUPPORTED_SERVICES[service_name]
    executable_src = BUILD_SERVICES_DIR / spec["build_rel"]
    toml_template_path = TEMPLATE_DIR / spec["template_toml"]
    service_template_path = TEMPLATE_DIR / spec["template_service"]

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
    simulation_special_keys = {
        "gateway_host",
        "gateway_port",
        "active_symbols",
        "group_tag",
        "sender_comp_ids",
    }
    
    if service_key == "gateway":
        allowed_non_template_keys = gateway_special_keys
    elif service_key in {"me", "mdp", "oms"}:
        allowed_non_template_keys = me_mdp_oms_special_keys
    elif service_key in {"mm", "nt", "it"}:
        allowed_non_template_keys = simulation_special_keys
    elif service_key == "oracle":
        allowed_non_template_keys = {"active_symbols"}
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
    for key, value in params.items():
        final_config[key] = _convert_value_to_template_type(template_data[key], value)

    group_tag: str | None = None
    if service_key in {"mm", "nt", "it"} and "group_tag" in params:
        group_tag = str(params["group_tag"]).strip()
        _validate_group_tag(group_tag)
    instance_name = (
        f"{service_name}-{group_tag}-{server_name}" if group_tag else f"{service_name}-{server_name}"
    )
    install_dir = INSTALL_BASE_DIR / instance_name
    install_dir.mkdir(parents=True, exist_ok=True)

    executable_dest = install_dir / spec["executable"]
    shutil.copy2(executable_src, executable_dest)
    executable_dest.chmod(0o755)

    config_dest = install_dir / spec["template_toml"]
    config_dest.write_text(_render_toml(final_config), encoding="utf-8")

    gateway_cfg_dest: Path | None = None
    simulation_cfg_files: list[str] = []
    if service_key == "gateway":
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
    elif service_key in {"mm", "nt", "it"}:
        if "gateway_host" not in params:
            raise DeploymentError(f"{service_key} deployment requires 'gateway_host'")
        if "gateway_port" not in params:
            raise DeploymentError(f"{service_key} deployment requires 'gateway_port'")
        gateway_host = str(params["gateway_host"]).strip()
        if not gateway_host:
            raise DeploymentError("'gateway_host' cannot be empty")
        gateway_port = _parse_port(params["gateway_port"], "gateway_port")

        count_key_by_service = {
            "mm": "num_market_makers",
            "nt": "num_noise_traders",
            "it": "num_informed_traders",
        }
        count_key = count_key_by_service[service_key]
        bot_count = _parse_non_negative_int(final_config.get(count_key, 0), count_key)
        cfg_prefix_value = final_config.get("cfg_prefix", "")
        if not isinstance(cfg_prefix_value, str) or not cfg_prefix_value.strip():
            raise DeploymentError(f"'{count_key}' requires a non-empty cfg_prefix")
        cfg_prefix = cfg_prefix_value.strip()

        fix_cfg_template = TEMPLATE_DIR / "simulation_client.cfg"
        if not fix_cfg_template.is_file():
            raise DeploymentError(f"Simulation client cfg template not found: {fix_cfg_template}")
        cfg_text_template = fix_cfg_template.read_text(encoding="utf-8")

        store_dir = install_dir / "store"
        log_dir = install_dir / "log"
        store_dir.mkdir(parents=True, exist_ok=True)
        log_dir.mkdir(parents=True, exist_ok=True)

        sender_comp_ids_raw = params.get("sender_comp_ids")
        sender_comp_ids: list[str] | None = None
        if sender_comp_ids_raw is not None:
            if not isinstance(sender_comp_ids_raw, list):
                raise DeploymentError("'sender_comp_ids' must be a list of strings")
            parsed_sender_ids: list[str] = []
            for idx, sender in enumerate(sender_comp_ids_raw):
                if not isinstance(sender, str) or not sender.strip():
                    raise DeploymentError(
                        f"'sender_comp_ids[{idx}]' must be a non-empty string"
                    )
                parsed_sender_ids.append(sender.strip())
            if len(parsed_sender_ids) != bot_count:
                raise DeploymentError(
                    f"'sender_comp_ids' length ({len(parsed_sender_ids)}) must match "
                    f"{count_key} ({bot_count})"
                )
            sender_comp_ids = parsed_sender_ids

        for index in range(1, bot_count + 1):
            sender_comp_id = (
                sender_comp_ids[index - 1]
                if sender_comp_ids is not None
                else f"{service_key}{index}"
            )
            rendered = cfg_text_template
            rendered = rendered.replace("<sender_comp_id>", sender_comp_id)
            rendered = rendered.replace("<target_comp_id>", server_name)
            rendered = rendered.replace("<gateway_host>", gateway_host)
            rendered = rendered.replace("<gateway_port>", str(gateway_port))
            rendered = rendered.replace("<store_path>", str(store_dir))
            rendered = rendered.replace("<log_path>", str(log_dir))
            cfg_path = install_dir / f"{cfg_prefix}_{index}.cfg"
            cfg_path.write_text(rendered, encoding="utf-8")
            simulation_cfg_files.append(str(cfg_path))

    service_template_text = service_template_path.read_text(encoding="utf-8")
    service_text = service_template_text
    service_text = service_text.replace(service_name, instance_name)

    service_dest = SYSTEMD_DIR / f"{instance_name}.service"
    service_dest.write_text(service_text, encoding="utf-8")

    _run_cmd("systemctl", "daemon-reload")
    _run_cmd("systemctl", "enable", f"{instance_name}.service")
    _run_cmd("systemctl", "restart", f"{instance_name}.service")

    return {
        "service_name": service_name,
        "server_name": server_name,
        "instance_name": instance_name,
        "install_dir": str(install_dir),
        "executable": str(executable_dest),
        "config_file": str(config_dest),
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
    service_key = service_type

    _validate_server_name(server_name)

    grouped_service = service_type in {"mm", "nt", "it"}
    if grouped_service:
        pattern = f"{service_type}-*-{server_name}.service"
        matching_service_paths = [p for p in SYSTEMD_DIR.glob(pattern) if p.is_file()]
        legacy_service_path = SYSTEMD_DIR / f"{service_type}-{server_name}.service"
        if legacy_service_path.is_file():
            matching_service_paths.append(legacy_service_path)
        seen_paths: set[str] = set()
        deduped_service_paths: list[Path] = []
        for path in matching_service_paths:
            path_str = str(path)
            if path_str in seen_paths:
                continue
            seen_paths.add(path_str)
            deduped_service_paths.append(path)
        matching_service_paths = deduped_service_paths
    else:
        matching_service_paths = [SYSTEMD_DIR / f"{service_type}-{server_name}.service"]

    removed_instances: list[str] = []
    for service_path in matching_service_paths:
        if not service_path.is_file():
            continue
        service_unit = service_path.name
        instance_name = service_unit[: -len(".service")]
        install_dir = INSTALL_BASE_DIR / instance_name
        LOGGER.info("Stopping and disabling service unit: %s", service_unit)
        _run_cmd("systemctl", "stop", service_unit)
        _run_cmd("systemctl", "disable", service_unit)
        LOGGER.info("Removing unit file: %s", service_path)
        service_path.unlink()
        if install_dir.exists():
            LOGGER.info("Removing install directory: %s", install_dir)
            shutil.rmtree(install_dir)
        removed_instances.append(instance_name)

    LOGGER.info("Reloading systemd daemon after removal")
    _run_cmd("systemctl", "daemon-reload")

    default_instance_name = f"{service_type}-{server_name}"
    result = {
        "service_type": service_type,
        "service_key": service_key,
        "server_name": server_name,
        "instance_name": default_instance_name,
        "unit_file": str(SYSTEMD_DIR / f"{default_instance_name}.service"),
        "install_dir": str(INSTALL_BASE_DIR / default_instance_name),
        "removed_unit_file": len(removed_instances) > 0,
        "removed_install_dir": len(removed_instances) > 0,
        "removed_instances": removed_instances,
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
        if self.path == "/health":
            self._write_json(200, {"status": "ok"})
            return
        self._write_json(404, {"error": "Not found"})

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/deploy":
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

        try:
            result = deploy_service(
                service_name=service_name.strip(),
                server_name=server_name.strip(),
                params=config_params,
            )
        except DeploymentError as exc:
            self._write_json(400, {"error": str(exc)})
            return
        except PermissionError as exc:
            self._write_json(500, {"error": f"Permission denied: {exc}"})
            return
        except OSError as exc:
            self._write_json(500, {"error": f"OS error: {exc}"})
            return

        self._write_json(200, {"status": "deployed", "result": result})

    def log_message(self, fmt: str, *args: Any) -> None:
        return


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Deployment REST server")
    parser.add_argument("--host", default="0.0.0.0", help="Bind host")
    parser.add_argument("--port", type=int, default=8080, help="Bind port")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    httpd = ThreadingHTTPServer((args.host, args.port), DeployRequestHandler)
    print(f"Deployment server listening on http://{args.host}:{args.port}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
