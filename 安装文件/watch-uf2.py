from __future__ import annotations

import argparse
import shutil
import sys
import time
from pathlib import Path


UF2_PREFIXES = [
    "0.",
    "1.",
    "2.",
    "3.",
    "4.",
]


def drive_root(letter: str) -> Path:
    return Path(f"{letter.upper()}:\\")


def drive_exists(letter: str) -> bool:
    return drive_root(letter).exists()


def find_source_file(script_root: Path, prefix: str) -> Path:
    matches = sorted(
        path for path in script_root.iterdir()
        if path.is_file()
        and path.suffix.lower() == ".uf2"
        and path.name.startswith(prefix)
    )

    if not matches:
        raise FileNotFoundError(f"No UF2 file found for prefix {prefix!r} in {script_root}")

    return matches[0]


def wait_for_drive(letter: str, should_exist: bool, poll_interval: float) -> None:
    while drive_exists(letter) != should_exist:
        time.sleep(poll_interval)


def wait_for_drive_with_timeout(letter: str, should_exist: bool, timeout: float, poll_interval: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if drive_exists(letter) == should_exist:
            return True
        time.sleep(poll_interval)
    return drive_exists(letter) == should_exist


def format_progress(current_step: int, total_steps: int, total_completed: int, cycle_count: int) -> str:
    width = 20
    percent = current_step / total_steps
    filled = int(width * percent)
    bar = "#" * filled + "-" * (width - filled)
    return f"Cycle {cycle_count} [{bar}] {current_step}/{total_steps} | Total completed: {total_completed}"


def print_progress(current_step: int, total_steps: int, total_completed: int, cycle_count: int) -> None:
    sys.stdout.write("\r" + format_progress(current_step, total_steps, total_completed, cycle_count) + " " * 8)
    sys.stdout.flush()


def copy_with_retry(source: Path, target: Path, target_drive: str, poll_interval: float, retry_timeout: float) -> None:
    deadline = time.monotonic() + retry_timeout
    last_error: Exception | None = None

    while time.monotonic() <= deadline:
        if not drive_exists(target_drive):
            time.sleep(poll_interval)
            continue

        try:
            shutil.copy2(source, target)
            if not target.exists():
                raise OSError(f"copy finished but target missing: {target}")
            return
        except Exception as exc:  # noqa: BLE001
            last_error = exc
            time.sleep(poll_interval)

    if last_error is None:
        raise TimeoutError(f"Target drive {target_drive}: not available for copying {source.name}")
    raise TimeoutError(f"Failed to copy {source.name} to {target_drive}:\\ within {retry_timeout} seconds") from last_error


def main() -> int:
    parser = argparse.ArgumentParser(description="Watch E drive and copy UF2 files to E drive sequentially.")
    parser.add_argument("--device-drive", default="E", help="Drive letter used for both detection and writing")
    parser.add_argument("--poll-interval", type=float, default=0.25, help="Polling interval in seconds")
    parser.add_argument("--reconnect-timeout", type=float, default=1.0, help="Timeout waiting for the device drive to reappear")
    parser.add_argument("--copy-retry-timeout", type=float, default=10.0, help="Timeout retrying a single file copy")
    args = parser.parse_args()

    script_root = Path(__file__).resolve().parent
    try:
        source_files = [find_source_file(script_root, prefix) for prefix in UF2_PREFIXES]
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    total_completed = 0
    cycle_count = 0
    total_steps = len(UF2_PREFIXES)

    device_drive = args.device_drive.upper()

    print(f"Waiting for {device_drive} drive...")

    while True:
        wait_for_drive(device_drive, True, args.poll_interval)
        cycle_count += 1
        print(f"Detected {device_drive} drive. Start cycle {cycle_count}.")

        for index, source in enumerate(source_files, start=1):
            destination = drive_root(device_drive) / source.name

            wait_for_drive(device_drive, True, args.poll_interval)
            print_progress(index, total_steps, total_completed, cycle_count)

            print(f"\n[{cycle_count}] Copying step {index}/{total_steps}: {source.name}")
            copy_with_retry(source, destination, device_drive, args.poll_interval, args.copy_retry_timeout)
            total_completed += 1

            print_progress(index, total_steps, total_completed, cycle_count)

            wait_for_drive(device_drive, False, args.poll_interval)

            if index < total_steps:
                if not wait_for_drive_with_timeout(
                    device_drive,
                    True,
                    args.reconnect_timeout,
                    args.poll_interval,
                ):
                    print(
                        f"\nWarning: {device_drive} drive did not reappear within {args.reconnect_timeout:.1f}s.",
                        file=sys.stderr,
                    )
                    wait_for_drive(device_drive, True, args.poll_interval)

        print()
        print(f"Cycle {cycle_count} complete. Total completed: {total_completed}")
        print(f"Waiting for next {device_drive} insertion...")


if __name__ == "__main__":
    raise SystemExit(main())