#!/usr/bin/env python3
"""Convert a Codex-style WebP sprite sheet into the firmware pet pack."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

try:
    from PIL import Image
except ImportError as error:  # pragma: no cover - exercised by the CLI environment
    raise SystemExit(
        "Pillow is required to build pet assets; run scripts/bootstrap_macos.sh"
    ) from error


MAGIC = b"CPET"
PACK_VERSION = 1
HEADER = struct.Struct("<4s8H5I32s")
ANIMATION_ENTRY = struct.Struct("<BBHHH")
FRAME_ENTRY = struct.Struct("<II")
ANIMATION_IDS = {
    "idle": 0,
    "run_right": 1,
    "run_left": 2,
    "waving": 3,
    "jumping": 4,
    "failed": 5,
    "waiting": 6,
    "running": 7,
    "review": 8,
}


class PetAssetError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Animation:
    name: str
    row: int
    frames: int
    frame_ms: int


@dataclass(frozen=True, slots=True)
class Manifest:
    path: Path
    pet_id: str
    display_name: str
    source_url: str | None
    source_path: Path | None
    source_sha256: str
    columns: int
    rows: int
    output_scale: int
    alpha_threshold: int
    palette_colors: int
    animations: tuple[Animation, ...]


def _integer(value: Any, name: str, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise PetAssetError(f"{name} must be an integer")
    if not minimum <= value <= maximum:
        raise PetAssetError(f"{name} must be between {minimum} and {maximum}")
    return value


def load_manifest(path: Path) -> Manifest:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise PetAssetError(f"cannot read pet manifest {path}: {error}") from error
    if not isinstance(raw, dict) or raw.get("schema_version") != 1:
        raise PetAssetError("pet manifest schema_version must be 1")

    pet_id = raw.get("id")
    display_name = raw.get("display_name")
    if not isinstance(pet_id, str) or not pet_id.strip():
        raise PetAssetError("pet id must be a non-empty string")
    if not isinstance(display_name, str) or not display_name.strip():
        raise PetAssetError("pet display_name must be a non-empty string")

    source = raw.get("source")
    sheet = raw.get("sheet")
    animations_raw = raw.get("animations")
    if not isinstance(source, dict) or not isinstance(sheet, dict):
        raise PetAssetError("source and sheet must be objects")
    if not isinstance(animations_raw, list) or not animations_raw:
        raise PetAssetError("animations must be a non-empty array")

    source_url = source.get("url")
    source_relative = source.get("path")
    if bool(source_url) == bool(source_relative):
        raise PetAssetError("source must contain exactly one of url or path")
    if source_url is not None and (
        not isinstance(source_url, str) or not source_url.startswith("https://")
    ):
        raise PetAssetError("source.url must be an HTTPS URL")
    source_path = None
    if source_relative is not None:
        if not isinstance(source_relative, str) or not source_relative.strip():
            raise PetAssetError("source.path must be a non-empty string")
        source_path = (path.parent / source_relative).resolve()
    sha256 = source.get("sha256")
    if (
        not isinstance(sha256, str)
        or len(sha256) != 64
        or any(character not in "0123456789abcdefABCDEF" for character in sha256)
    ):
        raise PetAssetError("source.sha256 must be a 64-character hexadecimal digest")

    columns = _integer(sheet.get("columns"), "sheet.columns", 1, 32)
    rows = _integer(sheet.get("rows"), "sheet.rows", 1, 32)
    output_scale = _integer(sheet.get("output_scale", 1), "sheet.output_scale", 1, 4)
    alpha_threshold = _integer(
        sheet.get("alpha_threshold", 8), "sheet.alpha_threshold", 0, 254
    )
    palette_colors = _integer(
        sheet.get("palette_colors", 255), "sheet.palette_colors", 2, 255
    )

    animations: list[Animation] = []
    names: set[str] = set()
    for index, item in enumerate(animations_raw):
        if not isinstance(item, dict):
            raise PetAssetError(f"animations[{index}] must be an object")
        name = item.get("name")
        if not isinstance(name, str) or name not in ANIMATION_IDS:
            raise PetAssetError(
                f"animations[{index}].name must be one of {', '.join(ANIMATION_IDS)}"
            )
        if name in names:
            raise PetAssetError(f"animation {name} is duplicated")
        names.add(name)
        row = _integer(item.get("row"), f"animations[{index}].row", 0, rows - 1)
        frames = _integer(
            item.get("frames"), f"animations[{index}].frames", 1, columns
        )
        frame_ms = _integer(
            item.get("frame_ms", 150),
            f"animations[{index}].frame_ms",
            40,
            5000,
        )
        animations.append(Animation(name, row, frames, frame_ms))
    if "idle" not in names:
        raise PetAssetError("the idle animation is required as the runtime fallback")

    return Manifest(
        path=path,
        pet_id=pet_id.strip(),
        display_name=display_name.strip(),
        source_url=source_url,
        source_path=source_path,
        source_sha256=sha256.lower(),
        columns=columns,
        rows=rows,
        output_scale=output_scale,
        alpha_threshold=alpha_threshold,
        palette_colors=palette_colors,
        animations=tuple(animations),
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_source(manifest: Manifest, *, offline: bool = False) -> Path:
    if manifest.source_path is not None:
        source = manifest.source_path
    else:
        cache_dir = manifest.path.parent / ".cache"
        source = cache_dir / f"{manifest.pet_id}-{manifest.source_sha256[:12]}.webp"
        if not source.exists() or sha256_file(source) != manifest.source_sha256:
            if offline:
                raise PetAssetError(f"pet source is not cached for offline build: {source}")
            cache_dir.mkdir(parents=True, exist_ok=True)
            temporary = source.with_suffix(".download")
            try:
                request = urllib.request.Request(
                    manifest.source_url,
                    headers={"User-Agent": "codex-island-pet-builder/1"},
                )
                with urllib.request.urlopen(request, timeout=30) as response:
                    temporary.write_bytes(response.read())
                temporary.replace(source)
            except (OSError, urllib.error.URLError) as error:
                temporary.unlink(missing_ok=True)
                raise PetAssetError(f"cannot download pet source: {error}") from error
    if not source.is_file():
        raise PetAssetError(f"pet source does not exist: {source}")
    actual_digest = sha256_file(source)
    if actual_digest != manifest.source_sha256:
        raise PetAssetError(
            f"pet source SHA-256 mismatch: expected {manifest.source_sha256}, "
            f"got {actual_digest}"
        )
    return source


def encode_rle(indices: bytes | bytearray | Iterable[int]) -> bytes:
    data = bytes(indices)
    output = bytearray()
    offset = 0
    while offset < len(data):
        run = 1
        while (
            offset + run < len(data)
            and data[offset + run] == data[offset]
            and run < 128
        ):
            run += 1
        if run >= 3:
            output.extend((0x80 | (run - 1), data[offset]))
            offset += run
            continue

        literal_start = offset
        offset += run
        while offset < len(data) and offset - literal_start < 128:
            next_run = 1
            while (
                offset + next_run < len(data)
                and data[offset + next_run] == data[offset]
                and next_run < 128
            ):
                next_run += 1
            if next_run >= 3:
                break
            offset += next_run
        literal = data[literal_start:offset]
        output.append(len(literal) - 1)
        output.extend(literal)
    return bytes(output)


def decode_rle(data: bytes, expected_size: int) -> bytes:
    output = bytearray()
    offset = 0
    while offset < len(data) and len(output) < expected_size:
        command = data[offset]
        offset += 1
        count = (command & 0x7F) + 1
        if command & 0x80:
            if offset >= len(data):
                raise PetAssetError("truncated repeated RLE command")
            output.extend((data[offset],) * count)
            offset += 1
        else:
            end = offset + count
            if end > len(data):
                raise PetAssetError("truncated literal RLE command")
            output.extend(data[offset:end])
            offset = end
    if offset != len(data) or len(output) != expected_size:
        raise PetAssetError("RLE stream does not match the expected frame size")
    return bytes(output)


def _pixels(image: Image.Image) -> Any:
    flattened = getattr(image, "get_flattened_data", None)
    return flattened() if flattened is not None else image.getdata()


def _name_field(display_name: str) -> bytes:
    encoded = (
        display_name.encode("utf-8")[:31].decode("utf-8", errors="ignore").encode("utf-8")
    )
    return encoded + bytes(32 - len(encoded))


def build_pack(manifest: Manifest, source: Path) -> tuple[bytes, dict[str, int]]:
    try:
        sheet = Image.open(source).convert("RGBA")
    except (OSError, ValueError) as error:
        raise PetAssetError(f"cannot decode WebP source {source}: {error}") from error
    if sheet.width % manifest.columns or sheet.height % manifest.rows:
        raise PetAssetError(
            f"sheet size {sheet.width}x{sheet.height} is not divisible by "
            f"{manifest.columns}x{manifest.rows}"
        )
    source_width = sheet.width // manifest.columns
    source_height = sheet.height // manifest.rows
    frame_width = source_width * manifest.output_scale
    frame_height = source_height * manifest.output_scale
    if frame_width > 466 or frame_height > 466:
        raise PetAssetError(
            f"scaled frame {frame_width}x{frame_height} exceeds the 466x466 display"
        )

    quantized = sheet.quantize(
        colors=manifest.palette_colors,
        method=Image.Quantize.FASTOCTREE,
        dither=Image.Dither.NONE,
    )
    rgba_palette = quantized.getpalette("RGBA") or []
    required_palette_values = manifest.palette_colors * 4
    if len(rgba_palette) < required_palette_values:
        raise PetAssetError("Pillow returned an incomplete RGBA palette")
    palette = bytearray((0, 0, 0, 0))
    palette.extend(rgba_palette[:required_palette_values])
    palette.extend(bytes(256 * 4 - len(palette)))

    animation_records = bytearray()
    compressed_frames: list[bytes] = []
    first_frame = 0
    for animation in manifest.animations:
        animation_records.extend(
            ANIMATION_ENTRY.pack(
                ANIMATION_IDS[animation.name],
                0,
                first_frame,
                animation.frames,
                animation.frame_ms,
            )
        )
        for column in range(animation.frames):
            box = (
                column * source_width,
                animation.row * source_height,
                (column + 1) * source_width,
                (animation.row + 1) * source_height,
            )
            source_frame = sheet.crop(box)
            indexed_frame = quantized.crop(box)
            alpha = source_frame.getchannel("A")
            remapped = Image.new("L", (source_width, source_height))
            remapped.putdata(
                [
                    0
                    if alpha_value <= manifest.alpha_threshold
                    else palette_index + 1
                    for palette_index, alpha_value in zip(
                        _pixels(indexed_frame), _pixels(alpha), strict=True
                    )
                ]
            )
            if manifest.output_scale != 1:
                remapped = remapped.resize(
                    (frame_width, frame_height), Image.Resampling.NEAREST
                )
            raw_indices = bytes(_pixels(remapped))
            compressed = encode_rle(raw_indices)
            if decode_rle(compressed, frame_width * frame_height) != raw_indices:
                raise PetAssetError("internal RLE round-trip check failed")
            compressed_frames.append(compressed)
        first_frame += animation.frames

    animation_offset = HEADER.size
    frame_offset = animation_offset + len(animation_records)
    palette_offset = frame_offset + len(compressed_frames) * FRAME_ENTRY.size
    data_offset = palette_offset + len(palette)
    padding = (-data_offset) % 4
    data_offset += padding

    frame_records = bytearray()
    frame_data = bytearray()
    for compressed in compressed_frames:
        frame_records.extend(
            FRAME_ENTRY.pack(data_offset + len(frame_data), len(compressed))
        )
        frame_data.extend(compressed)
    total_size = data_offset + len(frame_data)
    header = HEADER.pack(
        MAGIC,
        PACK_VERSION,
        HEADER.size,
        frame_width,
        frame_height,
        256,
        len(manifest.animations),
        len(compressed_frames),
        0,
        animation_offset,
        frame_offset,
        palette_offset,
        data_offset,
        total_size,
        _name_field(manifest.display_name),
    )
    pack = (
        header
        + bytes(animation_records)
        + bytes(frame_records)
        + bytes(palette)
        + bytes(padding)
        + bytes(frame_data)
    )
    if len(pack) != total_size:
        raise PetAssetError("internal pet pack size mismatch")
    return pack, {
        "width": frame_width,
        "height": frame_height,
        "animations": len(manifest.animations),
        "frames": len(compressed_frames),
        "raw_index_bytes": len(compressed_frames) * frame_width * frame_height,
        "pack_bytes": len(pack),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--offline", action="store_true")
    arguments = parser.parse_args(argv)
    try:
        manifest = load_manifest(arguments.manifest.resolve())
        source = resolve_source(manifest, offline=arguments.offline)
        pack, summary = build_pack(manifest, source)
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = arguments.output.with_suffix(arguments.output.suffix + ".tmp")
        temporary.write_bytes(pack)
        temporary.replace(arguments.output)
    except PetAssetError as error:
        print(f"pet asset error: {error}", file=sys.stderr)
        return 2
    print(
        f"Pet asset {manifest.display_name}: "
        f"{summary['width']}x{summary['height']}, "
        f"{summary['animations']} animations/{summary['frames']} frames, "
        f"{summary['pack_bytes']} bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
