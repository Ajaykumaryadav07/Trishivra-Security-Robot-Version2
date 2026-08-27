#!/usr/bin/env python3
"""
YOLO Training Script for Security Robot V2
Train custom YOLOv8 model for person/weapon/fire detection
"""

import os
import sys
from pathlib import Path
from ultralytics import YOLO
import argparse

def train_yolo(dataset_path, epochs=100, imgsz=320, device=0):
    """
    Train YOLO v8 nano model on custom dataset
    
    Args:
        dataset_path: Path to dataset.yaml
        epochs: Training epochs
        imgsz: Image size (320 recommended for Pi)
        device: GPU device (0) or 'cpu'
    """
    
    print("\n" + "="*60)
    print("  YOLO V8 TRAINING - Security Robot V2")
    print("="*60)
    
    # Check dataset exists
    dataset_yaml = Path(dataset_path)
    if not dataset_yaml.exists():
        print(f"❌ Dataset file not found: {dataset_path}")
        print("\n📋 Expected structure:")
        print("""
dataset/
├── images/
│   ├── train/     (person, weapon, fire images)
│   └── val/       (test images)
└── labels/
    ├── train/     (.txt annotations)
    └── val/       (.txt annotations)
        """)
        return False
    
    print(f"✅ Dataset found: {dataset_path}")
    
    # Load pre-trained YOLOv8n (nano)
    print("\n📥 Loading YOLOv8 Nano model...")
    model = YOLO('yolov8n.pt')
    
    # Train
    print("\n🚀 Starting training...")
    print(f"   📊 Epochs: {epochs}")
    print(f"   🖼️  Image size: {imgsz}x{imgsz}")
    print(f"   💻 Device: {'GPU (CUDA)' if device == 0 else 'CPU'}")
    print()
    
    results = model.train(
        data=str(dataset_yaml),
        epochs=epochs,
        imgsz=imgsz,
        device=device,
        patience=20,
        save=True,
        exist_ok=False,
        pretrained=True,
        batch=16,  # Adjust based on GPU memory
        workers=4,
        optimizer='SGD',
        lr0=0.01,
        momentum=0.937,
        weight_decay=0.0005,
        close_mosaic=10,
        hsv_h=0.015,
        hsv_s=0.7,
        hsv_v=0.4,
        degrees=10.0,
        translate=0.1,
        scale=0.5,
        flipud=0.5,
        fliplr=0.5,
        mosaic=1.0,
        copy_paste=0.0,
    )
    
    print("\n✅ Training completed!")
    print(f"\n📁 Results saved to: runs/detect/train/")
    
    # Export to OpenVINO
    print("\n🔄 Exporting to OpenVINO format...")
    best_model_path = 'runs/detect/train/weights/best.pt'
    
    if os.path.exists(best_model_path):
        model = YOLO(best_model_path)
        export_info = model.export(format='openvino', imgsz=320)
        
        print(f"\n✅ OpenVINO model exported!")
        print(f"   📦 Location: runs/detect/train/weights/best_openvino_model/")
        print(f"   📄 Files: best.xml, best.bin, best_openvino_model.yaml")
        
        # Copy to robot models directory
        import shutil
        dest_dir = 'models/best_openvino_model'
        if os.path.exists(dest_dir):
            shutil.rmtree(dest_dir)
        
        shutil.copytree(
            'runs/detect/train/weights/best_openvino_model',
            dest_dir
        )
        print(f"\n📍 Copied to: {dest_dir}/")
        
        return True
    else:
        print(f"\n❌ Best model not found at {best_model_path}")
        return False


def create_dummy_dataset():
    """Create a minimal test dataset structure"""
    
    print("\n" + "="*60)
    print("  Creating Dummy Dataset Structure")
    print("="*60)
    
    # Create directory structure
    os.makedirs('test_dataset/images/train', exist_ok=True)
    os.makedirs('test_dataset/images/val', exist_ok=True)
    os.makedirs('test_dataset/labels/train', exist_ok=True)
    os.makedirs('test_dataset/labels/val', exist_ok=True)
    
    # Create dataset.yaml
    yaml_content = """path: test_dataset
train: images/train
val: images/val

nc: 3
names: ['person', 'weapon', 'fire']
"""
    
    with open('test_dataset/dataset.yaml', 'w') as f:
        f.write(yaml_content)
    
    print("✅ Dataset structure created")
    print("""
📁 test_dataset/
   ├── images/
   │   ├── train/     (add your images here)
   │   └── val/
   ├── labels/
   │   ├── train/     (add .txt annotations here)
   │   └── val/
   └── dataset.yaml
    
📌 Next steps:
   1. Add images to test_dataset/images/train/
   2. Create .txt label files in test_dataset/labels/train/
      Format: <class_id> <x_center> <y_center> <width> <height> (normalized 0-1)
   3. Run: python3 train_yolo.py test_dataset/dataset.yaml
    """)
    
    return True


def check_dependencies():
    """Check if required packages are installed"""
    
    print("\n" + "="*60)
    print("  Checking Dependencies")
    print("="*60)
    
    required = {
        'ultralytics': 'YOLOv8 framework',
        'opencv-python': 'Image processing',
        'numpy': 'Numerical computing',
        'torch': 'PyTorch backend (for training)',
        'torchvision': 'Vision utilities',
    }
    
    missing = []
    for package, description in required.items():
        try:
            __import__(package.replace('-', '_'))
            print(f"✅ {package:20} - {description}")
        except ImportError:
            print(f"❌ {package:20} - {description}")
            missing.append(package)
    
    if missing:
        print(f"\n📥 Install missing packages:")
        print(f"   pip install {' '.join(missing)}")
        return False
    
    print("\n✅ All dependencies installed!")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Train YOLOv8 for Security Robot",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Check dependencies
  python3 train_yolo.py --check-deps
  
  # Create dummy dataset structure
  python3 train_yolo.py --create-dataset
  
  # Train on custom dataset
  python3 train_yolo.py test_dataset/dataset.yaml --epochs 100 --gpu
  
  # Train on CPU
  python3 train_yolo.py test_dataset/dataset.yaml --epochs 50 --cpu
        """
    )
    
    parser.add_argument('dataset', nargs='?', 
                       help='Path to dataset.yaml file')
    parser.add_argument('--epochs', type=int, default=100,
                       help='Number of training epochs (default: 100)')
    parser.add_argument('--imgsz', type=int, default=320,
                       help='Image size (default: 320)')
    parser.add_argument('--gpu', action='store_true',
                       help='Use GPU device 0')
    parser.add_argument('--cpu', action='store_true',
                       help='Use CPU only')
    parser.add_argument('--check-deps', action='store_true',
                       help='Check required dependencies')
    parser.add_argument('--create-dataset', action='store_true',
                       help='Create dummy dataset structure')
    
    args = parser.parse_args()
    
    # Check dependencies
    if args.check_deps:
        check_dependencies()
        return
    
    # Create dataset structure
    if args.create_dataset:
        create_dummy_dataset()
        return
    
    # Train model
    if not args.dataset:
        print("❌ Please provide dataset path")
        print("   Example: python3 train_yolo.py test_dataset/dataset.yaml")
        parser.print_help()
        return
    
    # Determine device
    device = 0 if args.gpu else ('cpu' if args.cpu else 0)
    
    # Train
    success = train_yolo(
        args.dataset,
        epochs=args.epochs,
        imgsz=args.imgsz,
        device=device
    )
    
    if success:
        print("\n" + "="*60)
        print("✅ READY FOR DEPLOYMENT")
        print("="*60)
        print("""
Next steps:
1. Copy model to Raspberry Pi:
   scp -r models/best_openvino_model/ pi@raspberrypi:/home/pi/security_robot_v2/models/

2. Update CMakeLists.txt with OpenVINO support

3. Recompile C++ project:
   cd security_robot_v2
   mkdir -p build && cd build
   cmake ..
   make -j4

4. Run on robot:
   ./security_robot
        """)
    else:
        print("\n❌ Training failed or export incomplete")
        sys.exit(1)


if __name__ == '__main__':
    main()
