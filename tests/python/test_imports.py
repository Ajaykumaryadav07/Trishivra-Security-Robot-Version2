import importlib

MODULES = [
    'alert_rules', 'autonomy', 'camera_handler', 'constants', 'detection_state',
    'main', 'monitor', 'obstacle_safety', 'qa_system', 'robot_state',
    'send_telegram_photo', 'sensor_fusion', 'telegram_bot', 'voice_controller'
]


def test_modules_importable():
    for name in MODULES:
        mod = importlib.import_module(f'python.{name}')
        assert hasattr(mod, '__name__')
