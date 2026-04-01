#!/usr/bin/env bash
# esp.sh — build / clean / flash / monitor helper for ESP-IDF projects
#
# Usage:
#   ./esp.sh build
#   ./esp.sh flash          (autodetects port)
#   ./esp.sh flash /dev/ttyUSB0
#   ./esp.sh monitor        (autodetects port)
#   ./esp.sh monitor /dev/ttyACM0
#   ./esp.sh clean
#   ./esp.sh rebuild        (clean + build)
#   ./esp.sh all            (clean + build + flash + monitor)

set -euo pipefail

IDF_PATH="$HOME/.espressif/v5.5.3/esp-idf"
ESP_TARGET="esp32c3"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── ESP-IDF environment ───────────────────────────────────────────────────────
setup_idf() {
    if [ -z "${IDF_TOOLS_PATH:-}" ]; then
        echo "[esp.sh] Activating ESP-IDF ${IDF_PATH##*/v} ..."
        # Pipe would run export.sh in a subshell, losing PATH changes.
        # Redirect both streams to /dev/null and source directly instead.
        # shellcheck disable=SC1091
        source "$IDF_PATH/export.sh" >/dev/null 2>&1
    fi
}

# ── Port autodetection ────────────────────────────────────────────────────────
find_port() {
    local port=""

    # Prefer /dev/serial/by-id symlinks (stable across reboots).
    for link in /dev/serial/by-id/*; do
        [ -e "$link" ] || continue
        port="$(readlink -f "$link")"
        break
    done

    # Fall back to ttyUSB* then ttyACM*.
    if [ -z "$port" ]; then
        for candidate in /dev/ttyUSB* /dev/ttyACM*; do
            [ -e "$candidate" ] || continue
            port="$candidate"
            break
        done
    fi

    if [ -z "$port" ]; then
        echo "[esp.sh] ERROR: No serial device found." >&2
        echo "         Plug in the ESP32-C3 and try again, or pass the port explicitly:" >&2
        echo "         $0 flash /dev/ttyUSB0" >&2
        exit 1
    fi

    echo "$port"
}

# ── Commands ──────────────────────────────────────────────────────────────────
cmd_build() {
    setup_idf
    # Set target if sdkconfig is missing (e.g. after a clean or first checkout).
    if [ ! -f "$SCRIPT_DIR/sdkconfig" ]; then
        echo "[esp.sh] No sdkconfig found — setting target to $ESP_TARGET ..."
        idf.py -C "$SCRIPT_DIR" set-target "$ESP_TARGET"
    fi
    echo "[esp.sh] Building ..."
    idf.py -C "$SCRIPT_DIR" build
}

cmd_clean() {
    # Use rm directly — idf.py fullclean fails when the build dir doesn't exist,
    # which would abort the whole script under set -e.
    echo "[esp.sh] Cleaning build directory ..."
    rm -rf "$SCRIPT_DIR/build"
}

cmd_flash() {
    local port="${1:-}"
    [ -z "$port" ] && port="$(find_port)"
    setup_idf
    echo "[esp.sh] Flashing → $port"
    idf.py -C "$SCRIPT_DIR" -p "$port" flash
}

cmd_monitor() {
    local port="${1:-}"
    [ -z "$port" ] && port="$(find_port)"
    setup_idf
    echo "[esp.sh] Monitor on $port  (Ctrl-] to exit)"
    idf.py -C "$SCRIPT_DIR" -p "$port" monitor
}

cmd_flash_monitor() {
    local port="${1:-}"
    [ -z "$port" ] && port="$(find_port)"
    setup_idf
    echo "[esp.sh] Flashing → $port"
    idf.py -C "$SCRIPT_DIR" -p "$port" flash monitor
}

# ── Dispatch ──────────────────────────────────────────────────────────────────
ACTION="${1:-help}"
PORT="${2:-}"

case "$ACTION" in
    build)
        cmd_build
        ;;
    clean)
        cmd_clean
        ;;
    rebuild)
        cmd_clean
        cmd_build
        ;;
    flash)
        cmd_flash "$PORT"
        ;;
    monitor)
        cmd_monitor "$PORT"
        ;;
    flash-monitor)
        cmd_flash_monitor "$PORT"
        ;;
    all)
        cmd_clean
        cmd_build
        cmd_flash_monitor "$PORT"
        ;;
    help|--help|-h)
        cat <<EOF
Usage: $0 <command> [port]

Commands:
  build                   Compile the firmware
  clean                   Remove the build directory
  rebuild                 clean + build
  flash        [port]     Flash firmware (autodetects port if omitted)
  monitor      [port]     Open serial monitor
  flash-monitor [port]    Flash then immediately open monitor
  all          [port]     clean + build + flash + monitor

Port autodetection order:
  1. /dev/serial/by-id/*  (stable symlinks)
  2. /dev/ttyUSB*
  3. /dev/ttyACM*

Examples:
  $0 build
  $0 flash
  $0 flash /dev/ttyUSB0
  $0 all
  $0 monitor /dev/ttyACM0
EOF
        ;;
    *)
        echo "[esp.sh] Unknown command: $ACTION" >&2
        echo "         Run '$0 help' for usage." >&2
        exit 1
        ;;
esac
