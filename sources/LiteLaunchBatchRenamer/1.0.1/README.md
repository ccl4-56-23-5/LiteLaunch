# LiteLaunch 批量重命名插件 1.0.1

独立更新的批量重命名插件，兼容 LiteLaunch 1.0.24 及以上；宿主正式版仍为 1.0.28。

- 长参数标签测量实际高度，完整显示换行；长表单支持滚动，底部提示区支持多行。
- 默认提供 EVER 简易处理：加字、删除/清空、替换、编号/元数据、大小写、空白清理、还原、撤销/重做；可切换规则队列组合处理。
- 删除字符新增“清空名称”。保留项目行和扩展名，可继续添加文字或编号；禁止提交空名称。
- 支持多选文件夹本身，或选择父目录批量添加直接子文件夹；保留拖入和读取资源管理器选中项。
- 预设只输入名称，固定保存至安装目录的 RenamePresets。通过宿主启动时采用宿主安装路径，独立运行时采用本 EXE 所在目录。预设保存规则队列，手工名称不包含在预设中。

验证：批处理/目录/预设 45 项，生产表单和布局 38 项，规则与实际 .NET 正则 69 项，文件事务 70 项，共 222 项自动检查通过。真实文件操作检查使用独立临时文件和文件夹。用户人工验收尚未记录。

构建：运行本目录 build.ps1，默认使用工作区 tools/zig/zig-x86_64-windows-0.15.2/zig.exe，也可传入 -Compiler。需要 Windows .NET Framework 的 C# 编译器。构建目录为 temp/LiteLaunchBatchRenamer-build-1.0.1，输出为 outputs/LiteLaunchBatchRenamer/1.0.1。

第三方源代码与许可完整保留于 vendor，Scintilla 来源记录见 vendor/scintilla/LITELAUNCH-PROVENANCE.md。
