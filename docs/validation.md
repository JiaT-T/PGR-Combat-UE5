# 验证记录

## 2026-11-19 · AnimeStudio 国服专用模式

实际运行 PGR_CN_JP_TW 的 AssetMap 模式，对 resource 与 document 下的 82,187 个 .uab 按批核对输入文件名、逐文件日志、返回码与 JSON 配置字段。87 批均退出 0，全部 .uab 报告 no assets found。已用同字节不同扩展名的阳性对照验证工具能列出 3 个动画和 4 个 Animator。四份标准索引、两份引擎内置资源也已读取，未定位到七实模型或动画。未取得运行时状态或验证原作动画 FBX 导出。[原始计数、指令和验证范围](research/nanami-motion-assets.md)

## 2026-11-19 · AnimeStudio CLI

用普通模式分别扫描 11 个散装文件、两份 UnityFS 索引及两个 .uab 样本，四份对象表的记录数依次为 870、1、1、0。核对全部 79,841 个 .uab 的文件头，并逐字节复核 15 个输入副本。未启用战双专用配置，未导出角色资源。[样本、命令与检索结果](research/nanami-motion-assets.md)

## 2026-11-19 · 七实·芒星之迹 SSS

在默认涂装的模型页逐项查看 Attack40–48，对照 V02 的长按黄球与盾斧普攻段，另以 V01 的相同演出复核。18 个参照片段的 73 条共同局部旋转轨用于区分独特衔接姿势与公共待机。网页的接缝有证据，录像到具体编号及输入触发条件分别标注为候选。[片段、帧号及方法](research/nanami-motion-assets.md#attack4048-的动作衔接)

完整解码 V02 的 2,308 个显示帧，复核多段可见触点、图标与库存的先后，读取 L02 进战、预载、版本字段及全段时间范围。检查 25 张技能说明，核对 Huaxu 对应等级数值与图标引用。角色语音页四种语言各 83 个条目与相应本地音库逐项吻合。未取得输入受理帧、实机伤害倍率或具体 PC 动作包的对应关系。[逐帧与资源验证记录](research/training-capture-02.md) · [技能条件与配置](research/nanami-startrail.md)

在浏览器打开模型页，以页面默认的泛用式·终加载 3D 预览。Fight 的选择器列出三层，对应清单共 108 条片段（含 Menu）。红球 Attack11 播放时间轴总长 4.20 秒，BaseLayer 的 Attack51 与 Layer1 的 Attack52 实际播放均显示 9.33 秒。其余列出的时长来自清单。未验证两次实机必杀各自的片段编号。[动作索引与测量范围](research/nanami-motion-assets.md)

## 2026-11-19 · 训练录像与 Android 日志

已核对日志版本字段、进战和退战标记、中段记录间隔与预载耗时。完整解码 1,766 帧后复查教学卡、红/蓝计数变化和两组 HUD 跳字边界。未发现可将按键、命中、闪避或超算对齐到录像帧号的日志事件。两份材料仅作本地分析，归档内容限测量和观察。[帧号、时间与取证范围](research/training-capture-01.md)

## 2026-11-18 · Windows 启动基线

| 检查 | 实际操作与结果 | 有效范围 |
|---|---|---|
| 定位与版本 | 从卸载记录定位 launcher.exe，读取游戏目录、version.json、PE 版本资源和标准序列化头。启动器 2.6.3.0、包 4.9.1、Unity 2022.3.52f1c1 | 本机磁盘快照，未检查服务器与运行中热更 |
| 资源清单 | 运行 inspect_client.py。85,254 条清单。launch 20 项、matrix 84,758 项的文件名和大小与磁盘一致。每扩展名最多等距取 16 个文件检查文件头 | 清单比对只查名称与大小。抽样文件头只代表样本 |
| 散装对象 | inspect_unity_types.py 普通和 --classify-names 两种模式均成功读取 11 个标准文件、10,010 个对象。未知格式的合成测试被跳过并返回非零 | 得到类型和命名线索，无动作或状态转换的行为验证 |
| 标准索引 / 媒体头 | inspect_public_index.py --media-headers 成功读取两个 UnityFS 索引。79,841 条 .uab 的逻辑名为空。读取三张明文 Cue 表，共 133 行。核对 24 个带训练命名的视频头，其中 11 个标称 60 fps | 未解包非标准 .uab，未播放音视频。没有逐帧结果 |
| Editor 编译 | UE 5.8.4 UBT 实际编译、链接 PGRCombatEditor Win64 Development，生成 UnrealEditor-PGRCombat.dll，返回 0 | 当前模块、当前工具链 |
| Game 编译 | PGRCombat Win64 Development 编译链接，生成 PGRCombat.exe，返回 0 | 未用 Cook 后内容启动该 exe |
| 无渲染启动 | UnrealEditor-Cmd -game -nullrhi 进入 /Engine/Maps/Entry，Engine is initialized、RequestExitWithStatus(0,0)、LogExit，进程返回 0 | 实际 Tick 与正常退出，不涉及画面或操控 |
| 图形启动 | -game -d3d12 -windowed，使用 RTX 5060。日志记录地图游玩、引擎初始化、游戏窗口与正常退出，返回 0 | 编辑器宿主图形路径，无战斗画面或人工体验验收 |
| 配置修正 | 首次空图运行出现 Enhanced Input 默认类警告。在 DefaultInput.ini 指定 EnhancedPlayerInput 与 EnhancedInputComponent 后，两种启动最终日志均无该警告或 Error、Fatal、Ensure | 留存首次结果。复跑命令见 [环境说明](environment.md) |
| 入库规则 | git check-ignore 命中媒体、客户端后缀、密钥、Intermediate 和 Binaries。git check-attr 对 .uasset/.umap 显示 lfs。LFS 只配置未来的授权资产 | 首次提交不需要传输任何 LFS 二进制。本机完整日志留在被忽略的 Saved/Logs |
| GitHub 发布 | 核实已有登录身份 JiaT-T，建库返回 HTTP 201，回读 visibility=private、push=true。首次推送 main 返回成功。远端分支与本地基线 `2f9448d8fcbbc3ba9308ef1a67a487301e4e2d3a` 一致，GitHub tree 与本地的 24 个文件名和 blob SHA 全部相符 | 首批文件均为原创 UTF-8 文本，已核对路径、大小与敏感标记。仓库级作者采用账号的 GitHub noreply 邮箱 |
| 干净克隆迁移演练 | 从 GitHub HTTPS 克隆至新的系统临时目录，核对 HEAD 为上述基线。git lfs install --local 和 git lfs pull 成功，当前有 0 个 LFS 文件。12 份文档、3 个工具和工程均存在。新目录重新编译 PGRCombat.cpp、链接 Editor DLL，Null RHI 进入 Entry 地图并正常退出。两个进程均返回 0，运行后 Git 工作树干净 | 同机干净检出，共享本机 UE、SDK 与系统缓存。第二台实体电脑仍待独立复测 |

**仍待验证的内容包括**原作战斗时序和机制行为、逐帧标定、目标角色手感、蓝图编辑和动画重定向、MCP 会话、自动化战斗回归、Cook、打包和第二台实体电脑运行。未启动原作、登录原作账号或自动输入游戏指令。
