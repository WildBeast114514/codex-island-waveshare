from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image

from scripts.build_pet_asset import (
    HEADER,
    MAGIC,
    build_pack,
    decode_rle,
    encode_rle,
    load_manifest,
    resolve_source,
)


def test_rle_round_trip_handles_transparent_and_literal_runs() -> None:
    source = bytes([0] * 130 + [1, 2, 3, 4] + [9] * 7)
    encoded = encode_rle(source)
    assert len(encoded) < len(source)
    assert decode_rle(encoded, len(source)) == source


def test_local_webp_manifest_builds_a_valid_pack(tmp_path: Path) -> None:
    image_path = tmp_path / "tiny.webp"
    image = Image.new("RGBA", (4, 2), (0, 0, 0, 0))
    image.putpixel((1, 0), (255, 0, 128, 255))
    image.putpixel((2, 1), (0, 255, 128, 200))
    image.save(image_path, "WEBP", lossless=True)
    digest = hashlib.sha256(image_path.read_bytes()).hexdigest()
    manifest_path = tmp_path / "pet.json"
    manifest_path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "id": "tiny",
                "display_name": "Tiny",
                "source": {"path": "tiny.webp", "sha256": digest},
                "sheet": {
                    "columns": 1,
                    "rows": 1,
                    "output_scale": 2,
                    "palette_colors": 8,
                },
                "animations": [
                    {"name": "idle", "row": 0, "frames": 1, "frame_ms": 100}
                ],
            }
        ),
        encoding="utf-8",
    )

    manifest = load_manifest(manifest_path)
    pack, summary = build_pack(manifest, resolve_source(manifest))
    header = HEADER.unpack_from(pack)
    assert header[0] == MAGIC
    assert header[3:5] == (8, 4)
    assert header[6:8] == (1, 1)
    assert summary["frames"] == 1
    assert summary["pack_bytes"] == len(pack)
