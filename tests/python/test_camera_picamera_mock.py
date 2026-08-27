import importlib
import numpy as np


def test_pi_camera_flow(monkeypatch):
    mod = importlib.reload(__import__('python.camera_handler', fromlist=['*']))

    # Fake Picamera2 and request
    class FakeRequest:
        def __init__(self, arr):
            self._arr = arr

        def make_array(self, key):
            return self._arr

        def release(self):
            pass

    class FakePicamera2:
        def __init__(self):
            pass

        def create_preview_configuration(self, main):
            return {'main': main}

        def configure(self, cfg):
            self._cfg = cfg

        def start(self):
            pass

        def capture_request(self):
            return FakeRequest(np.zeros((8, 8, 3), dtype=np.uint8))

    # Patch module to use fake picamera and make cv2.cvtColor a passthrough
    monkeypatch.setattr(mod, 'PI_CAMERA_AVAILABLE', True, raising=False)
    monkeypatch.setattr(mod, 'Picamera2', FakePicamera2, raising=False)
    fake_cv2 = type('cv2', (), {})()
    fake_cv2.cvtColor = lambda arr, flag: arr
    fake_cv2.COLOR_RGB2BGR = 0
    monkeypatch.setattr(mod, 'cv2', fake_cv2, raising=False)

    cam = mod.CameraHandler(camera_type='pi_camera')
    assert cam.camera_type == 'pi_camera'
    ok, frame = cam.read()
    assert ok is True
    assert frame.shape[0] == 8
    cam.release()
