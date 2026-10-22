# LiteLaunch（轻启）安装版

Windows x64中键启动面板与EVER批量重命名工具。仅发布安装版，不再分发免安装EXE。

## 下载与安装

在[正式版本](https://github.com/ccl4-56-23-5/LiteLaunch/releases/latest)下载LiteLaunch-Setup-1.0.21-x64.exe，运行安装向导。支持覆盖安装、安装目录选择、快捷方式、开机启动选项和Windows卸载入口。默认当前用户安装，无需管理员权限；已有安装沿用原目录。配置保存在%LOCALAPPDATA%\LiteLaunch，覆盖安装不替换现有用户配置。

安装包封装的是已由用户测试通过的1.0.21应用，包内应用SHA-256为D856A2653B69EAB741FD3E52E9D923D6FBC29CD25C60EAA76AA8D6DEC7AC7D1E。安装包本身的SHA-256见Release附件SHA256SUMS.txt。包内文件已解包核对，安装流程尚未在本次环境实测。

## 功能

中键呼出面板；第一格EVER重命名。拖入.lnk会解析真实应用目标、参数和图标；右键设置主题、缩放、黑名单与登录启动。

- 选中文字会填入查找框，默认替换全部名称，可手动限定选区。批量预览支持撤销，禁止替换改变文件对应行数。
- 编号模板(Alt+N)支持名称格式、起始数字、1—18位补零与步长；xxxx-{num}、起始1、位数3，对JPG得到xxxx-001.jpg，原扩展名自动保留。
- 字段{name}/{num}/{taken}/{created}/{modified}/{parent}/{now}支持名称、编号、EXIF拍摄时间、创建/修改时间、父目录和当前时间；如{taken:yyyyMMdd_HHmmss}-{num}。没有EXIF时提示并保留整批原文。
- 预览后主窗口点击确定才真正重命名，支持事务校验与后悔药。

## 安装版更新迁移

今后只发布安装包。1.0.21原内部更新器使用旧的单EXE协议；安装版内部更新正在1.0.22候选版迁移，须手动覆盖安装新版本一次。1.0.22测试通过并发布后，应用内可按GitHub项目地址下载安装包、校验并打开覆盖更新向导。目前不能把该候选功能描述为已在1.0.21交付，也不宣称无感热更新。

## 构建与验证

Windows x64、PowerShell、Zig0.15.2、Inno Setup6编译器和Windows自带.NET Framework csc.exe。

下载Zig官方0.15.2并解压到tools/zig/zig-x86_64-windows-0.15.2；安装Inno Setup。执行：

powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -InnoCompiler "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

输出outputs/LiteLaunch/1.0.21/LiteLaunch-Setup-1.0.21-x64.exe。build.ps1只发布安装包，内部LiteLaunch.exe位于temp，不作为免安装产品分发。构建结果可能因时间戳与已发布包不同；正式附件使用用户测试过的原始应用封装。

源码位于sources/LiteLaunch/1.0.21。Scintilla5.6.6和NanoSVG许可证完整保留，见THIRD-PARTY-NOTICES.md。自动测试不代替用户验收；后续新版本仍须用户明确测试通过后上传。