# LiteLaunch 批量重命名插件 1.0.2

本次将规则队列扩展到 ReNamer 文档列出的18类规则，并增加独立的元数据模板。保留 EVER 简易处理、手工微调、文件夹批量重命名、撤销和安装目录预设。基座仍为1.0.29，无需重装基座。

在“规则队列 → 新增规则”选择类型，再填写该规则的参数。序列化提供起始、正负步长、位数、重复、重置条件、数制与插入位置；元数据通过“字段 / 工具”按类别和关键字选择，可预览第一个项目的字段值。

规则队列从原始名称依次计算；简易处理作用于当前预览。预设固定存于基座安装目录的 RenamePresets。独立启动时使用插件目录。旧版规则可载入，首次覆盖旧格式前保留 .v1.bak；原设置、记忆和重命名撤销记录继续使用原位置。

正常规则只计算预览名称，点击“执行重命名”后才改名。Pascal脚本若显式勾选“允许脚本写文件和运行外部程序”，其文件操作在预览时也会执行；此项默认关闭，编译检查不执行脚本。元数据字段为只读，不修改照片、音频或文档的内部标签。

规则和兼容范围见 [规则功能对照.md](规则功能对照.md)。这是依据公开功能文档独立实现的引擎，未包含 ReNamer 的源码、二进制或美术资源。Pascal 脚本使用 **RemObjects Pascal Script**：https://github.com/remobjects/pascalscript 。原始许可证及已标注的编译兼容修改保留在vendor中。

## 构建和验证

在Windows上使用 Zig 0.15.2、系统.NET Framework 4.x编译器、Free Pascal 3.2.2 Win32。工作区的编译器位置为 tools/zig/zig-x86_64-windows-0.15.2/zig.exe 和 tools/fpc/3.2.2/{app}/bin/i386-win32/fpc.exe。FPC官方安装器可从 https://downloads.freepascal.org/fpc/dist/3.2.2/i386-win32/fpc-3.2.2.i386-win32.exe 获取；安装器可用Inno解包工具解包到上述位置。PascalScript源码和完整ExifTool便携组件已随源码保留，来源与SHA-256见vendor的provenance文件。

执行 build.ps1 构建单EXE，构建目录在工作区 temp/LiteLaunchBatchRenamer-build-1.0.2，交付在 outputs/LiteLaunchBatchRenamer/1.0.2。用户无需单独安装ExifTool、Perl或Pascal编译器；组件从已校验插件的内嵌资源释放到私有运行目录。

验证入口：qa/advanced_worker_test.py <EverRegex.exe>、qa/run-batch.ps1、qa/run-batch-form.ps1、qa/run-rules.ps1、qa/run-rename.ps1。测试只使用新建临时目录和自己的窗口，不对用户文件试改名。

## 发布

独立插件发布标签为 batch-renamer-v1.0.2，主程序最新通道保持 v1.0.29。按用户长期授权，完成必要验证后安装到已有目标，再上传源码与插件；实际下载核验成功后，本地仅留最新版源码与交付件，保护基线和用户数据。发布记录以输出目录的验证、安装和远端核验记录为准，不将自动测试视为用户人工验收。
