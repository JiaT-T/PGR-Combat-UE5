# PGR Combat UE5

**试玩第一段普攻。** 在默认地图 `/Game/Maps/Training/TrainingFighter/L_CombatTraining` 中点击播放，按 **J** 或 **鼠标左键**。TS 为剑盾普攻选择 `Attack01`，角色挥剑并自然收招，随后可再次输入。当前角色与动作是原创占位素材，一次挥击约 0.90 秒。第一次打开工程前请完成下方的插件恢复与 TS 构建。

| 从这里进入 | 内容 |
|---|---|
| [当前进度](docs/progress.md) · [验证记录](docs/validation.md) | 已完成、已验证、阻塞与下一项可执行工作 |
| [客户端盘点](docs/research/client-layout.md) · [对象与类型](docs/research/unity-types.md) · [标准资源索引](docs/research/public-index.md) | 安装位置、版本、普通元数据、反馈资源与复验方法 |
| [机制证据账本](docs/research/mechanics.md) · [下一阶段测量](docs/research/measurement.md) | 九类战斗问题及证据等级、手动采样与逐帧测量方法 |
| [UE 架构草案](docs/architecture.md) · [设计决策](docs/decisions.md) | 单机战斗职责、候选框架、决策依据与两周实施顺序 |
| [换机与构建](docs/environment.md) · [Agent 指令](AGENTS.md) | Git/LFS、引擎与工具版本、复跑命令和每阶段交接要求 |

## 开始工作

有权限的 GitHub 账号可从下列地址克隆。进入目录后，按环境文档设置 UE_ROOT，构建 PGRCombatEditor 并运行启动检查。编译 Demo 只需本仓库与公开工具链。复查原作文件时另外提供本机 PGR_CLIENT_ROOT。

```powershell
git clone https://github.com/JiaT-T/PGR-Combat-UE5.git
cd PGR-Combat-UE5
git lfs install --local
git lfs pull
python tools/install_puerts.py
npm.cmd ci
npm.cmd run build
```

脚本入口是 [TypeScript/PGR/main.ts](TypeScript/PGR/main.ts)。GameInstance 初始化时创建 Puerts QuickJS 环境并执行入口，退出时释放环境。修改 TS 后运行 `npm.cmd run build` 再启动游戏。C++ 宿主、UE 配置与工具分别位于 Source/、Config/、tools/。[构建与往返验证](docs/environment.md#puerts-与-typescript)

