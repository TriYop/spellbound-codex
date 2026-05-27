#!/usr/bin/env bash
# Install MasterTweak CLI and GUI.
# Usage:
#   ./install.sh           — install to user directories (~/.local/bin)
#   ./install.sh --system  — install system-wide to /usr/local/bin (requires sudo)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEM=0

for arg in "$@"; do
    case "$arg" in
        --system) SYSTEM=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ $SYSTEM -eq 1 ]]; then
    BIN_DIR="/usr/local/bin"
    DATA_DIR="/usr/local/share/mastertweak"
else
    BIN_DIR="${HOME}/.local/bin"
    DATA_DIR="${HOME}/.local/share/mastertweak"
fi

echo "Installing MasterTweak..."

mkdir -p "${BIN_DIR}"

# CLI
cp    "${SCRIPT_DIR}/bin/mastertweak" "${BIN_DIR}/"
chmod 755 "${BIN_DIR}/mastertweak"
echo "  CLI  → ${BIN_DIR}/mastertweak"

# GUI (optional — only if built)
if [[ -f "${SCRIPT_DIR}/bin/MasterTweak" ]]; then
    cp    "${SCRIPT_DIR}/bin/MasterTweak" "${BIN_DIR}/"
    chmod 755 "${BIN_DIR}/MasterTweak"
    echo "  GUI  → ${BIN_DIR}/MasterTweak"
fi

# Bundled presets (optional)
if [[ -d "${SCRIPT_DIR}/presets" ]]; then
    mkdir -p "${DATA_DIR}/presets"
    cp -r "${SCRIPT_DIR}/presets/"*.xml "${DATA_DIR}/presets/" 2>/dev/null || true
    echo "  Presets → ${DATA_DIR}/presets/"
fi

echo "Done."
