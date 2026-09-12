#!/usr/bin/env python3
"""SignGenerator 串口命令行控制台。

用法：
    python3 tools/serial_console.py -p /dev/ttyUSB0 -b 115200
    python3 tools/serial_console.py --list          # 列出可用串口

单键操作，不需要回车：q 启停波形、w 循环切换波形、e 减频、r 加频，大写同样
有效；Ctrl-C 或 Ctrl-D 退出。串口收到的固件日志会原样打印在终端上，所以按一下
键就能立刻看到 RUN/STOP 和频率的变化。

依赖 pyserial：pip install pyserial
"""

from __future__ import annotations

import argparse
import os
import select
import sys
import termios
import tty

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("需要 pyserial：pip install pyserial")

# 按键 -> 发送的字节，键为小写（固件同时接受大写）
KEYS = {
    "q": ("q", "启停波形"),
    "w": ("w", "切换波形"),
    "e": ("e", "频率 -"),
    "r": ("r", "频率 +"),
}

EXIT_KEYS = {"\x03", "\x04"}  # Ctrl-C / Ctrl-D


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SignGenerator 串口命令行控制台",
        epilog="按键：q 启停 / w 切换波形 / e 减频 / r 加频，Ctrl-C 退出",
    )
    parser.add_argument(
        "-p", "--port", default="/dev/ttyUSB0",
        help="串口设备（默认 /dev/ttyUSB0，板载 CH340）",
    )
    parser.add_argument(
        "-b", "--baud", type=int, default=115200,
        help="波特率（默认 115200）",
    )
    parser.add_argument("--list", action="store_true", help="列出可用串口后退出")
    return parser.parse_args()


def print_ports() -> None:
    # 板载 CH340 枚举成 /dev/ttyUSB*；ttyS* 是主板自带串口，列出来只会干扰定位。
    ports = [
        port for port in list_ports.comports()
        if port.device.startswith(("/dev/ttyUSB", "/dev/ttyACM"))
    ]
    if not ports:
        print("没有找到 USB 串口设备（/dev/ttyUSB* 或 /dev/ttyACM*）。")
        print("Type-C 插入后可用 dmesg | tail 确认是否识别到 CH340。")
        return
    for port in ports:
        print(f"{port.device}\t{port.description}")


def pump_serial(link: serial.Serial) -> None:
    """把固件的日志输出原样转发到终端。"""
    data = link.read(max(1, link.in_waiting))
    if data:
        sys.stdout.write(data.decode("ascii", "replace"))
        sys.stdout.flush()


def run(link: serial.Serial) -> None:
    print(f"已连接 {link.port} @ {link.baudrate} 8N1")
    print("q 启停 / w 切换波形 / e 减频 / r 加频（大写同样有效，无需回车），Ctrl-C 退出\n")

    fd = sys.stdin.fileno()
    saved_attrs = termios.tcgetattr(fd)
    try:
        # 单字符读取并关闭本地回显，这样按键不用等回车；回显由本脚本自己打印。
        tty.setcbreak(fd)

        while True:
            readable, _, _ = select.select([fd, link.fileno()], [], [])

            if fd in readable:
                key = os.read(fd, 1).decode("ascii", "replace")
                if key in EXIT_KEYS:
                    break

                action = KEYS.get(key.lower())
                if action is None:
                    # 方向键等转义序列里的可打印字符也会走到这里，直接忽略即可。
                    if key.isprintable():
                        print(f"[{key}] 未识别，可用按键：q w e r")
                    continue

                link.write(action[0].encode())
                print(f"[{key.lower()}] {action[1]}")

            if link.fileno() in readable:
                pump_serial(link)
    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved_attrs)

    print("\n已退出。")


def main() -> int:
    args = parse_args()

    if args.list:
        print_ports()
        return 0

    try:
        link = serial.Serial(args.port, args.baud, timeout=0)
    except serial.SerialException as error:
        print(f"打开 {args.port} 失败：{error}", file=sys.stderr)
        print("请检查：1) 设备名是否正确（用 --list 查看，板载 CH340 通常是 /dev/ttyUSB0）；",
              file=sys.stderr)
        print("        2) Type-C 是否插好（dmesg | tail 可看到 ttyUSB 的插入记录）；",
              file=sys.stderr)
        print("        3) 当前用户是否在 dialout 组（加入后需要重新登录）。",
              file=sys.stderr)
        return 1

    with link:
        run(link)

    return 0


if __name__ == "__main__":
    sys.exit(main())
