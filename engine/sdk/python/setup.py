# P1 多模态嵌入式 SDK Python 绑定
"""
P1 多模态嵌入式 SDK Python 绑定

注意：
- 运行时需要 MinGW GCC 的运行时 DLL（libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll）
- 这些 DLL 应放在 _core.pyd 同目录下，或加入系统 PATH
"""
from __future__ import annotations
import sys
import os
import subprocess
import sysconfig
from pathlib import Path

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

# 项目根目录
ROOT = Path(__file__).resolve().parent.parent.parent.parent

# pybind11 头文件路径
PYBIND11_INCLUDE = subprocess.check_output(
    [sys.executable, "-c", "import pybind11; print(pybind11.get_include())"],
    text=True
).strip()

# 预编译的 mmsdk 静态库路径
LIB_DIR = ROOT / "build" / "engineering" / "engineering" / "src" / "sdk"
# SQLite 编译产物
SQLITE_DIR = ROOT / "build" / "engineering" / "sdk_sqlite3_build"

# MinGW GCC 路径
MINGW_GPP = r"C:\mingw64\bin\g++.exe"

# Python 库路径
PYTHON_LIB_DIR = Path(sysconfig.get_config_var("LIBDIR"))
PYTHON_VERSION = f"{sys.version_info.major}{sys.version_info.minor}"


def is_mingw_available():
    return Path(MINGW_GPP).exists()


class BuildExtension(build_ext):
    """自定义构建命令：Windows 下用 MinGW GCC 直接编译扩展"""

    def build_extension(self, ext):
        extdir = Path(self.get_ext_fullpath(ext.name)).parent.absolute()
        extdir.mkdir(parents=True, exist_ok=True)

        if sys.platform == "win32" and is_mingw_available():
            self._build_with_mingw(ext, extdir)
        else:
            super().build_extension(ext)

    def _build_with_mingw(self, ext, extdir):
        """使用 MinGW GCC 直接编译扩展"""
        print("[pymultimodal] 使用 MinGW GCC 编译扩展...")

        python_include = sysconfig.get_path("include")

        sources = [str(ROOT / s) for s in ext.sources]
        include_dirs = [str(d) for d in ext.include_dirs] + [python_include]
        library_dirs = [str(d) for d in ext.library_dirs] + [str(PYTHON_LIB_DIR)]
        libraries = ext.libraries or []

        output_file = str(extdir / "_core.pyd")

        cmd = [
            MINGW_GPP,
            "-shared",
            "-o", output_file,
            *sources,
            *[f"-I{d}" for d in include_dirs],
            *[f"-L{d}" for d in library_dirs],
            *[f"-l{lib}" for lib in libraries],
            f"-lpython{PYTHON_VERSION}",
            "-std=c++17",
            "-D_hypot=hypot",
            "-O3",
            "-DNDEBUG",
        ]

        print(f"[pymultimodal] 编译命令: {' '.join(cmd)}")
        subprocess.check_call(cmd)
        print(f"[pymultimodal] 扩展已生成: {output_file}")


# pybind11 扩展模块
ext_modules = [
    Extension(
        "pymultimodal._core",
        sources=["pymultimodal/binding.cpp"],
        include_dirs=[
            str(ROOT / "include"),
            str(ROOT / "third_part" / "sqlite3"),
            PYBIND11_INCLUDE,
        ],
        library_dirs=[
            str(LIB_DIR),
            str(SQLITE_DIR),
        ],
        libraries=["mmsdk", "sqlite3"],
        language="c++",
        extra_compile_args=["-std=c++17"] if sys.platform != "win32" else [],
        extra_link_args=[],
    ),
]

setup(
    name="pymultimodal",
    version="0.1.0",
    description="P1 多模态嵌入式 SDK Python 绑定",
    packages=["pymultimodal"],
    ext_modules=ext_modules,
    cmdclass={"build_ext": BuildExtension},
    python_requires=">=3.9",
    package_data={"pymultimodal": ["*.pyi"]},
)