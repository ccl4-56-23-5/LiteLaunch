# LiteLaunch 批量重命名插件 1.0.4

这版精简了重命名操作：新增和编辑规则只用一个窗口，处理方式及参数选项使用中文；切换处理方式直接更换同页参数。常用参数直接显示，其余在同页“更多设置”展开。

规则列表直接显示处理内容：固定文字显示原文，文件信息显示中文类型。例如“添加文字：照片_【拍摄日期】_【编号】”，无需打开规则才能知道插入什么。

“文件信息”规则直接用下拉列表选择类别和字段，可加前后文字，并查看第一个文件的示例。其他文字规则点“插入文件信息”即可在一个窗口完成分类、搜索和选择，插入位置用中文标记显示，不用输入元数据代码。简易处理的编号入口使用同一套窗口。

不再显示字段缺失策略等日常用不到的设置；缺失的文件信息按空文本处理。原有18类规则、文件夹改名、手工微调、撤销以及安装目录预设继续保留，旧预设会自动显示为中文字段。

## 构建与发布

运行 build.ps1 构建单EXE。使用Zig 0.15.2、系统.NET Framework编译器及Free Pascal 3.2.2，编译器位置和第三方来源见vendor的说明与provenance记录。Pascal Script基于RemObjects Pascal Script：https://github.com/remobjects/pascalscript 。ExifTool/Perl和Pascal组件内嵌在插件中，使用时无需另装。

构建缓存位于工作区 temp/LiteLaunchBatchRenamer-build-1.0.4，交付位于 outputs/LiteLaunchBatchRenamer/1.0.4。验证入口为 qa/run-batch-form.ps1、qa/run-batch.ps1 和 qa/advanced_worker_test.py。

插件独立发布 batch-renamer-v1.0.4，基座保持1.0.30。按用户长期授权直接安装、发布GitHub，实际下载核验后清理旧版，保留用户数据与Git历史。自动验证与人工使用分别记录。
