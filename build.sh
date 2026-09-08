#!/usr/bin/env bash

set -Eeuo pipefail

readonly REPO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_DIR="${REPO_DIR}/stm32proj"
readonly TOOLCHAIN_FILE="${SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake"

build_type="Debug"
clean_build=false
jobs="${SIGNGEN_BUILD_JOBS:-}"

usage() {
    cat <<'EOF'
用法：./build.sh [Debug|Release] [--clean] [--jobs 数量]

选项：
  Debug|Release  选择构建类型，默认为 Debug
  --clean        删除对应构建目录后重新配置
  --jobs N       并行编译任务数；也可设置 SIGNGEN_BUILD_JOBS
  -h, --help     显示帮助
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
        --clean)
            clean_build=true
            ;;
        --jobs)
            if (($# < 2)); then
                echo "错误：--jobs 缺少任务数。" >&2
                exit 2
            fi
            jobs="$2"
            shift
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

if [[ -z "${jobs}" ]]; then
    if command -v nproc >/dev/null 2>&1; then
        jobs="$(nproc)"
    else
        jobs="1"
    fi
fi

if [[ ! "${jobs}" =~ ^[1-9][0-9]*$ ]]; then
    echo "错误：并行任务数必须是正整数，当前值为 '${jobs}'。" >&2
    exit 2
fi

for required_command in \
    cmake \
    make \
    arm-none-eabi-gcc \
    arm-none-eabi-g++ \
    arm-none-eabi-objcopy \
    arm-none-eabi-size; do
    if ! command -v "${required_command}" >/dev/null 2>&1; then
        echo "错误：未找到 '${required_command}'，请先安装 GNU Arm 工具链和 CMake/Make。" >&2
        exit 127
    fi
done

readonly BUILD_DIR="${REPO_DIR}/build/${build_type}"

if [[ "${clean_build}" == true ]]; then
    case "${BUILD_DIR}" in
        "${REPO_DIR}/build/Debug"|"${REPO_DIR}/build/Release")
            cmake -E remove_directory "${BUILD_DIR}"
            ;;
        *)
            echo "错误：拒绝清理非预期目录 '${BUILD_DIR}'。" >&2
            exit 2
            ;;
    esac
elif [[ -f "${BUILD_DIR}/CMakeCache.txt" && \
        ! -f "${BUILD_DIR}/CMakeFiles/Makefile.cmake" ]]; then
    echo "检测到未完成的 ${build_type} 配置，正在重新创建构建目录。"
    cmake -E remove_directory "${BUILD_DIR}"
fi

if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cached_generator="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' \
        "${BUILD_DIR}/CMakeCache.txt")"
    if [[ "${cached_generator}" != "Unix Makefiles" ]]; then
        echo "错误：${BUILD_DIR} 使用 '${cached_generator}'，本脚本使用 'Unix Makefiles'。" >&2
        echo "请执行 './build.sh ${build_type} --clean' 重新配置。" >&2
        exit 2
    fi
fi

cmake \
    -S "${SOURCE_DIR}" \
    -B "${BUILD_DIR}" \
    -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE:STRING="${build_type}"

cmake --build "${BUILD_DIR}" --parallel "${jobs}"

readonly ELF_FILE="${BUILD_DIR}/stm32proj.elf"
readonly HEX_FILE="${BUILD_DIR}/stm32proj.hex"
readonly BIN_FILE="${BUILD_DIR}/stm32proj.bin"

if [[ ! -s "${ELF_FILE}" ]]; then
    echo "错误：构建完成但未找到 '${ELF_FILE}'。" >&2
    exit 1
fi

arm-none-eabi-objcopy -O ihex "${ELF_FILE}" "${HEX_FILE}"
arm-none-eabi-objcopy -O binary "${ELF_FILE}" "${BIN_FILE}"
arm-none-eabi-size "${ELF_FILE}"

printf '\n构建完成（%s）：\n' "${build_type}"
printf '  ELF: %s\n' "${ELF_FILE}"
printf '  HEX: %s\n' "${HEX_FILE}"
printf '  BIN: %s\n' "${BIN_FILE}"
