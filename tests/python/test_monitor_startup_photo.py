import os


def test_monitor_sends_one_frame_from_started_camera(monkeypatch):
    from python import monitor

    sent = []

    class FakeCamera:
        def __init__(self, **kwargs):
            self.read_count = 0

        def get_camera_info(self):
            return "fake camera"

        def read(self):
            self.read_count += 1
            return True, object()

    def fake_imwrite(path, frame):
        assert frame is not None
        return True

    def fake_send_photo(path, caption):
        assert os.path.exists(path)
        sent.append(caption)
        return True

    monkeypatch.setattr(monitor, "CameraHandler", FakeCamera)
    monkeypatch.setattr(monitor, "YOLO_AVAILABLE", False)
    monkeypatch.setattr(monitor.cv2, "imwrite", fake_imwrite)
    monkeypatch.setattr(monitor, "send_telegram_photo", fake_send_photo)

    security_monitor = monitor.SecurityMonitor()

    assert security_monitor.camera.read_count == 1
    assert sent == ["📸 Security robot camera started"]
