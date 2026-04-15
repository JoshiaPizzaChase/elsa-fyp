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

    unknown_keys = [k for k in params.keys() if k not in template_data]
    if unknown_keys:
        raise DeploymentError(
            f"Unknown config fields for service '{service_name}': {', '.join(sorted(unknown_keys))}"
        )

    final_config = dict(template_data)
    for key, value in params.items():
        final_config[key] = _convert_value_to_template_type(template_data[key], value)

    instance_name = f"{service_name}-{server_name}"
    install_dir = INSTALL_BASE_DIR / instance_name
    install_dir.mkdir(parents=True, exist_ok=True)

    executable_dest = install_dir / spec["executable"]
    shutil.copy2(executable_src, executable_dest)
    executable_dest.chmod(0o755)

    config_dest = install_dir / spec["template_toml"]
    config_dest.write_text(_render_toml(final_config), encoding="utf-8")

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
