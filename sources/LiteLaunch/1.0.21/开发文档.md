# LiteLaunch（轻启）

Windows x64中键启动面板，带EVER批量重命名、主题/缩放、窗口黑名单、登录静默启动和GitHub版本更新。

## 下载与运行

从[正式版本](https://github.com/ccl4-56-23-5/LiteLaunch/releases/latest)下载`LiteLaunch.exe`，放入固定目录运行，无需安装Quicker、Zig或.NET SDK。Windows PowerShell用于可选的GitHub更新；正则/日期规则使用Windows自带.NET Framework。

当前版本：**1.0.21**。发布EXE SHA-256：

`D856A2653B69EAB741FD3E52E9D923D6FBC29CD25C60EAA76AA8D6DEC7AC7D1E`

中键呼出面板；第一格进入EVER重命名。拖入`.lnk`会解析真实应用目标、参数和图标。右键面板设置主题色、缩放、黑名单、自启动或检查GitHub更新。配置保存于`%LOCALAPPDATA%\LiteLaunch`，更新不会覆盖已有配置。

## 1.0.21主要改进

- 选中文字会填入查找框，默认替换全部名称；可手动限定选区，批量预览支持撤销，禁止替换改变文件对应行数。
- EVER“编号模板”(Alt+N)支持格式、起始数字、1—18位补零及步长。名称格式`xxxx-{num}`、起始1、位数3，对JPG得到`xxxx-001.jpg`。格式只填主体，扩展名自动保留。
- 字段：`{name}`当前名称、`{num}`编号、`{taken}`EXIF拍摄时间、`{created}`创建时间、`{modified}`修改时间、`{parent}`父目录、`{now}`当前时间。
- 时间可单独设格式，如`{taken:yyyyMMdd_HHmmss}-{num}`。Windows属性处理器无法读取EXIF时，提示并保留整批原文，不以其他时间替代。
- GitHub更新只接受本仓库稳定Release中的`LiteLaunch.exe`，校验SHA-256、大小及产品/版本；下载和安装分别确认，替换失败尝试回滚。

文件名先在编辑区预览，点击主窗口“确定”才修改磁盘；支持事务校验和后悔药。重命名前请核对预览。已完成本版本地自动测试和用户测试确认；真实在线升级链路将随着后续正式版本验证。

## 从源码构建

Windows x64，PowerShell，Zig **0.15.2**，Windows自带.NET Framework4的`csc.exe`。

1. 从[Zig官方归档](https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip)下载并解压到`tools/zig/zig-x86_64-windows-0.15.2/`；编译器不提交到仓库。
2. 执行`powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1`。亦可传`-Compiler <zig.exe完整路径>`。
3. 输出为`outputs/LiteLaunch/1.0.21/LiteLaunch.exe`；缓存仅在本仓库`temp`目录。

源码按`sources/LiteLaunch/1.0.21/`归档，随附静态Scintilla5.6.6和NanoSVG。编译结果可能因资源/编译器时间戳而与已发布EXE的哈希不同；Release提供的是经过用户确认的原始交付件。

## 验证入口

源码`qa`目录提供`run-rules.ps1`、`run-ever-form.ps1`、`run-ever-state.ps1`、`run-regression.ps1`和`test-update.ps1`。1.0.21本地结果分别为69、23、90、38、15项/组通过。测试样本均为独占临时文件；`presets/LiteLaunch.ini`是匿名迁移测试夹具，不是用户配置，不参与正常首次运行默认配置生成。

## 发布规则

每个版本只有在维护者明确确认人工测试通过后，才上传提交、标签和正式包。发布后核对远端源码及下载SHA-256，再清理本地旧普通版本。自动测试不能替代人工确认。

第三方许可证与来源见[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。