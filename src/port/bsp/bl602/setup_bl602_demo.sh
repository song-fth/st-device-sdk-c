#!/bin/bash

# 设置临时环境变量
export SDK_PATH="/wjy/20241202/st-device-sdk-c-ref-develop/bsp/bl_iot_sdk-dev_602" # BL602 SDK 的根路径
export TOOLCHAIN_PATH="/wjy/20241202/st-device-sdk-c-ref-develop/bsp/bl_iot_sdk-dev_602/toolchain/riscv/Linux" # 工具链 toolchain 的路径，主要指向 riscv64-unknown-elf-gcc 编译器
export PATH="$TOOLCHAIN_PATH/bin:$PATH" # 将工具链的路径添加到环境变量 PATH 中
export PROJECT_PATH="/wjy/20241202/st-device-sdk-c-ref-develop/bsp/bl_iot_sdk-dev_602/customer_app/get-start/helloworld" # hellowrld 工程项目的路径
#export BL60X_SDK_PATH="/wjy/20241202/st-device-sdk-c-ref-develop/bsp/bl_iot_sdk-dev_602" # BL602 SDK 的路径
export COMPONENT_PATH="$SDK_PATH/components/platform/hosal" # 指定存放项目中各个模块或组件的路径
#export FLASH_TOOL_PATH="$SDK_PATH/tools/flash_tool"

# 检查工具链路径
check_toolchain_path() {
    if [ ! -d "$TOOLCHAIN_PATH" ]; then
        echo "错误：工具链路径 $TOOLCHAIN_PATH 不存在。请检查路径是否正确。"
        return 1
    else
        echo "工具链路径已找到。"
    fi
}

# 检查 riscv64-unknown-elf-gcc
check_toolchain_executable() {
    if ! command -v "$TOOLCHAIN_PATH/bin/riscv64-unknown-elf-gcc" &> /dev/null; then
        echo "错误：未找到 riscv64-unknown-elf-gcc 工具链，请检查工具链是否安装在 $TOOLCHAIN_PATH 并设置了环境变量。"
        return 1
    else
        echo "riscv64-unknown-elf-gcc 已找到。"
    fi
}

# 检查 SDK 路径
check_sdk_path() {
    if [ ! -d "$SDK_PATH" ]; then
        echo "错误：SDK 未安装在 $SDK_PATH，请手动下载并解压到该路径。"
        return 1
    else
        echo "SDK 已找到。"
    fi
}

# 检查项目路径
check_project_path() {
    if [ ! -d "$PROJECT_PATH" ]; then
        echo "错误：项目路径 $PROJECT_PATH 不存在，请创建项目目录。"
        return 1
    else
        echo "项目路径已找到。"
    fi
}

# 检查 Makefile 存在
check_makefile() {
    if [ ! -f "$PROJECT_PATH/Makefile" ]; then
        echo "错误：未找到 Makefile 文件。请确保项目路径中包含 Makefile 文件。"
        return 1
    else
        echo "Makefile 文件已找到。"
    fi
}

# 检查 project.mk 文件
check_project_mk() {
    local project_mk_path="$SDK_PATH/make_scripts_riscv/project.mk"
    if [ ! -f "$project_mk_path" ]; then
        echo "错误：未找到 project.mk 文件。请手动检查 $SDK_PATH/make_scripts_riscv/project.mk 是否存在。"
        return 1
    else
        echo "project.mk 文件已找到。"
    fi
}

# 修正 Makefile 中的路径配置
fix_makefile_path() {
    local makefile_path="$PROJECT_PATH/Makefile"
    if [ ! -f "$makefile_path" ]; then
        echo "错误：未找到 Makefile 文件。请确保项目路径中包含 Makefile 文件。"
        return 1
    fi

    if grep -q "PROJECT_MK_PATH" "$makefile_path"; then
        echo "Makefile 中已包含 PROJECT_MK_PATH，无需修改。"
    else
        echo "在 Makefile 中添加项目路径配置..."
        sed -i '/^PROJECT_INCLUDES/a PROJECT_MK_PATH = $(SDK_PATH)/make_scripts_riscv' "$makefile_path"
        echo "Makefile 路径配置已修改。"
    fi
}

# 执行构建
build_project() {
    cd "$PROJECT_PATH"
    echo "开始编译..."
    make CONFIG_CHIP_NAME=BL602 CONFIG_LINK_ROM=1 -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "编译失败。"
        return 1
    else
        echo "编译完成。"
    fi
}

# 执行烧录
#flash_project() {
#    if ! command -v "$FLASH_TOOL_PATH/bflb_iot_tool" &> /dev/null; then
#        echo "错误：未找到烧录工具，请确保安装了烧录工具并设置了环境变量。"
#        return 1
#    fi
#
#    echo "开始烧录..."
#    "$FLASH_TOOL_PATH/bflb_iot_tool" --chipname=BL602 --interface=uart --port=/dev/ttyUSB0 --baudrate=115200 --dts="$SDK_PATH/make_scripts_riscv/project.dts" --pt="$SDK_PATH/make_scripts_riscv/partition_table.toml" --firmware="$PROJECT_PATH/build_out/helloworld.bin"
#    echo "烧录完成。"
#}

# 主程序
check_toolchain_path || exit 1
check_toolchain_executable || exit 1
check_sdk_path || exit 1
check_project_path || exit 1
check_makefile || exit 1
check_project_mk || exit 1
fix_makefile_path || exit 1

build_project || exit 1
#flash_project || exit 1

echo "BL602 开发环境设置完成。"
