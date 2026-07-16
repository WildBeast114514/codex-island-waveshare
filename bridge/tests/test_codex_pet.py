from __future__ import annotations

import json
from pathlib import Path

from codex_island_bridge.codex_pet import CodexPetProvider


def append_event(
    path: Path,
    timestamp: str,
    event_type: str,
    payload_type: str,
    **payload: object,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as handle:
        handle.write(
            json.dumps(
                {
                    "timestamp": timestamp,
                    "type": event_type,
                    "payload": {"type": payload_type, **payload},
                }
            )
            + "\n"
        )


def test_task_lifecycle_maps_to_pet_states(tmp_path: Path) -> None:
    path = tmp_path / "2026" / "07" / "16" / "rollout-one.jsonl"
    append_event(path, "2026-07-16T10:00:00Z", "event_msg", "task_started")
    provider = CodexPetProvider(tmp_path, discovery_seconds=0)

    running = provider.collect(now=1_784_196_120)
    assert running.state == "running"
    assert running.active_tasks == 1

    append_event(path, "2026-07-16T10:04:00Z", "event_msg", "task_complete")
    review = provider.collect(now=1_784_196_300)
    assert review.state == "review"
    assert review.active_tasks == 0

    assert provider.collect(now=1_784_196_600).state == "idle"


def test_pending_user_input_overrides_running(tmp_path: Path) -> None:
    path = tmp_path / "rollout-waiting.jsonl"
    append_event(path, "2026-07-16T10:00:00Z", "event_msg", "task_started")
    append_event(
        path,
        "2026-07-16T10:01:00Z",
        "response_item",
        "function_call",
        name="request_user_input",
        call_id="call-1",
    )
    provider = CodexPetProvider(tmp_path, discovery_seconds=0)
    waiting = provider.collect(now=1_784_196_120)
    assert waiting.state == "waiting"
    assert waiting.active_tasks == 1

    append_event(
        path,
        "2026-07-16T10:02:00Z",
        "response_item",
        "function_call_output",
        call_id="call-1",
    )
    assert provider.collect(now=1_784_196_180).state == "running"


def test_aborted_task_is_temporarily_blocked(tmp_path: Path) -> None:
    path = tmp_path / "rollout-failed.jsonl"
    append_event(path, "2026-07-16T10:00:00Z", "event_msg", "task_started")
    append_event(path, "2026-07-16T10:01:00Z", "event_msg", "turn_aborted")
    provider = CodexPetProvider(tmp_path, discovery_seconds=0)

    assert provider.collect(now=1_784_196_120).state == "failed"
    assert provider.collect(now=1_784_196_500).state == "idle"


def test_parallel_active_tasks_are_counted(tmp_path: Path) -> None:
    for index in range(2):
        append_event(
            tmp_path / f"rollout-{index}.jsonl",
            "2026-07-16T10:00:00Z",
            "event_msg",
            "task_started",
        )
    provider = CodexPetProvider(tmp_path, discovery_seconds=0)
    snapshot = provider.collect(now=1_784_196_120)
    assert snapshot.state == "running"
    assert snapshot.active_tasks == 2
