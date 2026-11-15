import importlib
import pytest


def test_usb_camera_is_not_supported(monkeypatch):
    mod = importlib.reload(__import__('python.camera_handler', fromlist=['*']))
    monkeypatch.setattr(mod, 'PI_CAMERA_AVAILABLE', True)

    class FakeCV2:
        def __getattr__(self, name):
            raise AssertionError(f"OpenCV camera API must not be used: {name}")

    monkeypatch.setattr(mod, 'cv2', FakeCV2())
    with pytest.raises(ValueError, match="Unsupported camera type"):
        mod.CameraHandler(camera_type='usb')
