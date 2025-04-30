#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani.


# Exit on error
set -e

# Usage check
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <destination-directory>"
    exit 1
fi

DEST_DIR="$1"
VERSION_FILE="VERSION"
SPEC_SOURCE="trafix.spec"
SPEC_DEST="${DEST_DIR}/trafix.spec"

# Check files
if [[ ! -f "$VERSION_FILE" ]]; then
    echo "Error: VERSION file not found."
    exit 1
fi

if [[ ! -f "$SPEC_SOURCE" ]]; then
    echo "Error: $SPEC_SOURCE not found."
    exit 1
fi

# Read version
VERSION=$(grep -v '^#' "$VERSION_FILE" | head -n 1)

# Create destination directory if needed
mkdir -p "$DEST_DIR"

# Replace placeholder and write to destination
sed "s/__VERSION__/$VERSION/g" "$SPEC_SOURCE" > "$SPEC_DEST"

echo "Copied and updated spec to $SPEC_DEST with version $VERSION"
