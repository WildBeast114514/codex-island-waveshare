# Pet manifest interface

`scripts/build_pet_asset.py` converts a Codex-style WebP sprite sheet into the
compact animation pack embedded by the ESP-IDF build. The firmware understands
generic animation IDs; all sprite-sheet layout details stay in a JSON manifest.

Use `mambo.json` as the template. A manifest contains:

- `source.url` or `source.path`, plus the required SHA-256;
- the sprite-sheet column and row count;
- an integer `output_scale` from 1 to 4;
- an optional transparency threshold and palette size;
- animation row, frame count, and frame duration.

`idle` is required. The other supported names are `run_right`, `run_left`,
`waving`, `jumping`, `failed`, `waiting`, `running`, and `review`. Missing
non-idle animations automatically fall back to `idle` on the device.

Build another pet without modifying firmware code:

```bash
CODEX_PET_MANIFEST="$PWD/assets/pets/my-pet.json" scripts/build_firmware.sh
```

The generated `firmware/generated/pet_asset.bin` and downloaded
`assets/pets/.cache/` source are ignored by Git. Commit the manifest so another
machine can reproduce the same asset from its verified URL or local file.
