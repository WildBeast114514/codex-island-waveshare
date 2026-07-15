from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path
from typing import Any


class AtomicJsonFile:
    def __init__(self, path: Path, schema_version: int = 1) -> None:
        self.path = path
        self.schema_version = schema_version

    def load(self, default: Any = None) -> Any:
        try:
            with self.path.open("r", encoding="utf-8") as handle:
                wrapper = json.load(handle)
        except (OSError, json.JSONDecodeError, TypeError):
            return default
        if not isinstance(wrapper, dict) or wrapper.get("schema_version") != self.schema_version:
            return default
        return wrapper.get("data", default)

    def save(self, data: Any) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{self.path.name}.", suffix=".tmp", dir=self.path.parent
        )
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
                json.dump(
                    {"schema_version": self.schema_version, "data": data},
                    handle,
                    ensure_ascii=False,
                    separators=(",", ":"),
                )
                handle.write("\n")
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(temporary_name, self.path)
            directory_fd = os.open(self.path.parent, os.O_RDONLY)
            try:
                os.fsync(directory_fd)
            finally:
                os.close(directory_fd)
        except BaseException:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass
            raise
