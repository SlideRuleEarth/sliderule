#!/bin/bash

set -e

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly CERTBOT_VERSION=2.6.0
readonly VENV="certbot/venv"
readonly PYTHON="python3.8"
readonly CERTBOT_ZIP_FILE="certbot-${CERTBOT_VERSION}.zip"
readonly CERTBOT_SITE_PACKAGES=${VENV}/lib/${PYTHON}/site-packages

cd "${SCRIPT_DIR}"

${PYTHON} -m venv "${VENV}"
source "${VENV}/bin/activate"

pip3 install -r requirements.txt

pushd ${CERTBOT_SITE_PACKAGES}
    zip -r -q ${SCRIPT_DIR}/certbot/${CERTBOT_ZIP_FILE} . -x "/*__pycache__/*"
popd

zip -g "certbot/${CERTBOT_ZIP_FILE}" main.py