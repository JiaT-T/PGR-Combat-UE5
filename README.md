# PGR Combat UE5

《战双帕弥什》UE5 战斗系统复刻是一个独立个人学习项目。目标是在寒假两周内交付可在 Windows 上重复游玩的单角色战斗 Demo。玩家在一间训练场里练习连段、闪避、信号球技能和超算，与一名可预测攻击的敌人交手。具体角色、动作来源和手感标准由项目主人确认。

**启动基线 · 2026-11-18**　已完成 PC 安装包 4.9.1 的只读调查、UE 5.8.4 C++ 空工程及本机编译启动验证。真实文件提供了技能球、超算、命中反馈等元数据线索。具体规则和角色时间窗等待实机对照。工程当前进入引擎空地图，尚无可操作的战斗角色。

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
```

源码位于 Source/，引擎设置位于 Config/，聚合调查工具位于 tools/。Git 跟踪原创源码、可迁移配置和研究结论。使用原作商标仅为标明研究对象。原客户端、专有素材、未获授权的媒体、工作单位的任何保密资料和账号信息均不得进入本仓库。
