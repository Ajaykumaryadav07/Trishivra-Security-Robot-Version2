import importlib
import sys
import numpy as np


def test_list_cameras_and_usb_read(monkeypatch):
    mod = importlib.reload(__import__('python.camera_handler', fromlist=['*']))

    # Fake cv2.VideoCapture that only opens index 0
    class FakeCap:
        def __init__(self, idx):
            self.idx = idx
            self._opened = (idx == 0)

        def isOpened(self):
            return self._opened

        def read(self):
            return True, np.zeros((10, 10, 3), dtype=np.uint8)

        def release(self):
            self._opened = False

        def set(self, a, b):
            return True

    fake_cv2 = type('cv2', (), {})()
    fake_cv2.VideoCapture = FakeCap
    fake_cv2.CAP_PROP_FRAME_WIDTH = 3
    fake_cv2.CAP_PROP_FRAME_HEIGHT = 4
    fake_cv2.CAP_PROP_FPS = 5
    fake_cv2.cvtColor = lambda arr, flag: arr

    monkeypatch.setattr(mod, 'cv2', fake_cv2, raising=False)
    monkeypatch.setattr(mod, 'CV2_AVAILABLE', True, raising=False)
    monkeypatch.setattr(mod, 'PI_CAMERA_AVAILABLE', False, raising=False)

    # list_cameras should find index 0
    cams = mod.list_cameras(max_index=3)
    assert 0 in cams

    # Creating a USB camera handler should succeed and read returns a frame
    cam = mod.CameraHandler(camera_type='usb')
    assert cam.is_opened() is True
    ok, frame = cam.read()
    assert ok is True
    assert frame.shape[0] == 10
    cam.release()
