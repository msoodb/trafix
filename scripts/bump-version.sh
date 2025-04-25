#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
set -e

VERSION_FILE="VERSION"

# Ensure a version type argument is passed (patch, minor, or major)
if [ -z "$1" ]; then
  echo "Usage: $0 {patch|minor|major}"
  exit 1
fi

# Check if VERSION file exists
if [ ! -f "$VERSION_FILE" ]; then
  echo "VERSION file not found!"
  exit 1
fi

# Read the old version
OLD_VERSION=$(cat "$VERSION_FILE")
IFS='.' read -ra PARTS <<< "$OLD_VERSION"

# Determine which part to bump
case "$1" in
  patch)
    LAST_INDEX=$((${#PARTS[@]} - 1))
    PARTS[$LAST_INDEX]=$((PARTS[$LAST_INDEX] + 1))
    ;;
  minor)
    if [ ${#PARTS[@]} -gt 1 ]; then
      PARTS[1]=$((PARTS[1] + 1))
      PARTS[2]=0
    else
      PARTS[1]=1
      PARTS[2]=0
    fi
    ;;
  major)
    PARTS[0]=$((PARTS[0] + 1))
    PARTS[1]=0
    PARTS[2]=0
    ;;
  *)
    echo "Invalid argument: $1"
    echo "Usage: $0 {patch|minor|major}"
    exit 1
    ;;
esac

# Reconstruct the new version
NEW_VERSION=$(IFS='.'; echo "${PARTS[*]}")

# Update VERSION file with the new version
echo "$NEW_VERSION" > "$VERSION_FILE"
echo "Bumped version: $OLD_VERSION → $NEW_VERSION"

# Optionally update the changelog in trafix.spec
if grep -q '%changelog' trafix.spec; then
    TODAY=$(LC_ALL=C date +"%a %b %d %Y")
    sed -i "0,/%changelog/!b;//a\\
* $TODAY Masoud Bolhassani <masoud.bolhassani@gmail.com> - $NEW_VERSION-1\\
- Bump version to $NEW_VERSION\\
" trafix.spec
    echo "Updated changelog in trafix.spec"
fi

# Commit the version bump
git add VERSION trafix.spec
git commit -m "Bump version to $NEW_VERSION"
