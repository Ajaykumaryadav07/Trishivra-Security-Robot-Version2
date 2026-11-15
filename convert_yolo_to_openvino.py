#!/usr/bin/env python3
"""Export a trained Ultralytics YOLO model for the robot's OpenVINO runtime."""

import argparse
import shutil
from pathlib import Path

from ultralytics import YOLO


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert a trained YOLO .pt model to OpenVINO IR format"
    )
    parser.add_argument(
        "model",
        nargs="?",
        default="best.pt",
        help="Path to trained YOLO weights (default: best.pt)",
    )
    parser.add_argument(
        "--output",
        default="models/best_openvino_model",
        help="Robot model directory (default: models/best_openvino_model)",
    )
    parser.add_argument(
        "--imgsz",
        type=int,
        default=320,
        help="Model input size (default: 320)",
    )
    args = parser.parse_args()

    model_path = Path(args.model).resolve()
    output_dir = Path(args.output)
    if not model_path.is_file():
        parser.error(f"model file not found: {model_path}")

    print(f"Loading trained model: {model_path}")
    model = YOLO(str(model_path))
    exported_path = Path(model.export(format="openvino", imgsz=args.imgsz))

    output_dir.parent.mkdir(parents=True, exist_ok=True)
    if output_dir.exists():
        shutil.rmtree(output_dir)
    shutil.copytree(exported_path, output_dir)

    xml_path = output_dir / f"{model_path.stem}.xml"
    bin_path = output_dir / f"{model_path.stem}.bin"
    print(f"OpenVINO model copied to: {output_dir}")
    print(f"XML: {xml_path}")
    print(f"BIN: {bin_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())