#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: deploy-frontend.sh

Deploy frontend systemd service by:
  - running npm run build in frontend-v2
  - moving frontend-v2/build to /usr/local/bin/frontend/build
  - copying frontend-v2/.env to /usr/local/bin/frontend/.env
  - copying template/frontend.service to /etc/systemd/system/frontend.service
  - enabling and starting frontend.service
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
FRONTEND_DIR="${REPO_ROOT}/frontend-v2"
SERVICE_TEMPLATE="${REPO_ROOT}/template/frontend.service"

if [[ ! -d "${FRONTEND_DIR}" ]]; then
  echo "Error: missing frontend directory: ${FRONTEND_DIR}" >&2
  exit 1
fi
if [[ ! -f "${SERVICE_TEMPLATE}" ]]; then
  echo "Error: missing service template: ${SERVICE_TEMPLATE}" >&2
  exit 1
fi
if [[ ! -f "${FRONTEND_DIR}/.env" ]]; then
  echo "Error: missing environment file: ${FRONTEND_DIR}/.env" >&2
  exit 1
fi

INSTALL_DIR="/usr/local/bin/frontend"
SYSTEMD_DIR="/etc/systemd/system"
SERVICE_DEST="${SYSTEMD_DIR}/frontend.service"
BUILD_SRC="${FRONTEND_DIR}/build"
BUILD_DEST="${INSTALL_DIR}/build"
ENV_DEST="${INSTALL_DIR}/.env"

cd "${FRONTEND_DIR}"
npm run build

mkdir -p "${INSTALL_DIR}"
rm -rf "${BUILD_DEST}"
if [[ -d "${BUILD_SRC}" ]]; then
  mv "${BUILD_SRC}" "${BUILD_DEST}"
else
  echo "Error: build output not found at ${BUILD_SRC}" >&2
  exit 1
fi
install -m 0644 "${FRONTEND_DIR}/.env" "${ENV_DEST}"
install -m 0644 "${SERVICE_TEMPLATE}" "${SERVICE_DEST}"

systemctl daemon-reload
systemctl enable frontend.service
systemctl restart frontend.service

echo "Deployment completed."
echo "Service file: ${SERVICE_DEST}"
echo "Frontend root: ${INSTALL_DIR}"
echo "Build directory: ${BUILD_DEST}"
echo "Env file: ${ENV_DEST}"

systemctl status frontend.service
