#!/usr/bin/env bash
# ============================================================================
# Sync version: Reads VERSION file and updates version string in code
# ============================================================================

VERSION_FILE="VERSION"
CODE_FILE="src/editor_app.cpp"

if [ ! -f "$VERSION_FILE" ]; then
    echo "❌ VERSION file not found"
    exit 1
fi

version=$(cat "$VERSION_FILE" | tr -d '\r\n')
echo "📌 Version from VERSION file: $version"

if grep -q "return \"pCode Editor version" "$CODE_FILE"; then
    oldVersion=$(grep -o 'return "pCode Editor version [^"]*"' "$CODE_FILE" | sed 's/return "pCode Editor version //;s/"//')
    echo "📌 Current version in code: $oldVersion"
    
    if [ "$oldVersion" = "$version" ]; then
        echo "✅ Version already in sync"
        exit 0
    fi
    
    # Update version in code
    sed -i "s/return \"pCode Editor version [^\"]*\"/return \"pCode Editor version $version\"/" "$CODE_FILE"
    echo "✅ Updated version to $version in $CODE_FILE"
else
    echo "❌ Could not find version pattern in $CODE_FILE"
    exit 1
fi

exit 0