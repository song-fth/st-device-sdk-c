import os
import subprocess
import sys

# 设置相对路径
SDK_PATH = os.path.abspath("../../bsp/bl_iot_sdk")
TOOLCHAIN_PATH = os.path.abspath("../../bsp/bl_iot_sdk/toolchain/riscv/Linux")
PROJECT_PATH = os.path.abspath("../../bsp/bl_iot_sdk/customer_app/get-start/helloworld")

# 更新环境变量
os.environ["SDK_PATH"] = SDK_PATH
os.environ["TOOLCHAIN_PATH"] = TOOLCHAIN_PATH
os.environ["PATH"] = os.path.join(TOOLCHAIN_PATH, "bin") + os.pathsep + os.environ["PATH"]

# 打印环境变量的路径
print("SDK_PATH:", os.getenv("SDK_PATH"))
print("TOOLCHAIN_PATH:", os.getenv("TOOLCHAIN_PATH"))
print("PROJECT_PATH:", PROJECT_PATH)

# 安装必要的工具
def install_dependencies():
    print("开始安装必要的工具...")
    try:
        subprocess.check_call(["sudo", "apt-get", "update"])
        subprocess.check_call(["sudo", "apt-get", "install", "-y", "make", "gtkterm"])
        print("安装完成。")  
    except subprocess.CalledProcessError as e:
        print(f"安装失败：{e}")
        sys.exit(1)

# 检查并设置 SDK 路径
def setup_sdk():
    if not os.path.isdir(SDK_PATH):
        print(f"错误：SDK 未安装在 {SDK_PATH}，请手动下载并解压到该路径或运行以下命令：")
        print("git clone https://github.com/bouffalolab/bl_iot_sdk.git " + SDK_PATH)
        sys.exit(1)

    os.chdir(SDK_PATH)
    
    # 检查是否是 Git 仓库并更新
    try:
        if subprocess.call(["git", "rev-parse", "--is-inside-work-tree"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
            print("当前目录是一个 Git 仓库。")
            remote = subprocess.check_output(["git", "remote", "-v"]).decode()
            if "https://github.com/bouffalolab/bl_iot_sdk" in remote:
                print("远程仓库正确。")
                subprocess.check_call(["git", "fetch"])
                if subprocess.check_output(["git", "rev-parse", "@"]).strip() != subprocess.check_output(["git", "rev-parse", "@{u}"]).strip():
                    print("代码不是最新的，正在拉取最新代码...")
                    subprocess.check_call(["git", "pull"])
                else:
                    print("代码已经是最新版本。")
            else:
                print("远程仓库不正确。请确保远程仓库指向 https://github.com/bouffalolab/bl_iot_sdk。")
                sys.exit(1)
        else:
            print("当前目录不是一个 Git 仓库，正在克隆最新代码...")
            subprocess.check_call(["git", "clone", "https://github.com/bouffalolab/bl_iot_sdk.git", SDK_PATH])
            print("克隆完成。")
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"发生错误：{e}")
        sys.exit(1)

# 检查项目路径
def check_project_path():
    if not os.path.isdir(PROJECT_PATH):
        print(f"错误：项目路径 {PROJECT_PATH} 不存在，请创建项目目录。")
        sys.exit(1)
    print("项目路径已找到。")

# 检查 Makefile 存在
def check_makefile():
    makefile_path = os.path.join(PROJECT_PATH, "Makefile")
    if not os.path.isfile(makefile_path):
        print("错误：未找到 Makefile 文件。请确保项目路径中包含 Makefile 文件。")
        sys.exit(1)
    print("Makefile 文件已找到。")

# 检查 project.mk 文件
def check_project_mk():
    project_mk_path = os.path.join(SDK_PATH, "make_scripts_riscv", "project.mk")
    if not os.path.isfile(project_mk_path):
        print(f"错误：未找到 project.mk 文件。请手动检查 {project_mk_path} 是否存在。")
        sys.exit(1)
    print("project.mk 文件已找到。")

# 修正 Makefile 中的路径配置
def fix_makefile_path():
    makefile_path = os.path.join(PROJECT_PATH, "Makefile")
    with open(makefile_path, "r+") as f:
        content = f.read()
        if "PROJECT_MK_PATH" not in content:
            print("在 Makefile 中添加项目路径配置...")
            f.write(f"\nPROJECT_MK_PATH = $(SDK_PATH)/make_scripts_riscv\n")

# 执行构建
def build_project():
    os.chdir(PROJECT_PATH)
    print("开始编译...")
    try:
        subprocess.check_call(["make", "CONFIG_CHIP_NAME=BL602", "CONFIG_LINK_ROM=1", "-j" + str(os.cpu_count())])
        print("编译完成。")
    except subprocess.CalledProcessError:
        print("编译失败。")
        sys.exit(1)

# 主程序
if __name__ == "__main__":
    install_dependencies() 
    setup_sdk()
    check_project_path()
    check_makefile()
    check_project_mk()
    fix_makefile_path()
    build_project()

    print("BL602 开发环境设置完成。")
