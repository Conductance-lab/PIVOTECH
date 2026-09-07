#!/usr/bin/env python3
"""
Generate a 14x14 UTF-8 Chinese font table for the SSD1306 renderer.

The script collects non-ASCII characters from active string literals in the UI
source files, merges them with any manual additions in `font12cn_extra.txt`,
reads glyphs directly from the original `wenquanyi_11pt.bdf` bitmap font, and
writes `font12cn.c`.
"""

from __future__ import annotations

from dataclasses import dataclass
import re
from pathlib import Path
from typing import Iterable


FONT_WIDTH = 14
FONT_HEIGHT = 14
ASCII_WIDTH = 7
BYTES_PER_ROW = (FONT_WIDTH + 7) // 8

# Crop the original 15px cell down to the visible 14x14 bitmap area.
CELL_MIN_Y = -1
CELL_MAX_Y = CELL_MIN_Y + FONT_HEIGHT - 1

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "Fonts"
FONT_PATH = FONT_DIR / "wenquanyi_10pt.bdf"
OUTPUT_PATH = FONT_DIR / "font12cn.c"
EXTRA_PATH = FONT_DIR / "font12cn_extra.txt"

SOURCE_PATTERNS = (
    "main.cpp",
    "menu/**/*.cpp",
    "menu/**/*.hpp",
    "read/**/*.cpp",
    "read/**/*.hpp",
)

STRING_RE = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')


@dataclass(frozen=True)
class BdfGlyph:
    encoding: int
    dwidth: int
    bbx_width: int
    bbx_height: int
    bbx_x: int
    bbx_y: int
    bitmap_rows: tuple[str, ...]


def iter_source_files() -> list[Path]:
    files: dict[Path, None] = {}
    for pattern in SOURCE_PATTERNS:
        for path in ROOT.glob(pattern):
            if path.is_file():
                files[path.resolve()] = None
    return sorted(files)


def strip_comments(text: str) -> str:
    result: list[str] = []
    i = 0
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                i += 2
                continue
            if ch == '"':
                state = "string"
            elif ch == "'":
                state = "char"
            result.append(ch)
            i += 1
            continue

        if state == "line_comment":
            if ch == "\n":
                result.append("\n")
                state = "code"
            i += 1
            continue

        if state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
            else:
                if ch == "\n":
                    result.append("\n")
                i += 1
            continue

        if state == "string":
            result.append(ch)
            if ch == "\\" and i + 1 < len(text):
                result.append(text[i + 1])
                i += 2
                continue
            if ch == '"':
                state = "code"
            i += 1
            continue

        if state == "char":
            result.append(ch)
            if ch == "\\" and i + 1 < len(text):
                result.append(text[i + 1])
                i += 2
                continue
            if ch == "'":
                state = "code"
            i += 1
            continue

    return "".join(result)


def decode_c_string(src: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(src):
        ch = src[i]
        if ch != "\\" or i + 1 >= len(src):
            out.append(ch)
            i += 1
            continue

        nxt = src[i + 1]
        simple_map = {
            "n": "\n",
            "r": "\r",
            "t": "\t",
            "\\": "\\",
            '"': '"',
            "'": "'",
            "0": "\0",
        }
        if nxt in simple_map:
            out.append(simple_map[nxt])
            i += 2
            continue

        if nxt == "x":
            j = i + 2
            hex_digits: list[str] = []
            while j < len(src) and src[j] in "0123456789abcdefABCDEF":
                hex_digits.append(src[j])
                j += 1
            if hex_digits:
                out.append(chr(int("".join(hex_digits), 16)))
                i = j
                continue

        if nxt in "01234567":
            j = i + 1
            oct_digits: list[str] = []
            while j < len(src) and len(oct_digits) < 3 and src[j] in "01234567":
                oct_digits.append(src[j])
                j += 1
            out.append(chr(int("".join(oct_digits), 8)))
            i = j
            continue

        out.append(nxt)
        i += 2

    return "".join(out)


def iter_string_literals(path: Path) -> Iterable[str]:
    stripped = strip_comments(path.read_text(encoding="utf-8"))
    for match in STRING_RE.finditer(stripped):
        yield decode_c_string(match.group(1))


def load_extra_chars() -> str:
    if not EXTRA_PATH.exists():
        return ""

    chars: list[str] = []
    for line in EXTRA_PATH.read_text(encoding="utf-8").splitlines():
        if line.lstrip().startswith("#"):
            continue
        chars.append(line)
    return "\n".join(chars)


def collect_charset() -> list[str]:
    chars: set[str] = set()

    for path in iter_source_files():
        for literal in iter_string_literals(path):
            for ch in literal:
                if ord(ch) > 127 and not ch.isspace():
                    chars.add(ch)

    for ch in load_extra_chars():
        if ord(ch) > 127 and not ch.isspace():
            chars.add(ch)

    return sorted(chars)


def build_glyph(data: dict[str, object]) -> BdfGlyph:
    encoding = data.get("encoding")
    dwidth = data.get("dwidth")
    bbx = data.get("bbx")
    bitmap_rows = data.get("bitmap_rows")

    if not isinstance(encoding, int):
        raise ValueError("Missing ENCODING in BDF glyph")
    if not isinstance(dwidth, int):
        raise ValueError(f"Missing DWIDTH for glyph {encoding}")
    if not isinstance(bbx, tuple) or len(bbx) != 4:
        raise ValueError(f"Missing BBX for glyph {encoding}")
    if not isinstance(bitmap_rows, list):
        raise ValueError(f"Missing BITMAP for glyph {encoding}")

    bbx_width, bbx_height, bbx_x, bbx_y = bbx
    if len(bitmap_rows) != bbx_height:
        raise ValueError(
            f"Bitmap row count mismatch for glyph {encoding}: "
            f"expected {bbx_height}, got {len(bitmap_rows)}"
        )

    return BdfGlyph(
        encoding=encoding,
        dwidth=dwidth,
        bbx_width=bbx_width,
        bbx_height=bbx_height,
        bbx_x=bbx_x,
        bbx_y=bbx_y,
        bitmap_rows=tuple(bitmap_rows),
    )


def load_bdf_glyphs(required_encodings: set[int]) -> dict[int, BdfGlyph]:
    if not FONT_PATH.exists():
        raise FileNotFoundError(f"Missing font file: {FONT_PATH}")

    glyphs: dict[int, BdfGlyph] = {}
    current: dict[str, object] | None = None
    in_bitmap = False

    for raw_line in FONT_PATH.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()

        if raw_line.startswith("STARTCHAR "):
            current = {"bitmap_rows": []}
            in_bitmap = False
            continue

        if current is None:
            continue

        if in_bitmap:
            if line == "ENDCHAR":
                glyph = build_glyph(current)
                if glyph.encoding in required_encodings:
                    glyphs[glyph.encoding] = glyph
                current = None
                in_bitmap = False
            else:
                bitmap_rows = current["bitmap_rows"]
                assert isinstance(bitmap_rows, list)
                bitmap_rows.append(line)
            continue

        if line.startswith("ENCODING "):
            current["encoding"] = int(line.split()[1])
            continue

        if line.startswith("DWIDTH "):
            current["dwidth"] = int(line.split()[1])
            continue

        if line.startswith("BBX "):
            _, width, height, xoff, yoff = line.split()
            current["bbx"] = (int(width), int(height), int(xoff), int(yoff))
            continue

        if line.startswith("BITMAP"):
            in_bitmap = True
            continue

        if line == "ENDCHAR":
            glyph = build_glyph(current)
            if glyph.encoding in required_encodings:
                glyphs[glyph.encoding] = glyph
            current = None

    return glyphs


def glyph_to_row_bytes(glyph: BdfGlyph) -> list[int]:
    rows: list[int] = []
    bitmap = [[0] * FONT_WIDTH for _ in range(FONT_HEIGHT)]

    for row_index, row_hex in enumerate(glyph.bitmap_rows):
        row_bits = len(row_hex) * 4
        row_value = int(row_hex, 16) if row_hex else 0
        glyph_y = glyph.bbx_y + glyph.bbx_height - 1 - row_index
        target_y = CELL_MAX_Y - glyph_y
        if not 0 <= target_y < FONT_HEIGHT:
            continue

        for bit_index in range(glyph.bbx_width):
            glyph_x = glyph.bbx_x + bit_index
            if not 0 <= glyph_x < FONT_WIDTH:
                continue

            if row_value & (1 << (row_bits - 1 - bit_index)):
                bitmap[target_y][glyph_x] = 1

    for y in range(FONT_HEIGHT):
        row = [0] * BYTES_PER_ROW
        for x in range(FONT_WIDTH):
            if bitmap[y][x]:
                row[x // 8] |= 0x80 >> (x % 8)
        rows.extend(row)

    return rows


def utf8_index_bytes(char: str) -> list[int]:
    data = char.encode("utf-8")
    if len(data) != 3:
        raise ValueError(f"Only 3-byte UTF-8 glyphs are supported: {char!r}")
    return list(data)


def format_bytes(values: Iterable[int]) -> str:
    return ", ".join(f"0x{value:02X}" for value in values)


def generate_source(chars: list[str]) -> str:
    glyphs = load_bdf_glyphs({ord(ch) for ch in chars})
    missing_chars = [ch for ch in chars if ord(ch) not in glyphs]
    if missing_chars:
        missing_preview = "".join(missing_chars[:16])
        raise KeyError(f"Missing glyphs in {FONT_PATH.name}: {missing_preview}")

    lines: list[str] = []
    lines.append('#include "fonts.h"')
    lines.append("")
    lines.append("// Generated by Fonts/generate_font12cn.py. Do not edit manually.")
    lines.append("")
    lines.append("static const CH_CN Font12CN_Table[] = {")

    for ch in chars:
        idx = utf8_index_bytes(ch)
        bitmap = glyph_to_row_bytes(glyphs[ord(ch)])
        lines.append(f"    {{{{{format_bytes(idx)}}}, {{{format_bytes(bitmap)}}}}}, // {ch}")

    lines.append("};")
    lines.append("")
    lines.append("cFONT Font12CN = {")
    lines.append("    Font12CN_Table,")
    lines.append("    sizeof(Font12CN_Table) / sizeof(Font12CN_Table[0]),")
    lines.append(f"    {ASCII_WIDTH},")
    lines.append(f"    {FONT_WIDTH},")
    lines.append(f"    {FONT_HEIGHT},")
    lines.append("};")
    lines.append("")
    lines.append("// Compatibility aliases kept for older code paths/build artifacts.")
    lines.append("cFONT Font13CN = {")
    lines.append("    Font12CN_Table,")
    lines.append("    sizeof(Font12CN_Table) / sizeof(Font12CN_Table[0]),")
    lines.append(f"    {ASCII_WIDTH},")
    lines.append(f"    {FONT_WIDTH},")
    lines.append(f"    {FONT_HEIGHT},")
    lines.append("};")
    lines.append("")
    lines.append("cFONT Font24CN = {")
    lines.append("    Font12CN_Table,")
    lines.append("    sizeof(Font12CN_Table) / sizeof(Font12CN_Table[0]),")
    lines.append(f"    {ASCII_WIDTH},")
    lines.append(f"    {FONT_WIDTH},")
    lines.append(f"    {FONT_HEIGHT},")
    lines.append("};")
    lines.append("")

    return "\n".join(lines)


def main() -> None:
    chars = collect_charset()
    source = generate_source(chars)
    OUTPUT_PATH.write_text(source, encoding="utf-8", newline="\n")
    print(f"Generated {OUTPUT_PATH} with {len(chars)} glyphs.")


if __name__ == "__main__":
    main()
