#!/usr/bin/env python3
"""
convert_image.py  —  Convert a photo for the Tjofia Weather Station display.

The GC9A01 is a 240x240 circular display that expects raw RGB565 big-endian
binary data (exactly 115 200 bytes per image).  Upload the resulting .raw file
via the web portal at http://TjofiaWX.local/images.

Usage:
    python convert_image.py photo.jpg
    python convert_image.py photo.jpg penny.raw
    python convert_image.py photo.jpg --preview

Non-square images are centre-cropped before scaling so the subject stays
centred rather than squashed.

Requirements:
    pip install Pillow
"""

import sys
import argparse
from pathlib import Path


def convert(src_path: str, dst_path: str = None, preview: bool = False) -> None:
    try:
        from PIL import Image
    except ImportError:
        print("ERROR: Pillow is required.  Install it with:  pip install Pillow")
        sys.exit(1)

    src = Path(src_path)
    if not src.exists():
        print(f"ERROR: file not found: {src}")
        sys.exit(1)

    dst = Path(dst_path) if dst_path else src.with_suffix(".raw")

    print(f"Loading   {src} ...")
    img = Image.open(src).convert("RGB")
    print(f"Original  {img.width}x{img.height} px")

    # Centre-crop to square so the subject stays centred rather than squashed.
    w, h = img.size
    if w != h:
        side = min(w, h)
        left = (w - side) // 2
        top  = (h - side) // 2
        img  = img.crop((left, top, left + side, top + side))
        print(f"Cropped   {side}x{side} px  (centre crop)")

    img = img.resize((240, 240), Image.LANCZOS)

    if preview:
        img.show()
        answer = input("Save this image? [Y/n] ").strip().lower()
        if answer not in ("", "y", "yes"):
            print("Aborted.")
            sys.exit(0)

    # Encode as big-endian RGB565 (high byte first — matches GC9A01 native order).
    raw = bytearray()
    for r, g, b in img.getdata():
        px = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        raw += px.to_bytes(2, "big")

    if len(raw) != 115200:
        print(f"ERROR: unexpected output size {len(raw)} (expected 115200)")
        sys.exit(1)

    with open(dst, "wb") as fh:
        fh.write(raw)

    print(f"Saved     {dst}  ({len(raw):,} bytes)")
    print(f"Upload '{dst.name}' via  http://TjofiaWX.local/images")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert an image to 240x240 RGB565 raw format for the Tjofia WX display.",
    )
    parser.add_argument("input",            help="Source image (JPEG, PNG, HEIC, BMP, ...)")
    parser.add_argument("output", nargs="?", help="Output .raw file (default: <input>.raw)")
    parser.add_argument("--preview", action="store_true",
                        help="Open preview and confirm before saving")
    args = parser.parse_args()
    convert(args.input, args.output, args.preview)


if __name__ == "__main__":
    main()
