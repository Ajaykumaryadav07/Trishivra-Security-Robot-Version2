import os
from types import SimpleNamespace

import pytest


def test_send_telegram_photo_success(tmp_path, monkeypatch):
    module = importlib = __import__('python.send_telegram_photo', fromlist=['*'])

    # create dummy image
    p = tmp_path / "img.jpg"
    p.write_bytes(b"\xff\xd8\xff\xd9")

    # Provide valid-looking bot token and chat id
    monkeypatch.setattr(module, 'BOT_TOKEN', 'fake-token', raising=False)
    monkeypatch.setattr(module, 'CHAT_ID', '12345', raising=False)

    # Ensure requests appears available and mock post
    monkeypatch.setattr(module, 'REQUESTS_AVAILABLE', True, raising=False)

    class DummyResp:
        def __init__(self, code=200, text='OK'):
            self.status_code = code
            self.text = text

    def fake_post(url, files=None, data=None, json=None, timeout=None):
        return DummyResp(200)

    import requests as real_requests
    monkeypatch.setattr(module, 'requests', SimpleNamespace(post=fake_post, exceptions=real_requests.exceptions))

    assert module.send_telegram_photo(str(p)) is True

