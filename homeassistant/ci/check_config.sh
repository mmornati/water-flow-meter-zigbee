#!/usr/bin/env bash
# Validates homeassistant/water_meter.yaml with Home Assistant's own config checker,
# inside the official container. Used by CI; runs locally too (needs Docker).
#   HA_IMAGE=ghcr.io/home-assistant/home-assistant:2026.10.4 homeassistant/ci/check_config.sh
set -euo pipefail

HA_IMAGE="${HA_IMAGE:-ghcr.io/home-assistant/home-assistant:stable}"
here="$(cd "$(dirname "$0")" && pwd)"
config="$(mktemp -d)"
trap 'rm -rf "$config"' EXIT

mkdir -p "$config/packages"
cp "$here/configuration.yaml" "$config/configuration.yaml"
cp "$here/../water_meter.yaml" "$config/packages/water_meter.yaml"

echo "Checking with $HA_IMAGE"
output="$(docker run --rm -v "$config:/config" "$HA_IMAGE" \
  python -m homeassistant --script check_config --config /config 2>&1)"
echo "$output"

# check_config can log invalid config and still exit 0, so look at what it printed.
if grep -qE "Invalid config|Failed config|ERROR" <<<"$output"; then
  echo "::error::Home Assistant rejected the configuration"
  exit 1
fi
echo "Home Assistant configuration is valid"
