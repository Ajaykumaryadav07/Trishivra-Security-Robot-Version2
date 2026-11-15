"""
Telegram Bot for Security Robot V2

Commands:
- /start   → Welcome message
- /stop    → Pause robot
- /resume  → Resume patrol
- /status  → Get robot status
- /patrol  → Start patrol mode
- /photo   → Capture and send photo
- /help    → Show commands

Integration:
- Reads status from /tmp/robot_status.json (written by C++ main)
- Sends commands via /tmp/robot_command.txt (read by C++ main)
- Sends photos via Telegram API
"""

from __future__ import annotations

import os
import sys
import json
import time
import logging
import threading
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any
from python.constants import STATUS_FILE, COMMAND_FILE, ALERT_FILE
from python.detection_state import read_detection_state, write_command
from python.qa_system import get_answer, get_random_greeting, add_question
from .camera_handler import CameraHandler


try:
    from telegram import Update, Bot
    from telegram.ext import Application, CommandHandler, ContextTypes, MessageHandler, filters
    TELEGRAM_AVAILABLE = True
except ImportError:
    TELEGRAM_AVAILABLE = False
    print("⚠️ python-telegram-bot not installed. Install: pip install python-telegram-bot")

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Project paths
PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_FILE = PROJECT_ROOT / "config" / "robot_settings.json"
YAML_CONFIG_FILE = PROJECT_ROOT / "config" / "robot_settings.yaml"


# ============================================================
# Configuration
# ============================================================

def _load_json_config(file_path: Path) -> Dict[str, Any]:
    """Load config from JSON file if it exists."""
    try:
        if file_path.exists():
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                return data if isinstance(data, dict) else {}
    except Exception as e:
        logger.warning(f"⚠️ Could not read JSON config {file_path}: {e}")
    return {}


def _load_yaml_config(file_path: Path) -> Dict[str, Any]:
    """Load config from YAML file if it exists."""
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
# Status Reader
# ============================================================

def read_robot_status() -> Dict[str, Any]:
    """Read robot status from JSON file (written by C++ main)."""
    try:
        if os.path.exists(STATUS_FILE):
            with open(STATUS_FILE, 'r') as f:
                return json.load(f)
    except Exception as e:
        logger.error(f"❌ Failed to read status: {e}")
    return {
        "state": "UNKNOWN",
        "x": 0, "y": 0, "theta": 0,
        "battery": 0, "person_count": 0,
        "weapon_count": 0, "fire_count": 0,
        "cpu": 0, "fps": 0, "paused": False,
        "obstacle": False, "temperature": 0
    }

def send_command_to_robot(command: str, value: str = ""):
    """Send command to robot via file (read by C++ main)."""
    try:
        with open(COMMAND_FILE, 'w') as f:
            if value:
                f.write(f"{command} {value}\n")
            else:
                f.write(f"{command}\n")
        logger.info(f"📤 Command sent: {command} {value}")
        return True
    except Exception as e:
        logger.error(f"❌ Failed to send command: {e}")
        return False

# ============================================================
# Camera Capture
# ============================================================

def capture_photo(save_path: Optional[str] = None) -> Optional[str]:
    """Capture photo from camera."""
    try:
        import cv2
        import tempfile

        if save_path is None:
            save_path = os.path.join(tempfile.gettempdir(), "telegram_photo.jpg")

        camera = CameraHandler(fps=10)
        ret, frame = camera.read()
        camera.release()
        
        if ret and frame is not None:
            cv2.imwrite(save_path, frame)
            return save_path
        else:
            logger.error("❌ Failed to capture photo")
            return None
    except Exception as e:
        logger.error(f"❌ Camera error: {e}")
        return None

# ============================================================
# Telegram Photo Sender
# ============================================================

def send_telegram_photo(photo_path: str, caption: str = "") -> bool:
    """
    Send photo to Telegram chat.
    Uses the synchronous HTTP sender so it can be called by both async handlers
    and the background alert monitor.
    """
    try:
        from .send_telegram_photo import send_telegram_photo as send_photo_http
        return send_photo_http(photo_path, caption)
    except Exception as e:
        logger.error(f"❌ Failed to send photo: {e}")
        return False

# ============================================================
# Telegram Bot Handlers
# ============================================================



async def handle_text_message(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle text messages (not commands)"""
    text = update.message.text.lower().strip()
    
    # Check if it's a known command
    if text in ['stop', 'pause']:
        await stop_command(update, context)
        return
    elif text in ['resume', 'continue']:
        await resume_command(update, context)
        return
    elif text in ['status', 'state']:
        await status_command(update, context)
        return
    elif text in ['patrol', 'start']:
        await patrol_command(update, context)
        return
    elif text in ['photo', 'snap', 'image']:
        await photo_command(update, context)
        return
    elif text in ['help', 'commands']:
        await help_command(update, context)
        return
    
    # Check Q&A system for answer
    answer = get_answer(text)
    
    # Send response
    await update.message.reply_text(answer, parse_mode='Markdown')





async def start_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /start command."""
    welcome_msg = (
        "🤖 **TRISHIVRA Security Robot V2**\n\n"
        "Commands:\n"
        "/start - Show this message\n"
        "/stop - Pause the robot\n"
        "/resume - Resume patrol\n"
        "/status - Get robot status\n"
        "/patrol - Start patrol mode\n"
        "/photo - Capture and send photo\n"
        "/help - Show commands\n\n"
        "🔋 System Ready\n"
        "📍 BBDITM Lucknow"
    )
    await update.message.reply_text(welcome_msg, parse_mode='Markdown')


async def stop_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /stop command."""
    send_command_to_robot("STOP")
    await update.message.reply_text("⏸️ Robot paused")


async def resume_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /resume command."""
    send_command_to_robot("RESUME")
    await update.message.reply_text("▶️ Robot resumed")


async def patrol_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /patrol command."""
    send_command_to_robot("PATROL")
    await update.message.reply_text("🚀 Patrol mode started")


async def status_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /status command."""
    status = read_robot_status()
    
    # Format status message
    state_emoji = {
        "IDLE": "💤",
        "PATROL": "🚀",
        "MAPPING": "🗺️",
        "THREAT_DETECTED": "⚠️",
        "EMERGENCY_STOP": "🚨",
        "RETURN_HOME": "🏠",
        "OBSTACLE_AVOID": "🚧",
        "PAUSED": "⏸️",
        "SHUTDOWN": "🔴"
    }
    state = status.get("state", "UNKNOWN")
    emoji = state_emoji.get(state, "❓")
    
    msg = (
        f"{emoji} **Robot Status**\n\n"
        f"State: {state}\n"
        f"📍 Position: ({status.get('x', 0):.2f}, {status.get('y', 0):.2f})\n"
        f"🧭 Heading: {status.get('theta', 0):.2f}°\n"
        f"🔋 Battery: {status.get('battery', 0):.1f}V\n"
        f"👤 Persons: {status.get('person_count', 0)}\n"
        f"🔫 Weapons: {status.get('weapon_count', 0)}\n"
        f"🔥 Fires: {status.get('fire_count', 0)}\n"
        f"📊 CPU: {status.get('cpu', 0):.1f}%\n"
        f"🌡️ Temp: {status.get('temperature', 0):.1f}°C\n"
        f"📸 FPS: {status.get('fps', 0)}\n"
        f"⏸️ Paused: {'Yes' if status.get('paused', False) else 'No'}\n"
        f"🚧 Obstacle: {'Yes' if status.get('obstacle', False) else 'No'}"
    )
    await update.message.reply_text(msg, parse_mode='Markdown')


async def photo_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /photo command."""
    await update.message.reply_text("📸 Capturing photo...")
    
    photo_path = capture_photo()
    if photo_path and os.path.exists(photo_path):
        caption = f"📸 Camera Snapshot\n🕐 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        if send_telegram_photo(photo_path, caption):
            await update.message.reply_text("✅ Photo sent!")
        else:
            await update.message.reply_text("❌ Failed to send photo")
    else:
        await update.message.reply_text("❌ Failed to capture photo")


async def help_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle /help command."""
    help_msg = (
        "🤖 **Available Commands**\n\n"
        "/start - Welcome message\n"
        "/stop - Pause the robot\n"
        "/resume - Resume patrol\n"
        "/status - Get robot status\n"
        "/patrol - Start patrol mode\n"
        "/photo - Capture and send photo\n"
        "/help - Show this message\n\n"
        "📱 You can also send:\n"
        "- 'stop' to pause\n"
        "- 'resume' to continue\n"
        "- 'status' for status"
    )
    await update.message.reply_text(help_msg, parse_mode='Markdown')


async def text_handler(update: Update, context: ContextTypes.DEFAULT_TYPE):
    """Handle text messages (not commands)."""
    text = update.message.text.lower().strip()
    
    if text in ['stop', 'pause']:
        await stop_command(update, context)
    elif text in ['resume', 'continue']:
        await resume_command(update, context)
    elif text in ['status', 'state']:
        await status_command(update, context)
    elif text in ['patrol', 'start']:
        await patrol_command(update, context)
    elif text in ['photo', 'snap', 'image']:
        await photo_command(update, context)
    else:
        await update.message.reply_text(
            f"🤖 I don't understand: '{text}'\nType /help for commands"
        )


# ============================================================
# Main Bot Runner
# ============================================================

def start_telegram_bot():
    """Start the Telegram bot."""
    if not TELEGRAM_AVAILABLE:
        logger.error("❌ python-telegram-bot not installed")
        return None
    
    if BOT_TOKEN in ("YOUR_BOT_TOKEN", "", None):
        logger.error("❌ Telegram bot token not configured")
        return None

    if CHAT_ID in ("YOUR_CHAT_ID", "", None):
        logger.error("❌ Telegram chat ID not configured")
        return None

    try:
        # Create application
        app = Application.builder().token(BOT_TOKEN).build()

        # Add command handlers
        app.add_handler(CommandHandler("start", start_command))
        app.add_handler(CommandHandler("stop", stop_command))
        app.add_handler(CommandHandler("resume", resume_command))
        app.add_handler(CommandHandler("status", status_command))
        app.add_handler(CommandHandler("patrol", patrol_command))
        app.add_handler(CommandHandler("photo", photo_command))
        app.add_handler(CommandHandler("help", help_command))

        # Add one text handler for non-command messages
        app.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, text_handler))

        # Start polling
        logger.info("🤖 Telegram bot started")
        app.run_polling(allowed_updates=["message", "callback_query"])

    except Exception as e:
        logger.error(f"❌ Telegram bot error: {e}")
        return None


def run_bot_in_background():
    """Run Telegram bot in a background thread."""
    thread = threading.Thread(target=start_telegram_bot, daemon=True)
    thread.start()
    return thread


# ============================================================
# Alert Monitor (for triggering alerts from C++)
# ============================================================

def monitor_alerts():
    """
    Monitor alert file for triggers from C++ main.
    Runs in background to send alerts to Telegram.
    """
    logger.info("📡 Alert monitor started")
    
    last_alert_time = 0
    alert_cooldown = 5  # seconds
    
    while True:
        try:
            # Check alert file
            if os.path.exists(ALERT_FILE):
                with open(ALERT_FILE, 'r') as f:
                    content = f.read().strip()
                
                # Clear file
                os.remove(ALERT_FILE)
                
                if content and (time.time() - last_alert_time) > alert_cooldown:
                    # Parse alert
                    lines = content.split('\n')
                    alert_type = lines[0] if lines else "ALERT"
                    conf = lines[1] if len(lines) > 1 else ""
                    photo_file = lines[2] if len(lines) > 2 and "File:" in lines[2] else None
                    
                    if photo_file:
                        photo_file = photo_file.replace("File: ", "").strip()
                    else:
                        # Capture photo
                        photo_file = capture_photo()
                    
                    # Send alert
                    caption = f"🚨 **{alert_type}**\n🕐 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
                    if conf:
                        caption += f"\n📊 {conf}"
                    
                    if photo_file and os.path.exists(photo_file):
                        send_telegram_photo(photo_file, caption)
                    else:
                        # Send text alert
                        if BOT_TOKEN not in ("YOUR_BOT_TOKEN", "", None) and CHAT_ID not in ("YOUR_CHAT_ID", "", None):
                            try:
                                from .send_telegram_photo import send_telegram_message
                                send_telegram_message(caption)
                            except Exception:
                                logger.warning("⚠️ Alert text message failed to send")
                        else:
                            logger.warning("⚠️ Telegram alert skipped: token or chat ID not configured")
                    
                    last_alert_time = time.time()
                    logger.info(f"📨 Alert sent: {alert_type}")
            
        except Exception as e:
            logger.error(f"❌ Alert monitor error: {e}")
        
        time.sleep(0.5)


def run_alert_monitor_in_background():
    """Run alert monitor in a background thread."""
    thread = threading.Thread(target=monitor_alerts, daemon=True)
    thread.start()
    return thread


# ============================================================
# Main Entry Point
# ============================================================

if __name__ == "__main__":
    print("🤖 Starting Telegram Bot...")
    
    # Start alert monitor
    run_alert_monitor_in_background()
    
    # Start bot
    start_telegram_bot()