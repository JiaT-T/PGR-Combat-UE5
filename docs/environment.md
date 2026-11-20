# 开发环境与换机恢复

核验日期为 2026-11-18。旧机的独立引擎位于 `E:\UnrealEngine\UE_5.8`。工程文件位于仓库根目录。以下命令均在仓库根目录的 Windows PowerShell 或 PowerShell 7 中执行。

## 已核验的工具链

| 组件 | 版本及取证方式 |
|---|---|
| Windows | Windows 11 专业版 25H2，内部版本 26200.9711。图形启动使用 NVIDIA GeForce RTX 5060 和 D3D12。 |
| Unreal Engine | 独立 Installed Build 5.8.4，CL 58902052，分支 `++UE5+Release-5.8`。核对引擎 `Engine/Build/Build.version`、`InstalledBuild.txt`、编辑器可执行文件和 UBT。 |
| Visual Studio | 2022 Professional 17.14.37906.3。此次 UBT 选择 MSVC 14.44.35230，安装目录族号 14.44.35207，Windows SDK 10.0.22621.0。VS Installer 另外列有 2026 Build Tools 18.9.12120.119，未参与本次构建。 |
| .NET | Build.bat 使用引擎随附的 .NET SDK 10.0.203。系统另装 SDK 9.0.321。 |
| Git / Git LFS | Git 2.47.1.windows.1，Git LFS 3.6.0。 |
| 工具脚本与视频 | `py -3 --version` 返回 Python 3.14.7。在本次命令行的 PATH 上未找到 `ffmpeg` 和 `ffprobe`。后续测视频前安装 FFmpeg 并记录实际版本。 |
| 索引解码 | 两个盘点/类型工具只需 Python 标准库。标准索引工具还需 MessagePack。本机实测 pip 26.2.1 随附的 msgpack 1.1.2。换机可在仓库外虚拟环境安装 `msgpack==1.1.2`，工具优先使用单独安装的包。 |
| Blender | `blender.exe --version` 返回 4.5.15 LTS。Blender 用户插件 `blender_mcp.py` 声明版本 1.8。检查时无运行中的 Blender，尚未验证连接及导出。 |

UE 插件版本取自同一引擎安装中的 `.uplugin`。

| 插件 | 描述文件版本 | 本工程状态 |
|---|---|---|
| Enhanced Input | 1.0 | 工程显式启用，配置了默认输入类，尚无动作绑定。 |
| Gameplay Abilities | 1.0 | 已安装，默认关闭。待架构试验再决定是否启用。 |
| StateTree / GameplayStateTree | 0.1 / 1.0 | 描述文件存在，默认关闭。 |
| Control Rig / IK Rig | 1.0 / 1.0 | 引擎默认启用，尚未导入或重定向动作。 |
| Functional Testing Editor | 1.0 | 已安装，默认关闭，首轮启动测试仅用引擎命令行。 |
| Unreal MCP（ModelContextProtocol / EditorToolset） | 1.0 / 1.0，Experimental | 工程为 Editor 目标显式启用两个引擎插件。2026-11-19 实测回环接口、工具发现、训练场地图和 Montage 元数据读取通过。 |

### 连接个人工程的 UE MCP

常规打开 `PGRCombat.uproject` 时，编辑器按 `Config/DefaultEditorPerProjectUserSettings.ini` 启动 MCP。项目使用独立的本机端口 `http://127.0.0.1:18475/mcp`。关闭该编辑器会终止服务。使用独立引擎时，也可以在已经指定 `UE_ROOT` 的 PowerShell 中后台启动编辑器：

```powershell
$editor = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor.exe'
$project = (Resolve-Path './PGRCombat.uproject').Path
Start-Process $editor -ArgumentList @("`"$project`"", '-RenderOffScreen', '-NoSplash', '-NoSound', '-NoAnalytics', '-NoLiveCoding', '-unattended', '-DisablePlugins=AndroidFileServer') -WindowStyle Hidden
Get-NetTCPConnection -State Listen -LocalPort 18475
```


2026-11-19 从本机独立 UE 编辑器完成 HTTP MCP 握手（协议 `2025-06-18`），发现 19 个工具集。读取当前地图成功，`AssetTools.get_asset_class` 返回 `AM_NanamiBasic` 的类型 `AnimMontage`；`AssetTools.get_dependencies` 在 `DA_NanamiActions` 的依赖中返回该 Montage。运行第二个编辑器宿主做自动化时，可为第二个进程传入 `-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False`。UE 5.8.4 的 Cook 等命令程序会自动跳过 MCP 自启动。

## Puerts 与 TypeScript

2026-11-19 在本项目启用项目级 Puerts，源码固定在官方提交 `46ced3d44b1faea7ad2e3511497bbfb369824be2`。安装工具保留上游许可证，将插件描述符的模块列表限定为 ScriptEnv、PapiQuickjs 和构建依赖 ParamDefaultValueMetas，其余插件源码维持上游版本。QuickJS-NG 0.6.1 随项目编译。Node.js 24.20.0、npm 11.19.0 和锁定的 TypeScript 5.9.3 用于在 Windows 上生成运行脚本。[固定版本的开源插件](https://github.com/Tencent/puerts/tree/46ced3d44b1faea7ad2e3511497bbfb369824be2/unreal/Puerts)

克隆到新设备后，在仓库根目录、首次编译或打开工程前执行以下命令。前两条需要访问公开依赖源。安装器会校验已有插件的上游文件。

```powershell
python tools/install_puerts.py
npm.cmd ci
npm.cmd run build
```

游戏脚本的可编辑入口是 `TypeScript/PGR/main.ts`，构建产物是 `Content/JavaScript/PGR/main.js`。每次修改 TS 后重新运行第三条命令并重启游戏实例。UBT 构建插件时会从上游复制运行库到 `Content/JavaScript/puerts`。项目入口、角色规则目录 Content/JavaScript/Combat 和 Puerts 运行库均在 DefaultGame.ini 中列入游戏包。插件、复制的声明及运行库、npm 依赖和生成的 JS 均在本地恢复。当前脚本宿主使用窄类型声明，未启用依赖 V8 的编辑器全量 UE 声明生成器。

按下节设置 UE_ROOT 并完成 Editor 构建后，运行 `./tools/test_puerts.ps1`。脚本会启动项目的默认地图，检查原生回调和环境关闭日志，并输出本次日志路径。测试独立 Development 包时，传入 `-PackagedExe` 指向包中 `PGRCombat/Binaries/Win64/PGRCombat.exe`。

以下命令从根目录制作当前训练地图的 Windows 包。Cook 命令只为本次关闭 AndroidFileServer，避免其在 Windows 训练工程中生成无关的设备设置。

```powershell
$uat = Join-Path $env:UE_ROOT 'Engine/Build/BatchFiles/RunUAT.bat'
$project = (Resolve-Path './PGRCombat.uproject').Path
$stage = Join-Path (Get-Location).Path 'StagedBuilds/BasicAttack'
& $uat BuildCookRun "-project=$project" -nop4 -unattended -utf8output -nocompileeditor -platform=Win64 -clientconfig=Development -build -cook -map=/Game/Maps/Training/TrainingFighter/L_CombatTraining -stage -pak -iostore -package "-stagingdirectory=$stage" "-additionalcookeroptions=-NoAnalytics -DisablePlugins=AndroidFileServer"
if ($LASTEXITCODE -ne 0) { throw '打包失败，检查 AutomationTool 日志' }
$game = Join-Path $stage 'Windows/PGRCombat/Binaries/Win64/PGRCombat.exe'
./tools/test_puerts.ps1 -PackagedExe $game
./tools/test_puerts.ps1 -PackagedExe $game -D3D12
```

## 换机与编译

1. 在新机克隆项目仓库。安装 Git LFS 后在克隆目录运行 `git lfs install --local` 和 `git lfs pull`。若尚无可克隆远端，可通过自己的离线介质转移该项目的 Git bundle，见下文。
2. 阅读项目的 `docs/validation.md`，按需进入 `docs/research/`。私人录像与原游戏安装独立保管。编译最小 UE 工程无需安装原游戏。
3. 安装 Epic 的 UE 5.8.4 Win64 发行版与 VS 的“使用 C++ 的游戏开发”工作负载，勾选 v143 x64/x86 工具与 Windows SDK 10.0.22621.0。命令行构建不依赖 Visual Studio 的具体付费版本。使用下列命令核对新安装的 `Build.version`。
4. 将本机引擎路径赋给当前 PowerShell 会话中的 `UE_ROOT`。它只在本机使用。以 `Build.bat` 和项目文件的显式路径编译，可以在未建立 Windows 双击关联时运行。仓库中的 `EngineAssociation` 为 `5.8`。

```powershell
$env:UE_ROOT = Read-Host '输入引擎根目录（其下有 Engine 文件夹）'
$project = (Resolve-Path './PGRCombat.uproject').Path
$build = Join-Path $env:UE_ROOT 'Engine/Build/BatchFiles/Build.bat'
$editor = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logs = (New-Item -ItemType Directory -Force './Saved/Logs').FullName
Get-Content (Join-Path $env:UE_ROOT 'Engine/Build/Build.version') -Raw

& $build PGRCombatEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE "-Log=$logs/PGRBuild.log"
if ($LASTEXITCODE -ne 0) { throw 'Editor target 构建失败，请检查 PGRBuild.log' }
& $build PGRCombat Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE "-Log=$logs/PGRGameBuild.log"
if ($LASTEXITCODE -ne 0) { throw 'Game target 构建失败，请检查 PGRGameBuild.log' }
```

本机两个 target 都曾实际编译并链接成功。Editor 模块为 `Binaries/Win64/UnrealEditor-PGRCombat.dll`，Game target 的链接输出为 `Binaries/Win64/PGRCombat.exe`。

## 重跑启动验证

在同一 PowerShell 会话继续执行。两次都使用编辑器宿主的 `-game` 模式。工程在 `Config/DefaultEngine.ini` 中指定训练地图 `/Game/Maps/Training/TrainingFighter/L_CombatTraining`。运行进入 Tick 后，`-seconds` 使引擎发出正常退出请求。首次启动和着色器准备可额外耗时。

```powershell
$common = @($project, '-game', '-unattended', '-NoSplash', '-NoSound', '-NoAnalytics', '-NoLiveCoding')
& $editor @common -nullrhi -seconds=4 "-ABSLOG=$logs/PGRSmokeNull.log"
if ($LASTEXITCODE -ne 0) { throw 'Null RHI 启动失败' }
& $editor @common -d3d12 -windowed -ResX=640 -ResY=360 -seconds=8 "-ABSLOG=$logs/PGRSmokeD3D12.log"
if ($LASTEXITCODE -ne 0) { throw 'D3D12 启动失败' }
Get-ChildItem $logs -Filter 'PGRSmoke*.log' | Select-String -Pattern 'verbatimrhiname=|Bringing World /Game/Maps/Training/TrainingFighter/L_CombatTraining|Engine is initialized|RequestExitWithStatus|LogExit: Exiting'
```

两份日志应各自显示预期 RHI（Null 或 D3D12）、地图进入游玩、引擎完成初始化和正常退出。两个编辑器宿主测试均返回 0。首次测试出现 Enhanced Input 本地玩家配置警告，补上 `Config/DefaultInput.ini` 后两种启动都已复测。加入 Puerts 后独立包的结果另见[验证记录](validation.md)。

## 可选的离线迁移

在旧机把已提交的 Git 历史导出至仓库外的个人存储。新机将路径换成本地介质位置。Git bundle 包含提交和 Git 引用。如果后续新增 Git LFS 对象，另行从有权限的 LFS 远端拉取，或随离线备份转移所需的 LFS 对象。本机安装目录、凭据、原始素材与构建缓存均独立于 Git 历史。

```powershell
# 旧机，在仓库根目录执行
git bundle create ../PGR-Combat-UE5.bundle --all
git bundle verify ../PGR-Combat-UE5.bundle
# 新机，在目标父目录执行，并按介质位置调整 bundle 的路径
git clone ./PGR-Combat-UE5.bundle PGR-Combat-UE5
```
