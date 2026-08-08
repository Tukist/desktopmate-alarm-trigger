# Desktop Mate 闹钟即时触发器

> ⚠️ **需要先安装 Desktop Mate (Steam App 3301060)**。本工具是其 mod，不是独立闹钟。  
> **项目目的**：将 Desktop Mate 的闹钟特效触发逻辑分离为独立组件，使得无需等待系统时钟匹配、无需操作游戏 UI，一行命令即可触发角色播放闹钟动画和音效。可用于桌面提醒、自动化工作流等场景。

一条命令，桌面角色立刻播放闹钟特效。无需等待时钟。

[▶️ 观看演示视频](https://github.com/Tukist/desktopmate-alarm-trigger/releases/download/v1.0/Recording.2026-08-08.223949.mp4)

## 原理

### 闹钟触发机制

Desktop Mate 的闹钟检测在 `AlarmManager.Update()` 中每帧运行（RVA `0x57A060`）。核心逻辑是比对系统时间和闹钟列表，用 `preHour` 和 `preMinute` 两个字段防止同一分钟内重复触发：

```
如果 闹钟.hour == 当前小时 && 闹钟.minute == 当前分钟:
    如果 preHour != 当前小时 || preMinute != 当前分钟:
        preHour = 当前小时
        preMinute = 当前分钟
        播放闹钟特效
```

**关键漏洞**：如果把 `preHour` 和 `preMinute` 重置为 `-1`，下一次 `Update()` 就会无条件触发当前时间的闹钟。

### 如何修改这两个字段

`preHour` 和 `preMinute` 是 `AlarmManager` 对象的实例字段，偏移分别为 `0xD8` 和 `0xDC`。要修改它们，必须拿到 `AlarmManager` 实例，并且必须在游戏主线程操作（非主线程调 IL2CPP 函数会崩溃）。

### 代理注入流程

本工具通过 DLL 代理（`version.dll`）注入到游戏进程，利用 Windows 的 DLL 搜索顺序：游戏目录下的 `version.dll` 优先于 `System32` 的版本。整个流程分为 6 步：

| 步骤 | 执行者 | 操作 | 线程 | 说明 |
|------|--------|------|------|------|
| 1 | `dm_fire.py` | 将闹钟写入注册表 `SaveData_h967477940`，部署 `version.dll` 到游戏目录 | 本进程 | 闹钟设为当前分钟，`isToggle=true` |
| 2 | `dm_fire.py` | 启动 `DesktopMate.exe` | — | `version.dll` 随游戏加载到进程空间 |
| 3 | `version.dll` | `DllMain()` 保存主线程 ID，启动后台线程 | 主线程 | `InterlockedExchange` 防重复初始化 |
| 4 | `version.dll` 后台线程 | 循环调用 `GetModuleHandleA("GameAssembly.dll")` 等待游戏引擎加载，然后定位 `AlarmManager.Update`（基址 + `0x57A060`） | 后台线程 | Hook 前存档被 `SaveManager.Load()` 自动读取 |
| 5 | `version.dll` 后台线程 | 通过 `VirtualProtect` 修改内存保护，写入 `jmp [rip+0]` 指令（`FF 25 00 00 00 00` + 8 字节地址），将 `AlarmManager.Update` 跳转到 `Update_hook` | 后台线程 | 保存原始 14 字节到 `g_orig_bytes` |
| 6 | `Update_hook` | 第 10 帧时直接写入 `*(int*)(__this + 0xD8) = -1` 和 `*(int*)(__this + 0xDC) = -1`，然后还原原始指令调用原 `Update` | 主线程 | 前 10 帧等待布局稳定；`__this` 就是 `AlarmManager` 实例指针 |

第 6 帧的 `__this` 参数是 IL2CPP 传给实例方法的 `this` 指针，直接指向 `AlarmManager` 对象，因此不需要 `GameObject.Find` 就能拿到实例。原始 `Update` 被调用后会检测到闹钟匹配并触发特效。

### 为什么 Hook Update 而不是其他函数

- **ShowWindow**：太早，此时 `AlarmManager.Start()` 还未执行，字段全是 0，且 Hook 内做重活会卡死消息循环
- **GameObject.Find**：从远程线程调用返回 NULL；从主线程劫持中调用也会因破坏 IL2CPP 上下文而失败
- **SaveManager.Load()**：只能刷新闹钟列表，无法绕过 `preHour`/`preMinute` 的重复触发保护

## 依赖

| 依赖 | 版本要求 | 用途 | 如何获取 |
|------|---------|------|---------|
| Windows | x64, Win10+ | 运行环境 | — |
| Desktop Mate | Steam App 3301060 | 被注入的目标游戏 | Steam 购买 |
| Python | 3.10+ | `dm_fire.py` 运行环境 | https://python.org 或 Anaconda |
| GCC (MinGW-w64) | 13.0+ | 编译 `version.dll`（仅开发者） | MSYS2: `pacman -S mingw-w64-x86_64-gcc` |

> **普通用户只需双击 `dm_fire.exe`，无需安装任何依赖。** Release 中的 exe 是 PyInstaller 打包的独立可执行文件，Python 和 `version.dll` 均已内嵌。

## 使用

**方式一：下载 Release 中的 `dm_fire.exe`，双击运行。** 无需安装任何依赖。

> 首次运行 Windows 可能弹出 SmartScreen 警告，点击"更多信息" → "仍要运行"即可。

**方式二：Python 脚本**

```cmd
pythonw dm_fire.py
```

> 如果游戏不在自动检测的路径下，可以指定：
> ```cmd
> pythonw dm_fire.py "D:\Games\Desktop Mate"
> ```

## 编译

```bash
gcc -shared -O2 -o version.dll dm_alarm_mod.c -lkernel32 -luser32
```

## 文件

| 文件 | 用途 |
|------|------|
| `dm_fire.py` | Python 一键部署脚本 |
| `dm_alarm_mod.c` | version.dll 代理源码 |
| `version.dll` | 编译好的代理 DLL |
| `dm_fire.vbs` | 静默启动器 |

## 反向工程来源

基于 Il2CppDumper 对 Desktop Mate 的反向工程：
- `AlarmManager.Update` RVA: 0x57A060
- `GameAssembly.dll` 导出: `il2cpp_string_new` (0x43E9E0)
- `AlarmManager.preHour`: offset 0xD8
- `AlarmManager.preMinute`: offset 0xDC

## License

仅供学习研究使用。
