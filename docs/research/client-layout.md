# PC 客户端盘点

调查日期为 2026-11-18。Windows 卸载项「战双帕弥什」的 DisplayIcon 指向启动器，DisplayVersion 与启动器 PE 版本相符。只沿该安装位置读取。以下路径除了定位记录，均相对于含 PGR.exe 的游戏根目录。

| 层级 | 本机实测 | 证据与解释 |
|---|---|---|
| 启动器 | `E:\PGR\Punishing Gray Raven\launcher.exe`，2.6.3.0 | 卸载注册表、PE FileVersion、旁边的同名版本目录 |
| 游戏 | `E:\PGR\Punishing Gray Raven\Punishing Gray Raven Game\PGR.exe`，安装包 4.9.1 | 根目录 version.json 的 package_version。文件内容采用未加引号的键值，名称虽为 .json，标准 JSON 解析器无法直接解析。未向服务端查询热更版本 |
| 运行引擎 | Unity 2022.3.52f1c1 | PGR.exe 与 UnityPlayer.dll 的 ProductVersion 均含同一修订 f5e26ee28457，globalgamemanagers 的序列化头独立给出相同引擎版本 |
| 构建布局 | 与 IL2CPP 构建一致 | GameAssembly.dll、PGR_Data/il2cpp_data、ScriptingAssemblies.json 同时存在。标准路径的 global-metadata.dat 文件为 0 字节，不能从这个文件取得方法或字段元数据 |

## 文件结构与可读边界

| 位置 / 格式 | 实际观察 | 对复刻的作用 |
|---|---|---|
| PGR.exe、UnityPlayer.dll、GameAssembly.dll、PGRBase.dll | 游戏根目录的原生文件。启动盘点检查了文件名、大小与已有 PE 版本字段 | 识别构建形态。11 月 19 日补充了 GameAssembly.dll 的单函数静态检查，见下文 |
| LocalGameResources.json | 标准 JSON，resource 数组有 85,254 条，含相对目标名、大小与校验字段等 | 可统计类型与版本快照、核对文件是否落地。清单及下载定位字段留在游戏目录 |
| PGR_Data/app.info、boot.config、ScriptingAssemblies.json、RuntimeInitializeOnLoads.json | app.info 指明产品。程序集清单有 168 个名称。含 Animation、Physics、Cinemachine、Timeline、UI 和 AI.Navigation 相关程序集 | 只能证明构建列出了模块。模块自身不能证明相机参数、打击检测算法或 AI 决策逻辑 |
| PGR_Data/globalgamemanagers、level0–3、*.assets、*.resS、*.resource | 普通序列化文件与流式数据。已单独检查散装文件的对象目录 | 可定向查对象类型，区分 UI 动画、特效与战斗动作，见 [散装对象调查](unity-types.md) |
| PGR_Data/StreamingAssets/resource/launch | 20 项，其中 17 个 .uab、1 个 .acf、2 个无扩展名文件 | 文件头抽样，另单独检查标准容器入口 |
| PGR_Data/StreamingAssets/resource/matrix | 84,758 项，常见文件名为 16 位十六进制样式 | 主体内容的清单和外层格式可识别，单凭文件名无法将角色与技能对应 |
| PGR_Data/Plugins 与 il2cpp_data | 原生插件、SDK 资源和运行时目录。清单含 114 个 .pak，主要出现在插件的 WebView 等子目录 | 结合具体位置、UnityPlayer 和序列化头识别引擎。避免把 .pak 扩展名误读为 UE 游戏证据 |

## 主资源目录清点

使用 tools/inspect_client.py 对 launch、matrix 的直接子文件按相对名与字节数逐项比对安装清单。两个目录的缺失、多出及大小不符均为 0。此次未校验全量内容哈希，以下样本也不外推到未读文件。

| 仅 matrix | 磁盘文件数 | 字节总数 | 文件头样本与可回答的问题 |
|---|---:|---:|---|
| .uab | 79,824 | 33,279,400,032 | 排序等距取 16 个，均以 HARUINKURO 前缀开头。launch 另取 16 个 .uab 亦如此。记录为非标准外层。这项证据本身不能判定是否加密、能否解包或里面有哪些动作 |
| .acb | 2,561 | 2,425,067,872 | 16/16 为 @UTF，符合 CRI 音频提示数据的线索 |
| .awb | 1,862 | 10,383,732,730 | 16/16 为 AFS2，符合音频波形库线索。无法由文件头推得受击音效与事件的对应关系 |
| .usm | 509 | 21,737,197,472 | 16/16 为 CRID，符合 CRI 媒体容器。不能当作带输入时间戳的战斗录像 |
| 无扩展名 | 2 | 13,179,780 | 两个文件中一个以 UnityFS 开头，另一个具有上述非标准前缀。launch 的两个无扩展名文件也各有一种前缀 |

两目录合计 79,841 个 .uab。进一步读取两份标准索引后，确认所有 .uab 条目的逻辑路径字段都为空。索引仍能定位音库和教学视频候选。见[标准索引与媒体调查](public-index.md)。标准容器入口、普通对象目录、程序集名与资源清单是四种证据，不能相互替代。对自定义外层不做剥离、还原或保护判断。本轮未取得可供 UE 使用的授权原作素材。

## 2026-11-19 · 资源包和代码能分别读到什么

本轮重读 resource/launch 与 document/launch 各一个 .uab 和同目录索引，前者均以 HARUINKURO 开头，后者均以 UnityFS 开头。.uab 在这里是客户端的资源文件后缀。自定义外层的内部组织、压缩或加密方式仍未确定。普通 Unity 动画片段描述采样曲线和事件等数据，播放条件可能来自其他代码或配置。[国服模式全量建表结果](nanami-motion-assets.md)

PC 4.9.1 的 ScriptingAssemblies.json 列有 Assembly-CSharp.dll，常规 PGR_Data/Managed 路径未见该文件，标准位置的 global-metadata.dat 为 0 字节。实际用 Visual Studio DUMPBIN 静态读取 GameAssembly.dll 的 PE 导出表及一个已命名函数的机器指令。384 个具名导出中有 239 个以 il2cpp_ 开头。普通运行时函数 il2cpp_string_length 读取一个 32 位字段后直接返回，按函数语义可写作伪代码 `return text.length;`。字段名是便于阅读的命名。此次只辨认了这个函数，未定位七实招式的调用者或转移条件。

PGR_Data/Plugins 的 x86 和 x86_64 子目录均有 xlua.dll。对安装清单 85,254 条 dest 检查 .cs、.lua、.luac 以及这些后缀追加 .txt 或 .bytes 的情形，命中 0 条。插件是脚本支持线索，清单文件名不能证明包内是否有脚本，也不能决定七实的战斗逻辑写在哪一种语言里。

## 复查

在 PowerShell 设置本机 `PGR_CLIENT_ROOT` 为含 PGR.exe 的目录，进入原创仓库根目录执行。两个脚本使用 Python 标准库，输出为终端汇总，不写入游戏目录。

```powershell
python tools/inspect_client.py "$env:PGR_CLIENT_ROOT"
python tools/inspect_unity_types.py --classify-names (Join-Path $env:PGR_CLIENT_ROOT PGR_Data)
```

新版本需重新核验安装包版本、标准头、外层抽样和目录差异。安装包清单与本机行为都不能代表所有平台、角色、敌人或热更新状态。
