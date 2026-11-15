from pathlib import Path

import cv2
import numpy as np
import pytest

from python import monitor
from src.ai.yolo_openvino import YOLOOpenVINO


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MODEL_XML = PROJECT_ROOT / "models" / "best_openvino_model" / "best.xml"
MODEL_ONNX = PROJECT_ROOT / "models" / "best.onnx"
MODEL_NAMES = PROJECT_ROOT / "models" / "best_openvino_model" / "best.names"
EXPECTED_CLASSES = ["person", "weapon", "vehicle", "fire"]


def test_python_detector_decodes_custom_classes_and_rescales_boxes():
    detector = object.__new__(YOLOOpenVINO)
    detector.input_size = 320
    detector.confidence_threshold = 0.5
    detector.class_names = EXPECTED_CLASSES

    output = np.zeros((1, 8, 2), dtype=np.float32)
    output[0, :4, 0] = [160, 160, 80, 40]
    output[0, 5, 0] = 0.9
    output[0, :4, 1] = [80, 80, 40, 40]
    output[0, 7, 1] = 0.8

    detections = detector.postprocess(output, (480, 640))

    assert [item["class"] for item in detections] == ["weapon", "fire"]
    assert detections[0]["bbox"] == [240, 210, 400, 270]
    assert detections[1]["bbox"] == [120, 90, 200, 150]


def test_python_detector_preprocesses_camera_frames_as_rgb():
    detector = object.__new__(YOLOOpenVINO)
    detector.input_size = 2
    frame = np.zeros((2, 2, 3), dtype=np.uint8)
    frame[:, :] = [255, 0, 0]

    blob = detector.preprocess(frame)

    assert blob.shape == (1, 3, 2, 2)
    assert np.all(blob[0, 0] == 0)
    assert np.all(blob[0, 2] == 1)


def test_monitor_uses_checked_in_openvino_model():
    assert Path(monitor.YOLO_MODEL_XML) == MODEL_XML
    assert Path(monitor.YOLO_MODEL_BIN) == MODEL_XML.with_suffix(".bin")
    assert monitor.YOLO_IMG_SIZE == 640
    assert MODEL_XML.is_file()
    assert MODEL_XML.with_suffix(".bin").is_file()
    assert [line.strip() for line in MODEL_NAMES.read_text().splitlines()] == EXPECTED_CLASSES


def test_checked_in_onnx_model_runs_inference():
    if not MODEL_ONNX.is_file():
        pytest.skip("ONNX model is not present")

    try:
        network = cv2.dnn.readNet(str(MODEL_ONNX))
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        blob = cv2.dnn.blobFromImage(
            frame, 1 / 255.0, (640, 640), swapRB=True
        )
        network.setInput(blob)
        outputs = network.forward(network.getUnconnectedOutLayersNames())
    except cv2.error as error:
        pytest.fail(f"Checked-in ONNX model could not run inference: {error}")

    assert outputs
    assert outputs[0].shape == (1, 4 + len(EXPECTED_CLASSES), 8400)


def test_missing_detector_does_not_fabricate_detections():
    instance = object.__new__(monitor.SecurityMonitor)
    instance.detector = None

    detections = instance.detect(np.zeros((32, 32, 3), dtype=np.uint8))

    assert detections == []
