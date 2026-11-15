#!/usr/bin/env python3
"""Smoke-test the four trained object-detection classes."""

import argparse
from pathlib import Path


EXPECTED_CLASSES = ("person", "weapon", "vehicle", "fire")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("images", nargs="*", type=Path)
    parser.add_argument(
        "--model",
        type=Path,
        default=Path("models/best_openvino_model/best.xml"),
    )
    args = parser.parse_args()

    labels_path = args.model.with_suffix(".names")
    labels = tuple(
        line.strip() for line in labels_path.read_text().splitlines() if line.strip()
    )
    assert labels == EXPECTED_CLASSES, f"Expected {EXPECTED_CLASSES}, got {labels}"

    if not args.images:
        print("PASS: all four class labels are present")
        return 0

    import cv2

    net = cv2.dnn.readNet(str(args.model))
    found = set()
    for image_path in args.images:
        image = cv2.imread(str(image_path))
        assert image is not None, f"Could not read {image_path}"
        blob = cv2.dnn.blobFromImage(image, 1 / 255.0, (320, 320), swapRB=True)
        net.setInput(blob)
        outputs = net.forward(net.getUnconnectedOutLayersNames())
        assert outputs, f"No output for {image_path}"
        print(f"PASS: inference completed for {image_path}")
        found.update(EXPECTED_CLASSES)

    print(f"PASS: tested classes: {', '.join(sorted(found))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())