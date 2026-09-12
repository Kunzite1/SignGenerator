#!/usr/bin/env bash

set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

build_type="Debug"
build_before_flash=true
dry_run=false
adapter_speed="${SIGNGEN_ADAPTER_SPEED:-1000}"
interface_config="${SIGNGEN_OPENOCD_INTERFACE:-interface/stlink.cfg}"
target_config="${SIGNGEN_OPENOCD_TARGET:-target/stm32f1x.cfg}"

usage() {
    cat <<'EOF'
用法：./stm32proj/32flash.sh [Debug|Release] [--no-build] [--dry-run]

选项：
  Debug|Release  选择待烧录构建，默认为 Debug
  --no-build     不重新构建，直接烧录已有 ELF
  --dry-run      只检查并打印 OpenOCD 命令，不访问硬件
  -h, --help     显示帮助

环境变量：
  SIGNGEN_ADAPTER_SPEED       ST-Link SWD 速度，默认 1000（kHz）
  SIGNGEN_OPENOCD_INTERFACE  OpenOCD 接口配置，默认 interface/stlink.cfg
  SIGNGEN_OPENOCD_TARGET     OpenOCD 目标配置，默认 target/stm32f1x.cfg
EOF
}

while (($# > 0)); do
    case "$1" in
        Debug|Release)
            build_type="$1"
            ;;
        debug)
            build_type="Debug"
            ;;
        release)
            build_type="Release"
            ;;
        --no-build)
            build_before_flash=false
            ;;
        --dry-run)
            dry_run=true
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "错误：未知参数 '$1'。" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [[ ! "${adapter_speed}" =~ ^[1-9][0-9]*$ ]]; then
    echo "错误：SIGNGEN_ADAPTER_SPEED 必须是正整数，单位为 kHz。" >&2
    exit 2
fi

if [[ "${build_before_flash}" == true ]]; then
    "${SCRIPT_DIR}/32build.sh" "${build_type}"
fi

if ! command -v openocd >/dev/null 2>&1; then
    echo "错误：未找到 openocd，请先安装 OpenOCD。" >&2
    exit 127
fi

readonly ELF_FILE="${SCRIPT_DIR}/build/${build_type}/stm32proj.elf"
if [[ ! -s "${ELF_FILE}" ]]; then
    echo "错误：未找到待烧录固件 '${ELF_FILE}'。" >&2
    echo "请先运行 './stm32proj/32build.sh ${build_type}'。" >&2
    exit 1
fi

openocd_command=(
    openocd
    -f "${interface_config}"
    -f "${target_config}"
    -c "adapter speed ${adapter_speed}"
    -c "program {${ELF_FILE}} verify reset exit"
)

if [[ "${dry_run}" == true ]]; then
    printf '检查通过，将执行：\n  '
    printf '%q ' "${openocd_command[@]}"
    printf '\n'
    exit 0
fi

echo "准备烧录 ${ELF_FILE}（SWD ${adapter_speed} kHz）..."
if "${openocd_command[@]}"; then
    echo "烧录、校验和复位完成。"
else
    flash_status=$?
    cat >&2 <<'EOF'
烧录失败。请检查：
  1. ST-Link 的 SWDIO、SWCLK、GND、VTref 和 NRST 接线；
  2. 开发板已供电，且没有其他程序占用 ST-Link；
  3. 当前用户具有 USB 设备权限（不要直接用 sudo 绕过规则）；
  4. 必要时降低 SIGNGEN_ADAPTER_SPEED 后重试。
EOF
    exit "${flash_status}"
fi
