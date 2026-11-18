# 验证记录

## 2026-11-18 · Windows 启动基线

| 检查 | 实际操作与结果 | 有效范围 |
|---|---|---|
| 定位与版本 | 从卸载记录定位 launcher.exe，读取游戏目录、version.json、PE 版本资源和标准序列化头；启动器 2.6.3.0、包 4.9.1、Unity 2022.3.52f1c1 | 本机磁盘快照，未检查服务器与运行中热更 |
| 资源清单 | 运行 inspect_client.py。85,254 条清单；launch 20 项、matrix 84,758 项的文件名和大小与磁盘一致。每扩展名最多等距取 16 个文件检查文件头 | 清单比对只查名称与大小。抽样文件头只代表样本 |
| 散装对象 | inspect_unity_types.py 普通和 --classify-names 两种模式均成功读取 11 个标准文件、10,010 个对象。未知格式的合成测试被跳过并返回非零 | 得到类型和命名线索，无动作或状态转换的行为验证 |
| 标准索引 / 媒体头 | inspect_public_index.py --media-headers 成功读取两个 UnityFS 索引。79,841 条 .uab 的逻辑名为空。读取三张明文 Cue 表，共 133 行；核对 24 个带训练命名的视频头，其中 11 个标称 60 fps | 未解包非标准 .uab，未播放音视频。没有逐帧结果 |
| Editor 编译 | UE 5.8.4 UBT 实际编译、链接 PGRCombatEditor Win64 Development，生成 UnrealEditor-PGRCombat.dll，返回 0 | 当前模块、当前工具链 |
| Game 编译 | PGRCombat Win64 Development 编译链接，生成 PGRCombat.exe，返回 0 | 未用 Cook 后内容启动该 exe |
| 无渲染启动 | UnrealEditor-Cmd -game -nullrhi 进入 /Engine/Maps/Entry，Engine is initialized、RequestExitWithStatus(0,0)、LogExit，进程返回 0 | 实际 Tick 与正常退出，不涉及画面或操控 |
| 图形启动 | -game -d3d12 -windowed，使用 RTX 5060。日志记录地图游玩、引擎初始化、游戏窗口与正常退出，返回 0 | 编辑器宿主图形路径，无战斗画面或人工体验验收 |
| 配置修正 | 首次空图运行出现 Enhanced Input 默认类警告。在 DefaultInput.ini 指定 EnhancedPlayerInput 与 EnhancedInputComponent 后，两种启动最终日志均无该警告或 Error、Fatal、Ensure | 留存首次结果；复跑命令见 [环境说明](environment.md) |
| 入库规则 | git check-ignore 命中媒体、客户端后缀、密钥、Intermediate 和 Binaries；git check-attr 对 .uasset/.umap 显示 lfs。LFS 只配置未来的授权资产 | 首次提交不需要传输任何 LFS 二进制。本机完整日志留在被忽略的 Saved/Logs |

**仍待验证**：所有原作战斗时序和机制行为、逐帧标定、目标角色手感、蓝图编辑和动画重定向、MCP 会话、自动化战斗回归、Cook、打包和第二台实体电脑运行。未启动原作、登录原作账号或自动输入游戏指令。
