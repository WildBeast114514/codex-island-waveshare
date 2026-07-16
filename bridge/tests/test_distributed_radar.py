from __future__ import annotations

import pytest

from codex_island_bridge.distributed_radar import (
    DistributedRadarError,
    display_model_name,
    parse_distributed_table,
)


def fixture() -> dict:
    return {
        "schema": 1,
        "combos": [
            {"model": "gpt-6.0-orbit", "effort": "low"},
            {"model": "gpt-6.0-orbit", "effort": "max"},
            {"model": "future-engine", "effort": "ultra"},
        ],
        "tasks": [{"id": "one"}, {"id": "two"}],
        "cells": {
            "one|gpt-6.0-orbit|low": {"rate": 1.0, "p": 2, "n": 2},
            "two|gpt-6.0-orbit|low": {"rate": 0.0, "p": 0, "n": 2},
            "one|gpt-6.0-orbit|max": {"rate": 1.0, "p": 3, "n": 3},
            "two|gpt-6.0-orbit|max": {"rate": None, "p": 0, "n": 0},
            "one|future-engine|ultra": {"rate": 0.5, "p": 1, "n": 2},
        },
    }


def test_live_iq_uses_dynamic_models_efforts_and_omits_family_average() -> None:
    snapshot = parse_distributed_table(fixture(), now=1_234)
    assert snapshot.updated_at == 1_234
    assert [(row.model, row.effort, row.aggregate) for row in snapshot.rows] == [
        ("Orbit", "", True),
        ("Orbit", "low", False),
        ("Orbit", "max", False),
        ("Future Engine", "", True),
        ("Future Engine", "ultra", False),
    ]
    assert [(row.iq, row.passed, row.total) for row in snapshot.rows] == [
        (107, 5, 7),
        (75, 2, 4),
        (150, 3, 3),
        (75, 1, 2),
        (75, 1, 2),
    ]
    assert all(row.model != "GPT-6.0" for row in snapshot.rows)


def test_model_display_names_are_not_an_enum() -> None:
    assert display_model_name("gpt-5.6-sol") == "Sol"
    assert display_model_name("gpt-5.5") == "GPT-5.5"
    assert display_model_name("gpt-7.1-new-horizon") == "New Horizon"
    assert display_model_name("custom_model") == "Custom Model"


def test_invalid_cell_counts_are_rejected() -> None:
    payload = fixture()
    payload["cells"]["one|gpt-6.0-orbit|low"]["p"] = 3
    with pytest.raises(DistributedRadarError, match="metrics"):
        parse_distributed_table(payload, now=1_234)
