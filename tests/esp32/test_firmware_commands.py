import shutil
import subprocess
from pathlib import Path

import pytest


TEST_DIR = Path(__file__).resolve().parent


def test_firmware_drive_commands_compile_and_set_expected_pwm(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is required to run the ESP32 host-side firmware test")

    executable = tmp_path / "firmware_command_harness"
    compile_result = subprocess.run(
        [
            compiler,
            "-std=c++17",
            f"-I{TEST_DIR / 'arduino_mock'}",
            str(TEST_DIR / "firmware_command_harness.cpp"),
            "-o",
            str(executable),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert compile_result.returncode == 0, compile_result.stderr

    run_result = subprocess.run(
        [str(executable)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert run_result.returncode == 0, run_result.stdout + run_result.stderr
