from codex_island_bridge.ble_transport import NotificationLineBuffer


def test_notification_lines_reassemble_across_chunks() -> None:
    buffer = NotificationLineBuffer()
    assert buffer.feed(b'{"v":1,') == []
    assert buffer.feed(b'"k":"hello"}\n') == [{"v": 1, "k": "hello"}]


def test_notification_overflow_drops_only_the_bad_line() -> None:
    buffer = NotificationLineBuffer(maximum=5)
    assert buffer.feed(b"123456789\n") == []
    assert buffer.feed(b"{}\n") == [{}]

