#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$project_dir/versions.env"
: "${IDF_PATH:?Use the documented ESP-Matter Docker image or export ESP-IDF first}"
: "${ESP_MATTER_PATH:?Export ESP_MATTER_PATH first}"

# Official images use shallow Git checkouts without release tags. Restore only
# the required tag and verify the commit before relying on IDF's version string.
idf_git() { git -c "safe.directory=$IDF_PATH" -C "$IDF_PATH" "$@"; }
idf_git fetch --depth 1 origin "refs/tags/$ESP_IDF_VERSION:refs/tags/$ESP_IDF_VERSION"
if [[ "$(idf_git rev-parse HEAD)" != "$(idf_git rev-parse "$ESP_IDF_VERSION^{commit}")" ]]; then
    echo "ESP-IDF checkout does not match the required $ESP_IDF_VERSION release" >&2
    exit 1
fi

# These upstream activation scripts are not guaranteed to be nounset-safe.
set +u
source "$IDF_PATH/export.sh"
set -u
idf.py --version
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
# The tests check the release rules, so they must not see the diagnostic switch.
env -u DNIOT_DIAGNOSTIC python3 -m unittest discover -s tests -v

# Usage: scripts/build.sh [variant ...]   (default: every directory in variants/)
# DNIOT_DIAGNOSTIC=1 builds the Light-sleep-off diagnostic images instead.
if [[ $# -gt 0 ]]; then
    variants=("$@")
else
    mapfile -t variants < <(find variants -mindepth 2 -maxdepth 2 -name variant.env -printf '%h\n' | xargs -n1 basename | sort)
fi
suffix=""
diag=OFF
diag_flag=()
if [[ "${DNIOT_DIAGNOSTIC:-}" == "1" ]]; then
    suffix="-diag"
    diag=ON
    diag_flag=(--diagnostic)
fi
for variant in "${variants[@]}"; do
    echo "=== Building variant $variant$suffix"
    build_dir="build/$variant$suffix"
    idf.py -B "$build_dir" -DIDF_TARGET=esp32h2 -DDNIOT_VARIANT="$variant" -DDNIOT_DIAGNOSTIC="$diag" build
    python3 "$ESP_MATTER_PATH/connectedhomeip/connectedhomeip/src/app/ota_image_tool.py" show "$build_dir/esp_matter_base-ota.bin"
    python3 scripts/package_firmware.py --variant "$variant" "${diag_flag[@]}" \
        --build-dir "$build_dir" --output-dir "dist/$variant$suffix"
done
