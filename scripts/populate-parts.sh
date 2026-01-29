#!/bin/bash
set -euo pipefail
BASEDIR=$(cd "$(dirname "$0")" && pwd)
PARTS_DIR="$BASEDIR/parts"
mkdir -p "$PARTS_DIR"
for socdir in "$BASEDIR"/*/; do
    soc=$(basename "$socdir")
    vcfile="$socdir/vc-config"
    if [ -f "$vcfile" ]; then
        partfile="$PARTS_DIR/$soc"
        # Strip the shebang if present (first line starting with #!)
        awk 'NR==1 && /^#!/{next} {print}' "$vcfile" > "$partfile"
        chmod 644 "$partfile"
        echo "Wrote part: $partfile"
    fi
done

echo "Populate parts complete. Review files in $PARTS_DIR."
