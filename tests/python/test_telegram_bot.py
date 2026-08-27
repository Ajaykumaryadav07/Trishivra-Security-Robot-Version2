import os


def test_send_command_to_robot_writes_file(tmp_path, monkeypatch):
    mod = __import__('python.telegram_bot', fromlist=['*'])

    fake_cmd_file = tmp_path / "cmd.txt"
    monkeypatch.setattr(mod, 'COMMAND_FILE', str(fake_cmd_file), raising=False)

    ok = mod.send_command_to_robot('TESTCMD', '42')
    assert ok is True
    content = fake_cmd_file.read_text()
    assert 'TESTCMD 42' in content

