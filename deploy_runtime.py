# -*- coding: utf-8 -*-
# 构建后自动部署：把运行所需的数据文件 + OpenCV / MySQL 运行时 DLL 复制到 exe 所在目录，
# 这样在 Qt Creator 里点“运行”时能直接找到图片、dbconfig.ini 和动态库。
#
# 用法：python deploy_runtime.py <目标目录>   （由 .pro 的 QMAKE_POST_LINK 自动调用）
import os
import sys
import shutil

SRC_DIR = os.path.dirname(os.path.abspath(__file__))   # 项目源码目录（含图片、dbconfig.ini）
MSYS_BIN = r"C:\msys64\mingw64\bin"                     # OpenCV 及其依赖 DLL 所在目录
MYSQL_LIB = r"C:\Program Files\MySQL\MySQL Server 8.0\lib"  # libmysql.dll 所在目录

# 运行时要读取的数据文件（代码里按相对路径读）
DATA_FILES = ["CT.jpg", "Tumor.jpg", "Tumor_proced.jpg", "dbconfig.ini"]

# 三个 OpenCV 模块 + 它们的全部依赖（imgcodecs 会拉入 png/jpeg/tiff/webp 等图像库）
OPENCV_DLLS = [
    "libopencv_core-500.dll", "libopencv_imgproc-500.dll", "libopencv_imgcodecs-500.dll",
    "libopencv_flann-500.dll", "libopencv_geometry-500.dll", "libtbb12.dll", "zlib1.dll",
    "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
    "libpng16-16.dll", "libjpeg-8.dll", "libtiff-6.dll", "libwebp-7.dll",
    "libwebpdemux-2.dll", "libwebpmux-3.dll", "libopenjp2-7.dll", "libopenjph-0.32.dll",
    "libavif-16.dll", "libOpenEXR-3_5.dll", "libOpenEXRCore-3_5.dll", "libIex-3_5.dll",
    "libIlmThread-3_5.dll", "libImath-3_2.dll", "libjxl.dll", "libjxl_threads.dll", "libjxl_cms.dll",
    "libaom.dll", "libdav1d-7.dll", "libSvtAv1Enc-4.dll", "librav1e.dll", "libyuv.dll",
    "libsharpyuv-0.dll", "libhwy.dll", "libLerc.dll", "liblcms2-2.dll", "libdeflate.dll",
    "liblzma-5.dll", "libzstd.dll", "libbrotlicommon.dll", "libbrotlidec.dll",
    "libbrotlienc.dll", "libjbig-0.dll",
]

# MySQL C API 运行时
MYSQL_DLLS = ["libmysql.dll"]


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "."
    target = os.path.abspath(target)
    os.makedirs(target, exist_ok=True)

    copied = 0

    def do_copy(src, dst):
        nonlocal copied
        if os.path.exists(src):
            shutil.copy2(src, dst)
            copied += 1
        else:
            print("  缺少:", src)

    for name in DATA_FILES:
        do_copy(os.path.join(SRC_DIR, name), os.path.join(target, name))
    for name in OPENCV_DLLS:
        do_copy(os.path.join(MSYS_BIN, name), os.path.join(target, name))
    for name in MYSQL_DLLS:
        do_copy(os.path.join(MYSQL_LIB, name), os.path.join(target, name))

    print("deploy_runtime.py -> %s (%d 个文件)" % (target, copied))


if __name__ == "__main__":
    main()
