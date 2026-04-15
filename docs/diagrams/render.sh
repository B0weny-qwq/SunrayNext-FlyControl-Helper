#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
OUT_DIR="${SCRIPT_DIR}/rendered"

BACKGROUND="${1:-transparent}"

usage() {
  cat <<'EOF'
Usage:
  ./render.sh [transparent|white]

Description:
  Render all PlantUML files in docs/diagrams/src into SVG files in
  docs/diagrams/rendered.

Options:
  transparent   Render SVGs with transparent background (default)
  white         Render SVGs with white background

Notes:
  Existing SVG files with the same names are overwritten.
EOF
}

case "${BACKGROUND}" in
  transparent)
    ;;
  white)
    ;;
  -h|--help|help)
    usage
    exit 0
    ;;
  *)
    echo "Unsupported background mode: ${BACKGROUND}" >&2
    usage >&2
    exit 1
    ;;
esac

mkdir -p "${OUT_DIR}"

DIAGRAMS=("${SRC_DIR}"/*.puml)

if [ ! -e "${DIAGRAMS[0]}" ]; then
  echo "No .puml files found in ${SRC_DIR}" >&2
  exit 1
fi

CMD=(
  plantuml
  --check-before-run
  --stop-on-error
  --svg
  --output-dir "${OUT_DIR}"
)

if [ "${BACKGROUND}" = "white" ]; then
  CMD+=(-d WHITE_BG=1)
fi

CMD+=("${DIAGRAMS[@]}")

"${CMD[@]}"

python3 - <<'PY' "${BACKGROUND}" "${OUT_DIR}"
from pathlib import Path
import re
import sys

background = sys.argv[1]
out_dir = Path(sys.argv[2])

for svg_file in sorted(out_dir.glob("*.svg")):
    text = svg_file.read_text(encoding="utf-8")

    # Normalize any previously injected explicit background layer so reruns stay idempotent.
    text = re.sub(r'<rect id="codex-svg-bg"[^>]*/>', '', text, count=1)

    def replace_style(match):
        style = match.group(1)
        style = re.sub(r'background:\s*#FFFFFF;?', '', style)
        style = re.sub(r';{2,}', ';', style).strip()
        if background == "white":
            style = f"background:#FFFFFF;{style}" if style else "background:#FFFFFF;"
        style = style.strip()
        return f'style="{style}"'

    if 'style="' in text:
        text = re.sub(r'style="([^"]*)"', replace_style, text, count=1)
    elif background == "white":
        text = text.replace("<svg ", '<svg style="background:#FFFFFF;" ', 1)

    # Some Markdown/SVG renderers ignore the root svg background style.
    # Insert an explicit white rect as the first drawable layer for white mode.
    if background == "white":
        bg_rect = '<rect id="codex-svg-bg" width="100%" height="100%" fill="#FFFFFF"/>'
        if "<defs/>" in text:
            text = text.replace("<defs/>", f"<defs/>{bg_rect}", 1)
        elif "<defs>" in text:
            text = re.sub(r'(<defs>.*?</defs>)', rf'\1{bg_rect}', text, count=1, flags=re.S)
        else:
            text = text.replace(">", f">{bg_rect}", 1)

    svg_file.write_text(text, encoding="utf-8")
PY

echo "Rendered ${#DIAGRAMS[@]} diagram(s) to ${OUT_DIR} with ${BACKGROUND} background."
