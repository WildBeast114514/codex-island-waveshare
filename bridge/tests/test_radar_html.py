from __future__ import annotations

from datetime import datetime
from pathlib import Path

import pytest

from codex_island_bridge.radar import RadarProviderError, parse_radar_html

FIXTURES = Path(__file__).parent / "fixtures"


def test_semantic_table_parses_dynamic_model_names() -> None:
    now = int(datetime(2026, 7, 15, 12, 0).astimezone().timestamp())
    snapshot = parse_radar_html(
        (FIXTURES / "codexradar_minimal.html").read_text(), now=now
    )
    assert [model.family for model in snapshot.models] == ["Orion", "Nova", "Pulse"]
    assert [model.key for model in snapshot.models] == [
        "orion/max",
        "nova/high",
        "pulse/low",
    ]
    assert snapshot.models[1].iq_x10 == 1055
    assert datetime.fromtimestamp(snapshot.updated_at).day == 15


def test_current_score_chip_shape_and_pass_counts() -> None:
    html = """
    <section><h2>降智雷达</h2>
      <div data-model-key="new_a" role="button"><span>Alpha max</span><strong>125.0</strong></div>
      <div data-model-key="new_b" role="button"><span>Beta medium</span><strong>101.5</strong></div>
      <div data-model-key="new_c" role="button"><span>Gamma low</span><strong>75.0</strong></div>
      <circle data-model-key="new_a" data-model-iq-tooltip-key="iq|today|125" aria-label="Alpha max: IQ 125.0, 9/10"></circle>
      <circle data-model-key="new_b" data-model-iq-tooltip-key="iq|today|101.5" aria-label="Beta medium: IQ 101.5, 7/10"></circle>
      <circle data-model-key="new_c" data-model-iq-tooltip-key="iq|today|75" aria-label="Gamma low: IQ 75.0, 5/10"></circle>
    </section>
    """
    snapshot = parse_radar_html(html, now=123_456)
    assert snapshot.updated_at == 123_456
    assert snapshot.models[0].passed == 9
    assert snapshot.models[1].iq_x10 == 1015


def test_mismatched_semantic_rows_are_rejected() -> None:
    html = """
    <section><h2>降智雷达</h2><table>
      <tr><th>项目</th><td>A max</td><td>B max</td><td>C max</td></tr>
      <tr><th>通过数</th><td>1/10</td><td>2/10</td></tr>
      <tr><th>IQ</th><td>15</td><td>30</td><td>45</td></tr>
    </table></section>
    """
    with pytest.raises(RadarProviderError, match="lengths"):
        parse_radar_html(html, now=123_456)
