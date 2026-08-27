# install_yolo_models.py
from ultralytics import YOLO
import os, shutil

os.makedirs('models', exist_ok=True)
model = YOLO('yolov8n.pt')
model.export(format='openvino', imgsz=320, half=True)
shutil.move('yolov8n_openvino_model/yolov8n.xml', 'models/')
shutil.move('yolov8n_openvino_model/yolov8n.bin', 'models/')
shutil.rmtree('yolov8n_openvino_model')
print('✅ Models installed in models/')