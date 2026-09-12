# Repository Guidelines

## Project Structure & Module Organization

Firmware lives under `stm32proj/`: CubeMX-generated code is in `Core/`, HAL/CMSIS dependencies are in `Drivers/`, and hand-written modules are added to `Core/Inc` and `Core/Src`. The root `docs/` directory contains schematics and engineering notes; place measurements and report material in `docs/` or `assets/`. Keep host-side algorithm tests in `tests/` when introduced. The local course brief is intentionally ignored and must not be committed.

## Build, Test, and Development Commands

Use the repository scripts from its root:

- `./stm32proj/32build.sh` — configure and build Debug firmware, then create ELF, HEX, and BIN artifacts under `stm32proj/build/Debug/`.
- `./stm32proj/32build.sh Release --clean` — perform a fresh size-optimized build.
- `./stm32proj/32flash.sh` — rebuild, then program and verify through ST-Link/OpenOCD.
- `./stm32proj/32flash.sh --no-build --dry-run` — validate the flash command without accessing hardware.

There is not yet an automated target-side test suite. Always build both configurations and run `git -c core.whitespace=cr-at-eol diff --check` before committing; CubeMX files use CRLF. Hardware changes require oscilloscope validation after the board is available.

## Coding Style & Naming Conventions

Use C11 and four-space indentation in hand-written modules. Follow `snake_case` for functions and variables, `UPPER_SNAKE_CASE` for constants, and a module prefix such as `waveform_` or `app_ui_` for public APIs. Keep CubeMX edits inside `USER CODE BEGIN/END` blocks; put reusable logic in separate files and register them in `stm32proj/CMakeLists.txt`. Avoid heap allocation and floating-point work in time-sensitive firmware.

## Testing Guidelines

Before hardware testing, confirm clean Debug and Release builds and inspect memory usage. On hardware, verify sine, square, triangle, and sawtooth outputs, all four buttons, and displayed versus measured frequency at the minimum, maximum, and representative intermediate settings. Save oscilloscope captures and discrepancy notes under `docs/` or `assets/`.

## Commit & Pull Request Guidelines

Use Conventional Commits with an English type and a concise Chinese subject, such as `feat: 添加正弦波DMA输出`, `fix: 修正频率换算误差`, or `docs: 更新示波器测试记录`. Common types are `feat`, `fix`, `docs`, `refactor`, `test`, `build`, and `chore`. Keep commits focused. Pull requests should summarize the change, identify the board/toolchain, list validation performed, link related issues, and include waveform captures for hardware-visible changes.

## Security & Configuration

Do not commit the ignored course brief, editor lock files, student identifiers, machine-specific paths, build artifacts, or third-party source without checking its license. Record borrowed ideas and licenses in `docs/开源项目参考.md`.
