# 验证记录

## 首段普攻审查修订

2026-11-20，代码 PR #1 的验证提交为 `0106f36e334e056e434443a563b4ee9a2f2a68b2`。TypeScript、UE 5.8.4 Editor 和 Win64 Development 游戏目标均通过编译。

- **输入阶段。** 在独立工程副本里观察 TS 实收的阶段。原配置下，两次 J 键按下、松开均收到 `Pressed → Released`。仅在副本内给同一输入动作设置 20 秒的长按触发条件，提前松开两次，均收到 `Pressed → Canceled`。两种配置下首段都正常收招；两次运行也通过了原有的同帧打断并重新起招用例。
- **启动确认。** 正常 TS 入口绑定选招回调后调用无参 `ConfirmStartup()`，即可出招。分别用“回调未绑定就确认”和“已绑定却未确认”的临时入口启动游戏，两次都进入启动失败处理并正常关闭宿主。这两种故障只在独立副本中注入。
- **交付构建。** 还原副本中的输入资产和 JS 后核对其与正式工作区的字节一致，再制作 Win64 Development 包。正式工程的新编辑器进程和独立包均以 D3D12 在七实训练场复跑 `PGR.Combat.BasicAttack.TrainingMap` 并通过。

## 七实默认涂装本地实测

2026-11-19 使用 UE 5.8.4，测试默认涂装「泛用式·终」及专武「隧星盛耀」。

- **下载和导入。** 角色清单有 108 个不同文件、79 个不同动作名。按层保留重名片段。另有四套武器模型及 44 个武器片段。含源清单共 162 个文件，逐个记录了 SHA-256。身体模型在战斗上下文有 14 个可见网格。角色素材转换后，UE 在新进程中成功加载 108 个不同的 `AnimSequence`，逐一核对骨架和源时长，最大时长差小于 0.000004 秒。
- **动作和材质。** UE 在 `Attack01` 的 0 与 0.45 秒取样，272 根骨骼中 143 根的局部旋转差异超过约 5°。基础待机使用同骨架的 `Stand2`。重新打开落盘材质后，14 个颜色输入、10 个源模型指定的法线输入、双面及透明设置均已核对。网页专用着色、特效、战斗事件与实际取消窗口另行验证。
- **剑盾装配。** 4 套武器中的刀和盾按网页默认的战斗挂点装在右手 `WeaponCase1` 与左手 `WeaponCase2`。训练图的展示网格有 321 根骨骼，共用身体的 14 个材质，另接 4 个武器槽。抽查 0、0.45、1、2 秒，两件武器的根均贴合各自挂点。身体首段为 4.167 秒，两件武器的同名片段均为 3.333 秒。这里采用各自原速，先结束的武器保持末姿。这是 Demo 的装配选择，原游戏的同步规则仍待核实。
- **训练场实测。** 在 D3D12 游戏模式经玩家控制器按下并松开 J 两次，两次均由 TS 选出 `Attack01` 并在约 4.17 秒后自然收招。同帧打断旧实例后，新实例也成功完成。初次抓图发现米制骨架在 UE 厘米场景中过小，放大本地角色网格 100 倍后，沿用的近景相机又把约两米的突进裁出了画面。在本地训练图设置 Player 0 专用相机后重新抓取 1280×720 的待机和四个攻击时刻，五帧均可见完整角色。保留的中段原图对应 2.023 秒。首段已接入输入，其他动作当前可在内容浏览器预览。
- **独立包。** 以最终训练图制作 Win64 Development 包，Cook 为 0 错误、0 警告。通过打包的可执行程序打开该图，在 D3D12 中复跑 `PGR.Combat.BasicAttack.TrainingMap`，结果为 Success，退出码为 0。本机可双击 `../Nanami-Local/Play-Nanami.cmd` 启动；打包日志和回归日志分别在素材目录下的 `work/ue/relocated-package.log`、`work/ue/relocated-game.log`。
- **换机复测。** 检出代码 PR #1 与配套资源均已合入的版本后执行 `git lfs pull`，打开 `/Game/LocalOnly/NanamiStartrail/L_NanamiTraining` 并按 J。模型、材质、动作及配套训练图仅用于本私库的学习验证。

## 2026-11-19 · 剑盾首段普攻

- TypeScript 5.9.3 编译通过。独立执行选招函数，普攻按下返回 `Attack01`，松开与其他命令返回空。
- UE 5.8.4 的 Editor 与 Win64 Development 游戏目标均编译成功。训练图 `/Game/Maps/L_CombatTraining` Cook 结果为 0 个错误、0 个警告，BuildCookRun 返回 0。地图、模型、骨架、材质、动画及配置为本项目制作，源模型与动画保存在 `SourceArt/TrainingFighter.fbx`。场地使用 UE 自带的基础几何体。
- Editor 游戏模式和独立包均使用 D3D12 执行 `PGR.Combat.BasicAttack.TrainingMap`，结果均为 Success。用例通过玩家控制器按下和释放映射到普攻的 J 键两次。两次都经 TS 选择 Attack01，Montage 播放约 0.90 秒后自然结束。再在同帧打断第三个实例并启动第四个实例，第四个实例保持有效并正常结束。D3D12 截图确认场地中可见完整人偶与武器。左键与 J 配在同一输入动作，本轮回归驱动的是 J。
- 初次建图后，放置角色保存了空的模型及输入配置覆盖值，截图仅见地面，J 键未起手。重新从已保存的角色蓝图放置实例，再次打开地图核对配置后，上述两条运行路径均通过。当前动作时长与姿势用于验证制作链和输入流程，手感未作原作对标。

可在编辑器的训练地图游玩状态运行控制台命令 `Automation RunTests PGR.Combat.BasicAttack.TrainingMap`。命令行复验使用 UnrealEditor-Cmd 的 `-game` 模式或启动 Development 包，传入 `-ExecCmds="Automation RunTests PGR.Combat.BasicAttack.TrainingMap" -TestExit="Automation Test Queue Empty"`，日志中检查该用例的 `Result={Success}`。

## 2026-11-19 · Puerts 与本机 UE 5.8.4

| 实际执行项 | 结果 |
| --- | --- |
| 独立探针 | 独立目录编译官方固定提交的 ScriptEnv、PapiQuickjs 和宿主模块。TypeScript 5.9.3 编译成功。用 UnrealEditor-Cmd 的游戏模式启动空地图，原生端收到了通过验证的脚本回传值，随后释放环境，进程返回 0。 |
| 正式工程 | 从官方源码恢复项目级插件，执行 npm ci、npm run build 和 PGRCombatEditor Win64 Development 构建。Editor 游戏模式下同一往返通过，退出码 0。 |
| Win64 独立包 | RunUAT BuildCookRun 对 /Engine/Maps/Entry 完成 Win64 Development 构建、Cook、Stage 和打包。UnrealPak 的列表包含项目的 PGR/main.js 和 Puerts 运行库，打包目录没有散装 main.js。直接启动包内游戏，在 Null RHI 和 D3D12 各通过一次往返并正常退出。D3D12 使用 RTX 5060。 |

往返由 GameInstance 的原生宿主发起。TS 编译产物从 Puerts 取得本次宿主，通过 UFUNCTION 读取本次随机值，构造带前缀的回复并调用第二个 UFUNCTION。C++ 校验匹配才打印 `completed native round trip`。验证脚本同时检查退出码、成功次数和随后发生的环境关闭。本轮未验证热重载、编辑器全量声明生成、七实技能或 Shipping 构建。[恢复与验证命令](environment.md#puerts-与-typescript)

## 2026-11-19 · 玩家确认的输入规则

红蓝二段资格保留、普攻末段输入缓存、技能球与闪避及切形态打断普攻后从首段重启，均按玩家说明登记。大招已确认优先于普攻。各段缓存窗口起止尚待实测，本轮未运行游戏或 UE 工程。[规则与来源](research/mechanics.md)

## 2026-11-19 · 战斗相关原生函数

在 PC 4.9.1 上实际执行 radare2 与 r2ghidra 的 pdg/pdgj。核对两个二进制的 SHA-256、18 个游戏程序集方法表槽位和两个插件导出，并按 PE 函数范围复核指令。干净目录复验，两个分析进程均返回 0，20 份 C 输出哈希全部匹配。3 份输出有尾跳转展开，已另附本方法的指令范围。未验证七实某一招的原始方法名、运行时调用顺序、取消条件或命中窗口。[反编译证据](research/native-combat-analysis.md)

## 2026-11-19 · UAB 文件头与代码可读性

复核两个 .uab 和两份标准索引的开头、常规托管程序集与元数据路径。读取 GameAssembly.dll 的 384 个具名导出，反汇编了一个 IL2CPP 字符串长度函数并解释其返回值。安装清单未命中按常见扩展名命名的 C# 或 Lua 源码。该结果没有验证任何角色状态转换。[样本和限制](research/client-layout.md)

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

**仍待验证的内容包括**原作战斗时序和机制行为、目标角色手感、蓝图编辑和动画重定向、MCP 会话、自动化战斗回归、带战斗内容的包和第二台实体电脑运行。

2026-11-20：UE 在移除临时重定向后的新进程中逐一加载 235 个迁移包（含两张训练地图），检查资产注册表中对旧包的依赖，全部通过。此次未重新打包或验收操作手感。
