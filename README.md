# Desktop Mate 闹钟即时触发器

> ⚠️ **需要先安装 Desktop Mate (Steam App 3301060)**。本工具是其 mod，不是独立闹钟。

一条命令，桌面角色立刻播放闹钟特效。无需等待时钟。

[▶️ 效果演示](https://github.com/Tukist/dm-alarm-trigger/releases/download/v1.0/Recording.2026-08-08.223949.mp4)

## 原理

```
pythonw dm_fire.py
    ↓
部署 version.dll 代理 → 启动游戏
    ↓
Hook AlarmManager.Update (主线程第一帧)
    ↓
重置 preHour=-1 preMinute=-1
    ↓
Update 检测闹钟匹配 → 特效即刻播放
```

## 依赖

- Windows x64
- Python 3 (Anaconda)
- Desktop Mate (Steam App 3301060)
- GCC (仅编译时需要)

## 使用

```cmd
D:\anaconda3\pythonw.exe dm_fire.py
```

或双击 `dm_fire.vbs`

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
