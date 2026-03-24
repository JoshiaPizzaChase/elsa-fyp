#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: deploy_deployment_server.sh

Deploy deployment server by:
  - copying template/deployment_server.service to /etc/systemd/system/deployment-server.service
  - copying the whole template directory to /usr/local/bin/deployment-server/template
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

if [[ ! -f "${SERVICE_TEMPLATE}" ]]; then
  echo "Error: missing service template: ${SERVICE_TEMPLATE}" >&2
  exit 1
fi

if [[ ! -d "${TEMPLATE_SRC_DIR}" ]]; then
  echo "Error: missing template directory: ${TEMPLATE_SRC_DIR}" >&2
  exit 1
fi

INSTALL_DIR="/usr/local/bin/deployment-server"
SYSTEMD_DIR="/etc/systemd/system"
SERVICE_DEST="${SYSTEMD_DIR}/deployment-server.service"
TEMPLATE_DEST_DIR="${INSTALL_DIR}/template"

mkdir -p "${INSTALL_DIR}"
rm -f "${SERVICE_DEST}"
install -m 0644 "${SERVICE_TEMPLATE}" "${SERVICE_DEST}"

rm -rf "${TEMPLATE_DEST_DIR}"
cp -a "${TEMPLATE_SRC_DIR}" "${TEMPLATE_DEST_DIR}"

systemctl daemon-reload
systemctl enable deployment-server.service
systemctl start deployment-server.service

echo "Deployment completed."
echo "Service file: ${SERVICE_DEST}"
echo "Template directory: ${TEMPLATE_DEST_DIR}"

systemctl status deployment-server
