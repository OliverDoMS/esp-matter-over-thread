#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$project_dir/versions.env"
: "${IDF_PATH:?Use the documented ESP-Matter Docker image or export ESP-IDF first}"
: "${ESP_MATTER_PATH:?Export ESP_MATTER_PATH first}"

# These upstream activation scripts are not guaranteed to be nounset-safe.
set +u
source "$IDF_PATH/export.sh"
set -u
idf.py --version | grep -F "ESP-IDF $ESP_IDF_VERSION"
sdk_git() { git -c "safe.directory=$ESP_MATTER_PATH" -C "$ESP_MATTER_PATH" "$@"; }
if [[ "$(sdk_git rev-parse HEAD)" != "$ESP_MATTER_COMMIT" ]]; then
    sdk_git fetch --depth 1 origin "$ESP_MATTER_COMMIT"
    sdk_git checkout --detach "$ESP_MATTER_COMMIT"
    sdk_git submodule update --init --depth 1
    (
        cd "$ESP_MATTER_PATH/connectedhomeip/connectedhomeip"
        python3 scripts/checkout_submodules.py --platform esp32 linux --shallow
    )
fi
set +u
source "$ESP_MATTER_PATH/export.sh"
set -u

cd "$project_dir"
python3 -m unittest discover -s tests -v
idf.py -DIDF_TARGET=esp32h2 build
python3 scripts/package_firmware.py --build-dir build --output-dir dist
