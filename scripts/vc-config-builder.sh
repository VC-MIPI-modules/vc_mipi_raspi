#!/bin/bash
# Simple assembler for vc-config variants
# Usage: ./vc-config-builder.sh [--deploy]
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
MANIFEST="$SCRIPT_DIR/vc-config.manifest"
PARTS_DIR="$SCRIPT_DIR/parts"
OUT_DIR="$SCRIPT_DIR/generated"
DEPLOY=false
if [ "${1:-}" = "--deploy" ]; then
    DEPLOY=true
fi
if [ ! -f "$MANIFEST" ]; then
    echo "Manifest $MANIFEST not found" >&2
    exit 1
fi
mkdir -p "$OUT_DIR"
while IFS= read -r line; do
    # strip comments and trim leading/trailing whitespace
    line=$(echo "$line" | sed -e 's/#.*//' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
    [ -z "$line" ] && continue
    target=${line%%:*}
    parts_str=${line#*:}
    target_dir="$SCRIPT_DIR/$target"
    gen_dir="$OUT_DIR/$target"
    mkdir -p "$gen_dir"
    out_file="$gen_dir/vc-config"
    : > "$out_file"
    for part in $parts_str; do
        part_file="$PARTS_DIR/$part"
        if [ ! -f "$part_file" ]; then
            echo "Missing part: $part_file" >&2
            exit 1
        fi
        sed '/^```/d' "$part_file" >> "$out_file"
        echo -e "\n" >> "$out_file"
    done
    chmod +x "$out_file"
    echo "Assembled $out_file"
    if [ "$DEPLOY" = true ]; then
        if [ -d "$target_dir" ]; then
            dest="$target_dir/vc-config"
            if [ -f "$dest" ]; then
                cp -a "$dest" "$dest.orig.$(date +%s)"
            fi
            cp -a "$out_file" "$dest"
            echo "Deployed to $dest"
        else
            echo "Target directory $target_dir does not exist; skipping deploy" >&2
        fi
    fi
done < "$MANIFEST"
echo "Done."