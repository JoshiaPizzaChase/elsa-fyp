#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: deploy_backend.sh <port> [backend_executable_path]

Deploy backend systemd service by:
  - copying template/backend.service to /etc/systemd/system/backend.service
  - copying backend executable to /usr/local/bin/backend/backend
  - rendering template/backend.toml with the given port to /usr/local/bin/backend/backend.toml
  - enabling and starting backend.service
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
  echo "Error: this script must be run as root (use sudo)." >&2
  exit 1
fi

PORT="$1"
if ! [[ "${PORT}" =~ ^[0-9]+$ ]] || (( PORT < 1024 || PORT > 9999 )); then
  echo "Error: backend port must be an integer in range [1024, 9999]." >&2
  exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

SERVICE_TEMPLATE="${REPO_ROOT}/template/backend.service"
TOML_TEMPLATE="${REPO_ROOT}/template/backend.toml"

if [[ ! -f "${SERVICE_TEMPLATE}" ]]; then
  echo "Error: missing service template: ${SERVICE_TEMPLATE}" >&2
  exit 1
fi
if [[ ! -f "${TOML_TEMPLATE}" ]]; then
  echo "Error: missing TOML template: ${TOML_TEMPLATE}" >&2
  exit 1
fi

DEFAULT_EXECUTABLE="${REPO_ROOT}/build/services/backend_service/backend_service"
EXECUTABLE_SRC="${2:-${DEFAULT_EXECUTABLE}}"
if [[ ! -x "${EXECUTABLE_SRC}" ]]; then
  echo "Error: backend executable not found or not executable: ${EXECUTABLE_SRC}" >&2
  echo "Hint: pass the executable path as the second argument if needed." >&2
  exit 1
fi

INSTALL_DIR="/usr/local/bin/backend"
SYSTEMD_DIR="/etc/systemd/system"
SERVICE_DEST="${SYSTEMD_DIR}/backend.service"
EXECUTABLE_DEST="${INSTALL_DIR}/backend"
CONFIG_DEST="${INSTALL_DIR}/backend.toml"

mkdir -p "${INSTALL_DIR}"
install -m 0644 "${SERVICE_TEMPLATE}" "${SERVICE_DEST}"
install -m 0755 "${EXECUTABLE_SRC}" "${EXECUTABLE_DEST}"

sed "s#<port placeholder>#${PORT}#g" "${TOML_TEMPLATE}" > "${CONFIG_DEST}"
chmod 0644 "${CONFIG_DEST}"

systemctl daemon-reload
systemctl enable backend.service
systemctl start backend.service

echo "Deployment completed."
echo "Service file: ${SERVICE_DEST}"
echo "Executable: ${EXECUTABLE_DEST}"
echo "Config: ${CONFIG_DEST}"
