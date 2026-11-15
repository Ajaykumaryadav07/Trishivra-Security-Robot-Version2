import pytest


def test_camerahandler_raises_when_no_cv2(monkeypatch):
    mod = __import__('python.camera_handler', fromlist=['*'])

    # Simulate no OpenCV and no picamera2
    monkeypatch.setattr(mod, 'CV2_AVAILABLE', False)
    monkeypatch.setattr(mod, 'PI_CAMERA_AVAILABLE', False)

    with pytest.raises(RuntimeError):
        # auto init should raise because no camera backends available
        mod.CameraHandler(camera_type='auto')


def test_camerahandler_api_surface():
    mod = __import__('python.camera_handler', fromlist=['*'])
    assert hasattr(mod, 'CameraHandler')
    assert callable(mod.CameraHandler)


def test_pi_camera_requires_opencv_for_frame_processing(monkeypatch):
    mod = __import__('python.camera_handler', fromlist=['*'])
    monkeypatch.setattr(mod, 'CV2_AVAILABLE', False)
    monkeypatch.setattr(mod, 'PI_CAMERA_AVAILABLE', True)

    with pytest.raises(RuntimeError, match="OpenCV"):
        mod.CameraHandler(camera_type='pi_camera')
