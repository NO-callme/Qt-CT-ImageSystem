# 远程医疗 CT 影像管理系统

基于 **Qt 6 + OpenCV + MySQL** 的 Windows 桌面应用：病人与病历的全流程增删改查，以及 CT 影像的读取、灰度化、高斯模糊与霍夫圆病灶检测标注。

> 说明：影像处理（霍夫圆检测）是真实执行的 OpenCV 代码；「疑似子宫肌瘤」等诊断结论文案为界面演示用的固定文案，并非 AI 自动诊断。

## 技术栈

- C++17 · Qt 6 (Widgets)
- OpenCV 5
- MySQL 8（直接使用 C API libmysql，而非 Qt 的 QSqlDatabase）
- 构建：qmake + MinGW；部署：`deploy_runtime.py`

## 目录结构

```
Telemedicine-repro/
├── main.cpp            入口：读配置 → 连库 → 建表 → 启动主窗口
├── mainwindow.h/.cpp   主窗口：影像处理 + 病人/病历 CRUD
├── mainwindow.ui       Qt Designer 界面
├── db.h / db.cpp       MySQL C API 访问层封装
├── dbconfig.ini.example  数据库连接配置模板
├── deploy_runtime.py   构建后自动拷贝运行时 DLL 与数据文件
├── 3rdparty/libmysql/  MinGW 用的 libmysql 导入库（gendef + dlltool 生成）
├── CT.jpg / Tumor.jpg  影像数据
└── 面试题.md           项目相关面试题整理
```

## 运行前准备

1. **数据库配置**：复制 `dbconfig.ini.example` 为 `dbconfig.ini`，填入你自己的 MySQL 账号密码。
2. **建库**：程序启动时会自动 `CREATE DATABASE IF NOT EXISTS telemedicine_repro` 并建表，无需手动建库。
3. **依赖**：`3rdparty/libmysql/libmysql.a` 已提供 MinGW 可用的导入库；运行时还需 `libmysql.dll`（由 `deploy_runtime.py` 从本机 MySQL 目录自动拷贝）。

## 构建

用 **Qt Creator** 打开 `Telemedicine.pro` 直接构建运行；或命令行：

```bash
qmake Telemedicine.pro
mingw32-make
```

构建完成后 `QMAKE_POST_LINK` 会自动执行 `deploy_runtime.py`，把 OpenCV / MySQL 运行时 DLL 与数据文件拷贝到 exe 同目录，实现一键运行。
