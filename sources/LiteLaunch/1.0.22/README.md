# LiteLaunch（轻启）安装版

Windows x64中键启动面板与EVER批量重命名工具。仅发布安装版。

## 下载和覆盖安装

从[最新正式版本](https://github.com/ccl4-56-23-5/LiteLaunch/releases/latest)下载LiteLaunch-Setup-1.0.22-x64.exe并运行。安装包识别当前运行的LiteLaunch、原安装记录（兼容32/64位注册表）和现有桌面/开始菜单快捷方式。找到有效旧版后自动沿用原目录、跳过目录选择，并在准备安装页显示旧应用版本和覆盖路径；不需要先卸载。首次安装才选择目录。默认当前用户安装，提供快捷方式、自启动选项和Windows卸载入口。

配置、应用列表、EVER设置与历史保存在%LOCALAPPDATA%\LiteLaunch；安装包不包含真实用户配置，覆盖安装不会替换这些文件。更新前保存正在编辑的重命名。

1.0.22已由维护者明确确认测试通过。正式安装包SHA-256：4930222ADC5C886374921CCDFDAD4C5DFF7A439BCF8134E07D68F9A725CE4C86。Release同时提供源码ZIP、SHA256SUMS.txt和第三方许可证。

## 应用内GitHub更新

面板右键“检查GitHub更新…”，填写GitHub项目地址或账号/仓库，例如https://github.com/ccl4-56-23-5/LiteLaunch。设置会保存，后续只查询指定仓库的稳定Release。

有更新时下载匹配版本的LiteLaunch-Setup-<版本>-x64.exe，校验GitHub生成的SHA-256、大小、产品及版本，再确认打开正常覆盖安装向导，沿用当前安装目录及开机启动选项。保留用户确认与可见向导，不是无感静默更新。1.0.21使用旧单EXE协议，请先手动安装1.0.22完成迁移。

## 功能

- 中键呼出启动面板；拖入.lnk解析真实应用目标、参数和图标；支持主题、自定义缩放、黑名单和登录启动。
- EVER重命名支持全文/选区查找替换、批量预览、撤销和事务校验。点击主窗口确定后才修改真实文件名。
- 编号模板（Alt+N）：xxxx-{num}、起始1、位数3，对JPG生成xxxx-001.jpg。支持起始数字、步长和1—18位补零；自动保留扩展名。
- 字段{name}/{num}/{taken}/{created}/{modified}/{parent}/{now}可组合；日期示例{taken:yyyyMMdd_HHmmss}。缺少EXIF时提示并保留整批原文。

## 构建和维护

环境：Windows x64、PowerShell、Zig0.15.2、Inno Setup6及Windows自带.NET Framework csc.exe。Zig下载路径见tools/README.md，安装Inno Setup后运行：

```powershell
.\build.ps1 -InnoCompiler 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
```

输出outputs/LiteLaunch/1.0.22/LiteLaunch-Setup-1.0.22-x64.exe。内部LiteLaunch.exe只作为构建/安装载荷，不发布免安装产品。由于编译时间戳等因素，重建包摘要不保证与正式附件相同。

源码位于sources/LiteLaunch/1.0.22；安装识别、更新协议和验证说明见[维护文档](sources/LiteLaunch/1.0.22/开发文档.md)。Scintilla5.6.6和NanoSVG许可证见THIRD-PARTY-NOTICES.md。新版本必须由维护者明确确认测试通过后才发布。
