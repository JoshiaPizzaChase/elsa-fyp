#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: deploy_deployment_server.sh

Deploy deployment server by:
  - copying template/deployment_server.service to /etc/systemd/system/deployment-server.service
  - copying deployment_server.py to /usr/local/bin/deployment-server/deployment_server.py
  - copying the whole template directory to /usr/local/bin/deployment-server/template
  - copying the whole build directory to /usr/local/bin/deployment-server/build
  - enabling and starting deployment-server.service
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -ne 0 ]]; then
  usage
  exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
  echo "Error: this script must be run as root (use sudo)." >&2
  exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

SERVICE_TEMPLATE="${REPO_ROOT}/template/deployment_server.service"
TEMPLATE_SRC_DIR="${REPO_ROOT}/template"
BUILD_SRC_DIR="${REPO_ROOT}/build"
DEPLOYMENT_SERVER_SRC="${REPO_ROOT}/deployment_server.py"

if [[ ! -f "${SERVICE_TEMPLATE}" ]]; then
  echo "Error: missing service template: ${SERVICE_TEMPLATE}" >&2
  exit 1
fi

if [[ ! -d "${TEMPLATE_SRC_DIR}" ]]; then
  echo "Error: missing template directory: ${TEMPLATE_SRC_DIR}" >&2
  exit 1
fi
if [[ ! -d "${BUILD_SRC_DIR}" ]]; then
  echo "Error: missing build directory: ${BUILD_SRC_DIR}" >&2
  exit 1
fi
if [[ ! -f "${DEPLOYMENT_SERVER_SRC}" ]]; then
  echo "Error: missing deployment server source: ${DEPLOYMENT_SERVER_SRC}" >&2
  exit 1
fi

INSTALL_DIR="/usr/local/bin/deployment-server"
SYSTEMD_DIR="/etc/systemd/system"
SERVICE_DEST="${SYSTEMD_DIR}/deployment-server.service"
TEMPLATE_DEST_DIR="${INSTALL_DIR}/template"
BUILD_DEST_DIR="${INSTALL_DIR}/build"
DEPLOYMENT_SERVER_DEST="${INSTALL_DIR}/deployment_server.py"
LOG_FILE="/var/log/deployment-server.log"

mkdir -p "${INSTALL_DIR}"
rm -f "${SERVICE_DEST}"
install -m 0644 "${SERVICE_TEMPLATE}" "${SERVICE_DEST}"
rm -f "${DEPLOYMENT_SERVER_DEST}"
install -m 0755 "${DEPLOYMENT_SERVER_SRC}" "${DEPLOYMENT_SERVER_DEST}"

rm -rf "${TEMPLATE_DEST_DIR}"
cp -a "${TEMPLATE_SRC_DIR}" "${TEMPLATE_DEST_DIR}"
rm -rf "${BUILD_DEST_DIR}"
cp -a "${BUILD_SRC_DIR}" "${BUILD_DEST_DIR}"

touch "${LOG_FILE}"
chown root:root "${LOG_FILE}"
chmod 0644 "${LOG_FILE}"
if command -v restorecon >/dev/null 2>&1; then
  restorecon -v "${LOG_FILE}" || true
fi

systemctl daemon-reload
systemctl enable deployment-server.service
systemctl restart deployment-server.service

echo "Deployment completed."
echo "Service file: ${SERVICE_DEST}"
echo "Deployment server script: ${DEPLOYMENT_SERVER_DEST}"
echo "Template directory: ${TEMPLATE_DEST_DIR}"
echo "Build directory: ${BUILD_DEST_DIR}"
echo "Log file: ${LOG_FILE}"

systemctl status deployment-server.service
