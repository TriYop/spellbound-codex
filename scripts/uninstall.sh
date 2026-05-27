#!/usr/bin/env bash
# Remove MasterTweak from all known install locations.
set -euo pipefail

removed=0

remove() {
    local path="$1"
    if [[ -e "$path" ]]; then
        rm -rf "$path"
        echo "  Removed: $path"
        removed=1
    fi
}

echo "Uninstalling MasterTweak..."

# User locations
remove "${HOME}/.local/bin/mastertweak"
remove "${HOME}/.local/bin/MasterTweak"
remove "${HOME}/.local/share/mastertweak"

# System locations (silently skip if no permission)
if [[ $EUID -eq 0 ]]; then
    remove "/usr/local/bin/mastertweak"
    remove "/usr/local/bin/MasterTweak"
    remove "/usr/local/share/mastertweak"
else
    for path in "/usr/local/bin/mastertweak" \
                "/usr/local/bin/MasterTweak" \
                "/usr/local/share/mastertweak"; do
        if [[ -e "$path" ]]; then
            echo "  Skipping $path (re-run with sudo to remove)"
        fi
    done
fi

if [[ $removed -eq 0 ]]; then
    echo "  Nothing to remove."
else
    echo "Done."
fi
