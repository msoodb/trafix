#!/bin/bash
set -e

VERSION_FILE="VERSION"

if [ ! -f "$VERSION_FILE" ]; then
  echo "VERSION file not found!"
  exit 1
fi

OLD_VERSION=$(cat "$VERSION_FILE")
IFS='.' read -ra PARTS <<< "$OLD_VERSION"

# Increment last number
LAST_INDEX=$((${#PARTS[@]} - 1))
PARTS[$LAST_INDEX]=$((PARTS[$LAST_INDEX] + 1))

NEW_VERSION=$(IFS='.'; echo "${PARTS[*]}")
echo "$NEW_VERSION" > "$VERSION_FILE"

echo "Bumped version: $OLD_VERSION → $NEW_VERSION"

# Optionally update the changelog (optional)
if grep -q '%changelog' trafix.spec; then
    TODAY=$(LC_ALL=C date +"%a %b %d %Y")
    sed -i "0,/%changelog/!b;//a\\
* $TODAY Masoud Bolhassani <masoud.bolhassani@gmail.com> - $NEW_VERSION-1\\
- Bump version to $NEW_VERSION\\
" trafix.spec
    echo "Updated changelog in trafix.spec"
fi

git add VERSION trafix.spec
git commit -m "Bump version to $NEW_VERSION"
