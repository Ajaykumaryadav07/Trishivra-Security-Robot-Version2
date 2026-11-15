import asyncio

import pytest

from python import telegram_bot


@pytest.mark.parametrize(
    ("handler_name", "expected_command"),
    [
        ("stop_command", "STOP"),
        ("resume_command", "RESUME"),
        ("patrol_command", "PATROL"),
    ],
)
def test_telegram_control_handlers_write_robot_commands(
    handler_name, expected_command, tmp_path, monkeypatch
):
    command_file = tmp_path / "robot_command.txt"
    monkeypatch.setattr(telegram_bot, "COMMAND_FILE", str(command_file))

    class FakeMessage:
        def __init__(self):
            self.reply = None

        async def reply_text(self, text):
            self.reply = text

    class FakeUpdate:
        def __init__(self):
            self.message = FakeMessage()

    handler = getattr(telegram_bot, handler_name)
    update = FakeUpdate()
    asyncio.run(handler(update, None))

    assert command_file.read_text() == f"{expected_command}\n"
    assert update.message.reply
