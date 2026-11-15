from pathlib import Path
import argparse

from ultralytics import YOLO

parser = argparse.ArgumentParser(description='Export a trained Ultralytics model to OpenVINO.')
parser.add_argument('weights', nargs='?', default='models/best.pt')
parser.add_argument('--imgsz', type=int, default=320)
args = parser.parse_args()

weights = Path(args.weights).resolve()
if not weights.is_file():
	raise SystemExit(f'Weights file not found: {weights}')

model = YOLO(str(weights))
exported = Path(model.export(format='openvino', imgsz=args.imgsz, half=False, dynamic=False))
target = weights.parent / 'best_openvino_model'
target.mkdir(parents=True, exist_ok=True)

for suffix in ('.xml', '.bin'):
	source = exported / f'{weights.stem}{suffix}'
	if not source.is_file():
		raise RuntimeError(f'Ultralytics did not create {source}')
	source.replace(target / f'best{suffix}')

names = model.names
(target / 'best.names').write_text('\n'.join(str(names[index]) for index in range(len(names))) + '\n')
print(f'OpenVINO model written to {target}')