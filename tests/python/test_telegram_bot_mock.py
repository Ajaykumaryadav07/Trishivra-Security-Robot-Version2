import importlib
import tempfile
import os


def test_send_photo_with_mocked_bot(monkeypatch, tmp_path):
    mod = importlib.reload(__import__('python.telegram_bot', fromlist=['*']))

    # Create a dummy photo file
    photo = tmp_path / 'p.jpg'
    photo.write_bytes(b'JPEG')

    # Fake Bot class
    sent = {}

    class FakeBot:
        def __init__(self, token):
            self.token = token

        def send_photo(self, chat_id, photo, caption=None):
            sent['chat_id'] = chat_id
            sent['caption'] = caption

    monkeypatch.setattr(mod, 'TELEGRAM_AVAILABLE', True, raising=False)
    monkeypatch.setattr(mod, 'Bot', FakeBot, raising=False)
    monkeypatch.setattr(mod, 'BOT_TOKEN', 'fake-token', raising=False)
    monkeypatch.setattr(mod, 'CHAT_ID', '12345', raising=False)

    ok = mod.send_telegram_photo(str(photo), caption='hello')
    assert ok is True
    assert sent.get('chat_id') == '12345'


def test_capture_photo_uses_camerahandler(monkeypatch, tmp_path):
    mod = importlib.reload(__import__('python.telegram_bot', fromlist=['*']))

    # Fake CameraHandler that returns a dummy frame
    class FakeCam:
        def __init__(self, *args, **kwargs):
            self.is_initialized = True

        def read(self):
            return True, b'FAKEFRAME'

        def release(self):
            pass

    monkeypatch.setitem(os.environ, 'TMPDIR', str(tmp_path))
    monkeypatch.setattr('python.telegram_bot.CameraHandler', FakeCam, raising=False)

    # Provide a fake cv2.imwrite that writes a file
    def fake_imwrite(path, frame):
        with open(path, 'wb') as f:
            f.write(b'JUNK')
        return True

    import sys
    sys.modules['cv2'] = type('cv2', (), {'imwrite': fake_imwrite})

    out = mod.capture_photo()
    assert out is not None
    assert os.path.exists(out)
