#!/usr/bin/env python3
"""Packs every MIDI file below --input into a single binary blob that the
plugin embeds through juce_add_binary_data.

Pack layout (little endian):

    char    magic[4]        "LDLB"
    uint16  version         1
    uint32  numEntries
    ... entries, back to back:
        uint8   nameLen,   uint8 name[...]
        uint8   folderLen, uint8 folder[...]
        uint16  flags
        uint16  numNotes
        uint8   minNote,   uint8 maxNote
        uint16  ppq
        uint16  channelsMask    bit n set -> channel n+1 is used
        uint32  lengthTicks
        uint32  dataOffset      offset of the raw MIDI bytes from the data block
        uint32  dataSize
        float32 bpm             (first tempo event, 120 if none)

The blob is read back by Source/engine/LoopLibrary.cpp.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

MAGIC = b"LDLB"
VERSION = 1
HEADER_SIZE = 10


class MidiParseError(Exception):
    pass


def _read_vlq(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if pos >= len(data):
            raise MidiParseError("truncated variable length quantity")
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, pos
    raise MidiParseError("variable length quantity too long")


def parse_midi(data: bytes) -> dict:
    if len(data) < 14 or data[0:4] != b"MThd":
        raise MidiParseError("missing MThd header")

    header_len = struct.unpack(">I", data[4:8])[0]
    _fmt, num_tracks, division = struct.unpack(">HHH", data[8:14])
    ppq = 480 if division & 0x8000 or division == 0 else division

    pos = 8 + header_len
    longest_track = 0
    last_note_on = 0
    tempo_us = 500_000
    notes = 0
    channels = 0
    min_note, max_note = 128, -1

    for _ in range(num_tracks):
        if pos + 8 > len(data) or data[pos:pos + 4] != b"MTrk":
            break
        track_len = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        pos += 8
        end = min(pos + track_len, len(data))
        running = 0
        tick = 0

        while pos < end:
            delta, pos = _read_vlq(data, pos)
            tick += delta
            if pos >= end:
                break
            status = data[pos]

            if status == 0xFF:
                pos += 1
                if pos >= end:
                    break
                meta_type = data[pos]
                pos += 1
                length, pos = _read_vlq(data, pos)
                payload = data[pos:pos + length]
                pos += length
                if meta_type == 0x51 and length >= 3:
                    tempo_us = (payload[0] << 16) | (payload[1] << 8) | payload[2]
                continue

            if status in (0xF0, 0xF7):
                pos += 1
                length, pos = _read_vlq(data, pos)
                pos += length
                continue

            if status & 0x80:
                running = status
                pos += 1
            elif running == 0:
                raise MidiParseError("running status without status byte")
            else:
                status = running

            kind = status & 0xF0
            channel = (status & 0x0F) + 1
            n_data = 1 if kind in (0xC0, 0xD0) else 2

            if pos + n_data > end:
                break
            d1 = data[pos]
            d2 = data[pos + 1] if n_data == 2 else 0
            pos += n_data

            if kind == 0x90 and d2 > 0:
                notes += 1
                channels |= 1 << (channel - 1)
                min_note = min(min_note, d1)
                max_note = max(max_note, d1)
                last_note_on = max(last_note_on, tick)

        longest_track = max(longest_track, tick)
        pos = end

    bpm = 60_000_000.0 / tempo_us if tempo_us > 0 else 120.0
    return {
        "ppq": ppq,
        "bpm": bpm,
        "lengthTicks": loop_length_ticks(last_note_on, longest_track, ppq),
        "contentTicks": longest_track,
        "numNotes": notes,
        "channelsMask": channels,
        "minNote": min_note if max_note >= 0 else 0,
        "maxNote": max(max_note, 0),
    }


def loop_length_ticks(last_note_on: int, content_end: int, ppq: int) -> int:
    """The loops in this library end on the final note's note-off, so the raw
    track length is always slightly short (or long) of a whole number of bars.

    Derive the musical loop length from the last note *start*, snapped to the
    nearest bar when it is within half a beat of one, otherwise rounded up to
    the next bar.
    """
    bar = ppq * 4
    if last_note_on <= 0:
        return bar

    bars = last_note_on / bar
    nearest = round(bars)
    if abs(bars - nearest) <= 0.125:
        return max(1, nearest) * bar

    return max(1, int(-(-last_note_on // bar))) * bar


def natural_key(text: str):
    parts = []
    for chunk in text.replace("_", " ").split(" "):
        if chunk.isdigit():
            parts.append((1, int(chunk), ""))
        else:
            parts.append((0, 0, chunk.lower()))
    return parts


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Pack .mid loops into one blob")
    parser.add_argument("--input", required=True, type=Path, help="folder containing .mid files")
    parser.add_argument("--output", required=True, type=Path, help="packed .bin to write")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    root = args.input.resolve()
    files = sorted(
        (p for p in root.rglob("*") if p.is_file() and p.suffix.lower() == ".mid"),
        key=lambda p: (str(p.parent.relative_to(root)).lower().replace("\\", "/"), natural_key(p.stem)),
    )

    if not files:
        print(f"error: no .mid files below {root}", file=sys.stderr)
        return 1

    entries = []
    blob = bytearray()
    skipped = 0
    bars_histogram: dict[float, int] = {}

    for path in files:
        raw = path.read_bytes()
        try:
            info = parse_midi(raw)
        except MidiParseError as exc:
            skipped += 1
            print(f"warning: skipping {path}: {exc}", file=sys.stderr)
            continue

        folder = str(path.parent.relative_to(root)).replace("\\", "/")
        if folder == ".":
            folder = ""

        name = path.stem.encode("utf-8")[:255]
        folder_bytes = folder.encode("utf-8")[:255]

        bars = max(info["lengthTicks"] / (info["ppq"] * 4.0), 1.0)
        bars_histogram[round(bars, 2)] = bars_histogram.get(round(bars, 2), 0) + 1

        entries.append(
            (
                name,
                folder_bytes,
                1 if info["numNotes"] else 0,
                min(info["numNotes"], 0xFFFF),
                info["minNote"],
                info["maxNote"],
                info["ppq"],
                info["channelsMask"],
                info["lengthTicks"],
                len(blob),
                len(raw),
                info["bpm"],
            )
        )
        blob += raw

    entry_table = bytearray()
    for (name, folder_bytes, flags, num_notes, min_note, max_note,
         ppq, channels, length_ticks, offset, size, bpm) in entries:
        entry_table += struct.pack("<B", len(name)) + name
        entry_table += struct.pack("<B", len(folder_bytes)) + folder_bytes
        entry_table += struct.pack(
            "<HHBBHHIIIf",
            flags,
            num_notes,
            min_note,
            max_note,
            ppq,
            channels,
            length_ticks,
            offset,
            size,
            bpm,
        )

    out = bytearray()
    out += MAGIC
    out += struct.pack("<H", VERSION)
    out += struct.pack("<I", len(entries))
    out += entry_table
    out += blob

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(out)

    if not args.quiet:
        print(
            f"packed {len(entries)} loops ({skipped} skipped) into {args.output} "
            f"({len(out)} bytes)"
        )
        top = sorted(bars_histogram.items(), key=lambda kv: -kv[1])[:8]
        print("bars: " + ", ".join(f"{b}->{c}" for b, c in top))

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
