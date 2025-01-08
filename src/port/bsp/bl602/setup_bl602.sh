#!/bin/bash

# 设置相对路径
export SDK_PATH="../../bsp/bl_iot_sdk" # BL602 SDK 的根路径
export TOOLCHAIN_PATH="../../bsp/bl_iot_sdk/toolchain/riscv/Linux" # 工具链的路径
export PATH="$TOOLCHAIN_PATH/bin:$PATH" # 将工具链的路径添加到环境变量 PATH 中
export PROJECT_PATH="../../bsp/bl_iot_sdk/customer_app/get-start/helloworld" # helloworld 工程项目的路径

# 测试用绝对路径
#export SDK_PATH="/home/wjy/st-device-sdk-c-ref/bsp/bl_iot_sdk" # BL602 SDK 的根路径
#export TOOLCHAIN_PATH="/home/wjy/st-device-sdk-c-ref/bsp/bl_iot_sdk/toolchain/riscv/Linux" # 工具链 toolchain 的路径，主要指向 riscv64-unknown-elf-gcc 编译器
#export PATH="$TOOLCHAIN_PATH/bin:$PATH" # 将工具链的路径添加到环境变量 PATH 中
#export PROJECT_PATH="/home/wjy/st-device-sdk-c-ref/bsp/bl_iot_sdk/customer_app/get-start/helloworld" # helloworld 工程项目的路径

# 打印环境变量的路径
echo "SDK_PATH: $SDK_PATH"
echo "TOOLCHAIN_PATH: $TOOLCHAIN_PATH"
echo "PROJECT_PATH: $PROJECT_PATH"

# 安装必要的工具
install_dependencies() {
    echo "开始安装必要的工具..."
    sudo apt-get update
    sudo apt-get install -y make gtkterm
    if [ $? -ne 0 ]; then
        echo "安装失败，请确保安装过程中没有错误。"
        exit 1
    else
        echo "安装完成。"
    fi
}

# 检查并设置 SDK 路径
setup_sdk() {
    if [ ! -d "$SDK_PATH" ]; then
        echo "错误：SDK 未安装在 $SDK_PATH，请手动下载并解压到该路径。"
        echo "正在克隆最新代码..."
        git clone https://github.com/bouffalolab/bl_iot_sdk.git "$SDK_PATH"
        cd "$SDK_PATH"
        git remote set-url origin https://github.com/bouffalolab/bl_iot_sdk
        git fetch
        git pull
    else
        echo "SDK 已找到。"
        cd "$SDK_PATH"
        if git rev-parse --is-inside-work-tree &> /dev/null; then
            echo "当前目录是一个 Git 仓库。"
            if git remote -v | grep -q "https://github.com/bouffalolab/bl_iot_sdk" &> /dev/null; then
                echo "远程仓库正确。"
                git fetch
                if [ "$(git rev-parse @)" != "$(git rev-parse @{u})" ]; then
                    echo "代码不是最新的。"
                    echo "正在拉取最新代码..."
                    git pull
                else
                    echo "代码已经是最新版本。"
                fi
            else
                echo "远程仓库不正确。请确保远程仓库指向 https://github.com/bouffalolab/bl_iot_sdk。"
                echo "正在克隆最新代码..."
                git clone https://github.com/bouffalolab/bl_iot_sdk.git "$SDK_PATH"
                cd "$SDK_PATH"
                git remote set-url origin https://github.com/bouffalolab/bl_iot_sdk
                git fetch
                git pull
            fi
        else
            echo "当前目录不是一个 Git 仓库。"
            echo "正在克隆最新代码..."
            git clone https://github.com/bouffalolab/bl_iot_sdk.git "$SDK_PATH"
            cd "$SDK_PATH"
            git remote set-url origin https://github.com/bouffalolab/bl_iot_sdk
            git fetch
            git pull
        fi
    fi
}

# 检查项目路径
check_project_path() {
    if [ ! -d "$PROJECT_PATH" ]; then
        echo "错误：项目路径 $PROJECT_PATH 不存在，请创建项目目录。"
        exit 1
    else
        echo "项目路径已找到。"
    fi
}

# 检查 Makefile 存在
check_makefile() {
    if [ ! -f "$PROJECT_PATH/Makefile" ]; then
        echo "错误：未找到 Makefile 文件。请确保项目路径中包含 Makefile 文件。"
        exit 1
    else
        echo "Makefile 文件已找到。"
    fi
}

# 检查 project.mk 文件
check_project_mk() {
    local project_mk_path="$SDK_PATH/make_scripts_riscv/project.mk"
    if [ ! -f "$project_mk_path" ]; then
        echo "错误：未找到 project.mk 文件。请手动检查 $SDK_PATH/make_scripts_riscv/project.mk 是否存在。"
        exit 1
    else
        echo "project.mk 文件已找到。"
    fi
}

# 修正 Makefile 中的路径配置
fix_makefile_path() {
    local makefile_path="$PROJECT_PATH/Makefile"
    if [ ! -f "$makefile_path" ]; then
        echo "错误：未找到 Makefile 文件。请确保项目路径中包含 Makefile 文件。"
        exit 1
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
        exit 1
    else
        echo "编译完成。"
    fi
}

# 主程序
install_dependencies
setup_sdk
check_project_path
check_makefile
check_project_mk
fix_makefile_path

build_project

echo "BL602 开发环境设置完成。"