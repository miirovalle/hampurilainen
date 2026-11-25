#!/usr/bin/env python3
"""Draws the LoopDeck app icon as a 1024x1024 PNG, without any dependencies.

Xcode (through juceaide) uses this single 1024pt image to build every iOS and
macOS icon size, so the repo does not need to store a set of bitmaps.
"""

from __future__ import annotations

import argparse
import math
import struct
import zlib
from pathlib import Path

SIZE = 1024
SUPERSAMPLE = 3

BACKGROUND_TOP = (0x18, 0x1E, 0x27)
BACKGROUND_BOTTOM = (0x0A, 0x0D, 0x12)
ACCENT = (0x35, 0xD6, 0xA4)
ACCENT_ALT = (0xFF, 0xB0, 0x57)
OUTLINE = (0x2B, 0x33, 0x40)


def rounded_rect_contains(x: float, y: float, rect: tuple[float, float, float, float], radius: float) -> bool:
    left, top, right, bottom = rect

    if x < left or x > right or y < top or y > bottom:
        return False

    radius = max(0.0, min(radius, (right - left) * 0.5, (bottom - top) * 0.5))
    cx = min(max(x, left + radius), right - radius)
    cy = min(max(y, top + radius), bottom - radius)
    dx = x - cx
    dy = y - cy

    return dx * dx + dy * dy <= radius * radius


def mix(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[float, float, float]:
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def render() -> bytes:
    # The artwork: a dark card holding a looping waveform, so the icon reads
    # as "MIDI loop player" at 40 px as well as at 1024 px.
    card = (150.0, 196.0, 874.0, 828.0)
    card_radius = 92.0
    inner = (card[0] + 7, card[1] + 7, card[2] - 7, card[3] - 7)

    bar_width = 54.0
    bars = [
        (240.0, 52.0, ACCENT),
        (348.0, 96.0, ACCENT),
        (456.0, 150.0, ACCENT),
        (564.0, 96.0, ACCENT),
        (672.0, 40.0, ACCENT_ALT),
        (780.0, 120.0, ACCENT),
    ]

    centre_y = (card[1] + card[3]) * 0.5
    inv_samples = 1.0 / float(SUPERSAMPLE * SUPERSAMPLE)

    rows = []

    for py in range(SIZE):
        row = bytearray()

        for px in range(SIZE):
            r = g = b = 0.0

            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    x = px + (sx + 0.5) / SUPERSAMPLE
                    y = py + (sy + 0.5) / SUPERSAMPLE

                    # background gradient
                    cr, cg, cb = mix(BACKGROUND_TOP, BACKGROUND_BOTTOM, y / SIZE)

                    if rounded_rect_contains(x, y, card, card_radius):
                        # card fill, slightly lighter towards the top
                        t = (y - card[1]) / (card[3] - card[1])
                        cr, cg, cb = mix((0x1D, 0x23, 0x2D), (0x11, 0x15, 0x1B), t)

                        # 1 px style outline inside the card edge
                        if not rounded_rect_contains(x, y, inner, card_radius - 7):
                            cr, cg, cb = OUTLINE
                        else:
                            for cx, half, colour in bars:
                                if abs(x - cx) <= bar_width * 0.5 and abs(y - centre_y) <= half:
                                    cr, cg, cb = colour
                                    break

                    r += cr
                    g += cg
                    b += cb

            row += bytes((int(r * inv_samples), int(g * inv_samples), int(b * inv_samples), 255))

        rows.append(bytes(row))

    return b"".join(rows)


def write_png(path: Path, pixels: bytes, size: int) -> None:
    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    raw = bytearray()

    for y in range(size):
        raw.append(0)
        raw += pixels[y * size * 4:(y + 1) * size * 4]

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent.parent / "Resources" / "AppIcon.png")
    args = parser.parse_args()

    write_png(args.output, render(), SIZE)
    print(f"wrote {args.output} ({args.output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
