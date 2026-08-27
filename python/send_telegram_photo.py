"""
Telegram Photo Sender Utility for Security Robot V2

Sends photos and text messages via Telegram Bot API.
Used by:
- Telegram Bot (telegram_bot.py)
- Alert Monitor (monitor.py)
- C++ main (via file trigger)
"""

import os
import json
import time
import logging
try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ============================================================
# Configuration Loading
# ============================================================

PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_FILE = PROJECT_ROOT / "config" / "robot_settings.json"
YAML_CONFIG_FILE = PROJECT_ROOT / "config" / "robot_settings.yaml"


def _load_json_config(file_path: Path) -> Dict[str, Any]:
    """Load config from a JSON file if it exists."""
    try:
        if file_path.exists():
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                return data if isinstance(data, dict) else {}
    except Exception as e:
        logger.warning(f"⚠️ Could not read JSON config {file_path}: {e}")
    return {}


def _load_yaml_config(file_path: Path) -> Dict[str, Any]:
    """Load config from a YAML file if it exists."""
    try:
        import yaml
    except ImportError:
        return {}

    try:
        if file_path.exists():
            with open(file_path, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f) or {}
                return data if isinstance(data, dict) else {}
    except Exception as e:
        logger.warning(f"⚠️ Could not read YAML config {file_path}: {e}")
    return {}


def load_config() -> Dict[str, Any]:
    """Load configuration from JSON/YAML files and merge them."""
    merged: Dict[str, Any] = {}

    yaml_config = _load_yaml_config(YAML_CONFIG_FILE)
    json_config = _load_json_config(CONFIG_FILE)

    merged.update(yaml_config)
    merged.update(json_config)

    if isinstance(merged.get("telegram"), dict):
        for key, value in merged["telegram"].items():
            merged.setdefault(key, value)

    return merged


config = load_config()


def get_telegram_credentials() -> tuple[str, str]:
    """Resolve Telegram credentials from env vars or config files."""
    telegram_cfg = config.get("telegram", {}) if isinstance(config.get("telegram"), dict) else {}

    bot_token = (
        os.getenv("TELEGRAM_BOT_TOKEN")
        or os.getenv("BOT_TOKEN")
        or config.get("telegram_token")
        or config.get("bot_token")
        or telegram_cfg.get("bot_token")
        or telegram_cfg.get("token")
        or "YOUR_BOT_TOKEN"
    )
    chat_id = (
        os.getenv("TELEGRAM_CHAT_ID")
        or os.getenv("CHAT_ID")
        or config.get("telegram_chat_id")
        or config.get("chat_id")
        or telegram_cfg.get("chat_id")
        or config.get("telegram_id")
        or "YOUR_CHAT_ID"
    )

    bot_token = str(bot_token).strip() if bot_token is not None else "YOUR_BOT_TOKEN"
    chat_id = str(chat_id).strip() if chat_id is not None else "YOUR_CHAT_ID"
    return bot_token, chat_id


BOT_TOKEN, CHAT_ID = get_telegram_credentials()

# ============================================================
# Photo Sending Functions
# ============================================================

def send_telegram_photo(
    photo_path: str,
    caption: str = "",
    parse_mode: str = "Markdown",
    timeout: int = 10
) -> bool:
    """
    Send a photo to Telegram chat.

    Args:
        photo_path: Path to the photo file
        caption: Optional caption for the photo
        parse_mode: "Markdown" or "HTML"
        timeout: Request timeout in seconds

    Returns:
        True if successful, False otherwise
    """
    # Validate configuration
    if BOT_TOKEN in ("YOUR_BOT_TOKEN", "", None):
        logger.error("❌ Telegram bot token not configured")
        return False

    if CHAT_ID in ("YOUR_CHAT_ID", "", None):
        logger.error("❌ Telegram chat ID not configured")
        return False

    # Validate photo path
    if not os.path.exists(photo_path):
        logger.error(f"❌ Photo not found: {photo_path}")
        return False

    # Build URL
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendPhoto"

    if not REQUESTS_AVAILABLE:
        logger.error("❌ requests library not installed. Install: pip install requests")
        return False

    try:
        # Open and send photo
        with open(photo_path, "rb") as photo:
            files = {"photo": photo}
            data = {
                "chat_id": CHAT_ID,
                "caption": caption[:1024],  # Telegram caption limit
                "parse_mode": parse_mode
            }

            response = requests.post(url, files=files, data=data, timeout=timeout)

            # Check response
            if response.status_code == 200:
                logger.info(f"📸 Photo sent: {os.path.basename(photo_path)}")
                return True
            else:
                logger.error(f"❌ Failed to send photo: {response.status_code}")
                logger.debug(f"Response: {response.text}")
                return False

    except requests.exceptions.Timeout:
        logger.error(f"❌ Timeout sending photo: {timeout}s")
        return False
    except requests.exceptions.ConnectionError:
        logger.error("❌ Connection error to Telegram API")
        return False
    except Exception as e:
        logger.error(f"❌ Error sending photo: {e}")
        return False


def send_telegram_message(
    text: str,
    parse_mode: str = "Markdown",
    timeout: int = 10
) -> bool:
    """
    Send a text message to Telegram chat.

    Args:
        text: Message text
        parse_mode: "Markdown" or "HTML"
        timeout: Request timeout in seconds

    Returns:
        True if successful, False otherwise
    """
    # Validate configuration
    if BOT_TOKEN in ("YOUR_BOT_TOKEN", "", None):
        logger.error("❌ Telegram bot token not configured")
        return False

    if CHAT_ID in ("YOUR_CHAT_ID", "", None):
        logger.error("❌ Telegram chat ID not configured")
        return False

    # Build URL
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

    try:
        data = {
            "chat_id": CHAT_ID,
            "text": text[:4096],  # Telegram message limit
            "parse_mode": parse_mode
        }

        response = requests.post(url, json=data, timeout=timeout)

        # Check response
        if response.status_code == 200:
            logger.info(f"📨 Message sent: {text[:50]}...")
            return True
        else:
            logger.error(f"❌ Failed to send message: {response.status_code}")
            logger.debug(f"Response: {response.text}")
            return False

    except requests.exceptions.Timeout:
        logger.error(f"❌ Timeout sending message: {timeout}s")
        return False
    except requests.exceptions.ConnectionError:
        logger.error("❌ Connection error to Telegram API")
        return False
    except Exception as e:
        logger.error(f"❌ Error sending message: {e}")
        return False


def send_telegram_alert(
    alert_type: str,
    message: str,
    photo_path: Optional[str] = None,
    extra_data: Optional[Dict[str, Any]] = None
) -> bool:
    """
    Send a formatted alert to Telegram.

    Args:
        alert_type: Type of alert (e.g., "PERSON_DETECTED", "WEAPON_DETECTED")
        message: Alert message
        photo_path: Optional photo to send
        extra_data: Optional extra data to include

    Returns:
        True if successful, False otherwise
    """
    # Build caption with icon based on alert type
    icons = {
        "PERSON_DETECTED": "👤",
        "WEAPON_DETECTED": "🔫",
        "FIRE_DETECTED": "🔥",
        "EMERGENCY": "🚨",
        "OBSTACLE": "🚧",
        "BATTERY_LOW": "🔋⚠️",
        "ERROR": "❌",
        "INFO": "ℹ️"
    }
    icon = icons.get(alert_type.upper(), "⚠️")

    caption = f"{icon} **{alert_type.replace('_', ' ').title()}**\n"
    caption += f"📝 {message}\n"
    caption += f"🕐 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"

    # Add extra data if provided
    if extra_data:
        for key, value in extra_data.items():
            caption += f"\n📊 {key}: {value}"

    # Send photo or just message
    if photo_path and os.path.exists(photo_path):
        return send_telegram_photo(photo_path, caption)
    else:
        return send_telegram_message(caption)


# ============================================================
# Helper Functions
# ============================================================

def send_startup_message() -> bool:
    """Send startup notification to Telegram and capture one startup snapshot."""
    msg = (
        "🤖 **TRISHIVRA Security Robot V2**\n\n"
        "✅ System Started\n"
        f"🕐 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n"
        "📱 Use /help for commands\n"
        "🌐 Type any command to control the robot"
    )

    message_sent = send_telegram_message(msg)

    try:
        from python.telegram_bot import capture_photo
        photo_path = capture_photo()
        if photo_path and os.path.exists(photo_path):
            caption = (
                "📸 **Robot camera startup check**\n\n"
                f"🕐 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
            )
            photo_sent = send_telegram_photo(photo_path, caption)
            return message_sent or photo_sent
    except Exception as e:
        logger.warning(f"⚠️ Startup photo not sent: {e}")

    return message_sent


def send_status_report(status: Dict[str, Any]) -> bool:
    """Send a formatted status report to Telegram."""
    msg = (
        "📊 **Robot Status Report**\n\n"
        f"State: {status.get('state', 'UNKNOWN')}\n"
        f"📍 Position: ({status.get('x', 0):.2f}, {status.get('y', 0):.2f})\n"
        f"🧭 Heading: {status.get('theta', 0):.2f}°\n"
        f"🔋 Battery: {status.get('battery', 0):.1f}V\n"
        f"👤 Persons: {status.get('person_count', 0)}\n"
        f"🔫 Weapons: {status.get('weapon_count', 0)}\n"
        f"🔥 Fires: {status.get('fire_count', 0)}\n"
        f"📊 CPU: {status.get('cpu', 0):.1f}%\n"
        f"🌡️ Temp: {status.get('temperature', 0):.1f}°C\n"
        f"📸 FPS: {status.get('fps', 0)}"
    )
    return send_telegram_message(msg)


# ============================================================
# Test Functions
# ============================================================

def test_send_photo():
    """Test photo sending by capturing a test image."""
    try:
        # Create a test image. Prefer OpenCV if available, otherwise use Pillow.
        try:
            import cv2
            import numpy as np
            test_image = np.zeros((480, 640, 3), dtype=np.uint8)
            cv2.putText(test_image, "Test Photo", (200, 240),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 255, 0), 3)
            cv2.putText(test_image, "Security Robot V2", (180, 300),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

            test_path = "/tmp/test_photo.jpg"
            cv2.imwrite(test_path, test_image)
        except Exception:
            try:
                from PIL import Image, ImageDraw, ImageFont
                img = Image.new('RGB', (640, 480), color=(40, 40, 40))
                draw = ImageDraw.Draw(img)
                try:
                    f = ImageFont.load_default()
                except Exception:
                    f = None
                draw.text((200, 240), "Test Photo", fill=(0,255,0), font=f)
                draw.text((180, 300), "Security Robot V2", fill=(255,255,255), font=f)
                test_path = "/tmp/test_photo.jpg"
                img.save(test_path, format='JPEG')
            except Exception as e:
                logger.error(f"❌ Failed to create test image: {e}")
                return False

        # Send test photo
        return send_telegram_photo(test_path, "🧪 Test Photo")
    except Exception as e:
        logger.error(f"❌ Test failed: {e}")
        return False


def test_send_message():
    """Test message sending."""
    return send_telegram_message("🧪 Test message from Security Robot V2")


# ============================================================
# Main Entry Point
# ============================================================

if __name__ == "__main__":
    print("📨 Telegram Photo Sender Test")
    print("=" * 40)

    print("\n📤 Sending test message...")
    if test_send_message():
        print("✅ Message sent successfully!")
    else:
        print("❌ Failed to send message")

    print("\n📸 Sending test photo...")
    if test_send_photo():
        print("✅ Photo sent successfully!")
    else:
        print("❌ Failed to send photo")

    print("\n✅ Test complete")