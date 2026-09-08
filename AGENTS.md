# Repository Guidelines

## Project Structure & Module Organization

This workspace currently contains one authoritative artifact: `二级项目任务书-报告模板-注意事项-模电实验室新位置.doc`, the project brief and report template. No firmware, test, or asset directories exist yet. The `.~...doc` file is an editor lock file and must not be included in contributions. When implementation files are added, use `src/` for firmware, `include/` for headers, `tests/` for host-side tests, `hardware/` for schematics and simulations, and `docs/` or `assets/` for report material, photographs, and measurements. Record the selected MCU, board, and toolchain in a root `README.md`.

## Build, Test, and Development Commands

There is currently no build system, automated test suite, formatter, or linter. Useful document checks are:

- `libreoffice --headless --convert-to pdf --outdir /tmp "二级项目任务书-报告模板-注意事项-模电实验室新位置.doc"` — export a review PDF outside the workspace.
- `file "二级项目任务书-报告模板-注意事项-模电实验室新位置.doc"` — confirm the legacy Word format.

Open the document in WPS Office or Microsoft Word for final layout inspection. Firmware contributions must add reproducible build, flash, lint, and test commands alongside their configuration.

## Coding Style & Naming Conventions

Preserve the document's Chinese filename and `.doc` format unless a migration is intentional. Follow its report rules: SimSun for Chinese, Times New Roman for English and numbers, 小四 size, fixed 22-point line spacing, two-character first-line indents, and justified paragraphs. Center figures with captions below; center tables with captions above; number and cite both consistently. Until a formatter is introduced, use four-space indentation for embedded code, `snake_case` for functions and variables, and `UPPER_SNAKE_CASE` for constants.

## Testing Guidelines

Validation is manual. Confirm sine, square, triangle, and sawtooth outputs; external waveform and frequency controls; and the displayed waveform/frequency. Calculate minimum, maximum, and step frequencies, measure them with an oscilloscope, and explain discrepancies. Include captures for optional noise, AM, FM, or custom features. Store evidence under `docs/` or `assets/` when those directories are introduced.

## Commit & Pull Request Guidelines

No Git history is present in this copy. Use short imperative subjects, such as `Add square-wave measurement results`, and keep commits focused. Pull requests should summarize the change, identify the board/toolchain, list validation performed, link related issues, and include before/after layout images or waveform captures. Explain binary `.doc` changes explicitly because line-based review is unavailable.

## Security & Configuration

Do not commit editor lock files, personal phone or banking details, student identifiers, machine-specific paths, or licensed tool files.
