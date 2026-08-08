# Desktop Mate 逆向工程完全参考手册

> 基于 Il2CppDumper v6.7.46 对 Desktop Mate (Steam App 3301060, IL2CPP v29) 的完整逆向分析。

## 一、工具链

### Il2CppDumper

```bash
# 下载
https://github.com/Perfare/Il2CppDumper/releases/download/v6.7.46/Il2CppDumper-net6-win-v6.7.46.zip

# 运行
Il2CppDumper.exe GameAssembly.dll global-metadata.dat output/
```

**输出文件**：
| 文件 | 大小 | 用途 |
|------|------|------|
| `dump.cs` | 29MB | 所有 C# 类型定义（77万行） |
| `il2cpp.h` | 45MB | C++ 结构体定义 + 字段偏移 |
| `script.json` | 91MB | 方法地址（RVA）+ 签名 |
| `stringliteral.json` | 1.5MB | 字符串常量表 |
| `DummyDll/` | - | 还原的 DLL（可用 dnSpy 查看） |

### pefile

```bash
pip install pefile
```

用于查看 `GameAssembly.dll` 的导出表。

### UnityPy

```bash
pip install UnityPy
```

用于提取 Unity 资源文件（贴图、GameObject 层级等）。

---

## 二、存档系统

### 存档位置

**注册表**：`HKEY_CURRENT_USER\Software\infiniteloop\DesktopMate`

**键值**：`SaveData_h967477940`（REG_BINARY）

内容是 hex 编码的 JSON 字符串（末尾有 `\x00` 终止符）。

### SaveData 结构

```
SaveData (TypeDefIndex: 12189)
├── alarmDataList : List<AlarmData>   // 0x10
├── seVol : float                     // 0x18
├── sizeValue : float                 // 0x1C
├── isNoTutorial : bool               // 0x20
├── presetSettingIndex : int          // 0x24
├── mascotType : AssetBundleType      // 0x28
└── languageType : LanguageType       // 0x2C
```

JSON 示例：
```json
{
  "alarmDataList": [
    {"hour":11, "minute":40, "isToggle":true, "alarmElement":{"instanceID":-1774}}
  ],
  "seVol": 1.0,
  "sizeValue": 1709.0,
  "isNoTutorial": false,
  "presetSettingIndex": 0,
  "mascotType": 2,
  "languageType": 1
}
```

### SaveManager 单例

```
SaveManager (TypeDefIndex: 12188) : MonoBehaviour
├── static Instance { get; set; }    // <Instance>k__BackingField (static)
└── SaveData { get; set; }           // <SaveData>k__BackingField (0x20)

方法:
├── Awake()            // 调用 Load()
├── Load()             // 从注册表读取存档
├── Save()             // 写入注册表
└── Delete()           // 删除存档
```

### Python 读写存档

```python
import winreg, json

# 读取
key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\infiniteloop\DesktopMate")
hex_data, _ = winreg.QueryValueEx(key, "SaveData_h967477940")
data = json.loads(hex_data[:hex_data.index(0)].decode('utf-8'))

# 写入
data["alarmDataList"] = [{"hour":12,"minute":0,"isToggle":True,"alarmElement":{"instanceID":-1774}}]
json_str = json.dumps(data, ensure_ascii=False).encode('utf-8') + b'\x00'
winreg.SetValueEx(key, "SaveData_h967477940", 0, winreg.REG_BINARY, json_str)
```

---

## 三、闹钟系统

### AlarmData

```
AlarmData (TypeDefIndex: 12119)
├── hour : int                    // 0x10
├── minute : int                  // 0x14
├── isToggle : bool               // 0x18
├── alarmElement : AlarmElement   // 0x20（指针）
└── GetString() : string
```

### AlarmElement

```
AlarmElement (TypeDefIndex: 12114) : MonoBehaviour
├── buttonImage : Image           // 0x20
├── toggle : Toggle               // 0x28
├── button : Button               // 0x30
└── textMesh : TextMeshProUGUI    // 0x38
```

### AlarmManager

```
AlarmManager (TypeDefIndex: 12118) : MonoBehaviour
├── [SerializeField] alarmUIPrefab : AlarmElement     // 0x20
├── [SerializeField] buttonAdd : Button              // 0x28
├── [SerializeField] buttonDelete : Button           // 0x30
├── [SerializeField] buttonSave : Button             // 0x38
├── [SerializeField] triggerHourUp : EventTrigger     // 0x40
├── [SerializeField] triggerMinuteUp : EventTrigger   // 0x48
├── [SerializeField] triggerHourDown : EventTrigger   // 0x50
├── [SerializeField] triggerMinuteDown : EventTrigger  // 0x58
├── [SerializeField] timerInputField : TMP_InputField // 0x60
├── [SerializeField] contentTransform : RectTransform // 0x68
├── [SerializeField] audioMixer : AudioMixer          // 0x70
├── [SerializeField] seSlider : Slider               // 0x78
├── [SerializeField] seSliderText : TextMeshProUGUI  // 0x80
├── [SerializeField] seAudioSource : AudioSource     // 0x88
├── [SerializeField] seHandleTrigger : SliderPointerUpTrigger // 0x90
├── [SerializeField] alarmSettingPage : MenuPage     // 0x98
├── tmpAlarmData : AlarmData                          // 0xA0  ← 关键！
├── [SerializeField] spriteAlarmOn : Sprite          // 0xA8
├── [SerializeField] spriteAlarmOff : Sprite         // 0xB0
├── [SerializeField] hourText : TextMeshProUGUI      // 0xB8
├── [SerializeField] minuteText : TextMeshProUGUI    // 0xC0
├── mouseButtonCount : int                            // 0xC8
├── longDownEvent : Action                            // 0xD0
├── preHour : int                                     // 0xD8  ← 闹钟检测用！
├── preMinute : int                                   // 0xDC  ← 闹钟检测用！
└── alarmAction : Action                              // 0xE0

属性:
├── alarmList : List<AlarmData> { get; }   // [SerializeField]

方法:
├── Start()                                           // RVA: 0x5791E0
├── Update()                                          // RVA: 0x57A060 ← 每帧检测闹钟
├── StopAlarm()                                       // RVA: 0x579740
├── CreateAlarmButton(AlarmData, bool)                // RVA: 0x577960
├── OnAlarmButton(AlarmElement, AlarmData)            // RVA: 0x578260
├── LoadSaveData()                                    // RVA: 0x577FC0
├── RemoveAlarm(AlarmData)                            // RVA: 0x578940
├── RemoveEmptyAlarmDatas()                           // RVA: 0x578A50
├── SortList()                                        // RVA: 0x578CC0
├── OpenAlarmSettingPage(AlarmElement, AlarmData)     // RVA: 0x5785B0
├── OnAlarmToggle(bool, AlarmElement, AlarmData)      // RVA: 0x5782B0
├── GetStringHourMinute(string)                       // RVA: 0x577D20
└── ApplyButtonText(AlarmData, TMP_InputField)        // RVA: 0x577860
```

### 闹钟检测逻辑（伪代码）

```csharp
void Update() {
    if (alarmList == null) return;
    foreach (var alarm in alarmList) {
        if (!alarm.isToggle) continue;
        
        var now = DateTime.Now;
        if (alarm.hour != now.Hour || alarm.minute != now.Minute) continue;
        
        // 防止同一分钟内重复触发
        if (preHour == now.Hour && preMinute == now.Minute) return;
        
        preHour = now.Hour;
        preMinute = now.Minute;
        
        // 触发闹钟！
        alarmAction?.Invoke();
        // 或者通过状态机转换到 AlarmState
    }
}
```

### ⚡ 关键发现：重置 preHour/preMinute 即可强制触发

将 `AlarmManager.preHour` 和 `AlarmManager.preMinute` 设为 -1，下一帧 Update 就会触发当前时间的闹钟。

### AlarmState

```
AlarmState (TypeDefIndex: 12142) : KStateMachine.StateBase<MainManager>
├── OnEnter() override    // RVA: 0x5743696 ← 播放闹钟动画+音效
├── OnUpdate() override   // RVA: 0x5744752
└── OnExit() override     // RVA: 0x574A7B0
```

---

## 四、主管理器 MainManager

```
MainManager (TypeDefIndex: 12156) : MonoBehaviour [DefaultExecutionOrder(-100)]
├── alarmManager : AlarmManager          // 0x2A0  ← 关键！
├── alarmState : AlarmState             // 0x320  ← 关键！
├── stateMachine : KStateMachine<MainManager> // 0x2E0
├── vrmRoot : GameObject                // 0x20
├── vrmAnimator : Animator              // 0x28
├── windowSizeSlider : Slider           // 0x220
├── windowLockToggle : Toggle           // 0xC0
├── isWindowLock : bool                 // 0xC8
├── uniWindowController : UniWindowController // 0x3B8
├── vrmInstance : Vrm10Instance         // 0x3C0
└── ... (200+ 字段)
```

### KStateMachine

```
KStateMachine<TOwner> (TypeDefIndex: 12135)
├── currentState : StateBase<TOwner>    // 当前状态
├── currentEnum : Enum                  // 状态枚举
└── stateDictionary                     // 所有状态
```

---

## 五、GameAssembly.dll 导出函数（关键 RVA）

| 函数 | RVA | 签名 |
|------|-----|------|
| `il2cpp_string_new` | `0x43E9E0` | `Il2CppString* (const char*)` |
| `il2cpp_string_new_len` | `0x43E9F0` | `Il2CppString* (const char*, int)` |
| `il2cpp_string_new_utf16` | `0x43EA00` | `Il2CppString* (const wchar_t*)` |
| `il2cpp_thread_attach` | `0x43EA10` | `Il2CppDomain* (Il2CppDomain*)` |
| `il2cpp_string_chars` | `0x43E9A0` | `wchar_t* (Il2CppString*)` |
| `il2cpp_string_length` | `0x43E9D0` | `int (Il2CppString*)` |

### 关键托管方法 RVA

| 方法 | RVA | 签名 |
|------|-----|------|
| `SaveManager.get_Instance` | `5863184` | `SaveManager_o* (MethodInfo*)` |
| `SaveManager.Load` | `5862656` | `void (SaveManager_o*, MethodInfo*)` |
| `SaveManager.get_SaveData` | `4956528` | `SaveData_o* (SaveManager_o*, MethodInfo*)` |
| `GameObject.Find` | `38012400` | `GameObject_o* (Il2CppString*, MethodInfo*)` |
| `GameObject.GetComponent` | `38012752` | `Component_o* (GameObject_o*, Type_o*, MethodInfo*)` |
| `AlarmManager.Update` | `0x57A060` | `void (AlarmManager_o*, MethodInfo*)` |
| `AlarmState.OnEnter` | `0x5743696` | `void (AlarmState_o*, MethodInfo*)` |

---

## 六、注入方案对比

### 方案 A：SaveManager.Load() 注入（远程线程）

```python
# Shellcode: get_Instance → Load(instance, NULL)
sc = bytearray(b'\x48\x83\xEC\x28')
sc += b'\x48\xB8' + struct.pack('<Q', base + 5863184) + b'\xFF\xD0'  # get_Instance
sc += b'\x48\x89\xC1\x48\x31\xD2'                                      # mov rcx,rax; xor rdx,rdx
sc += b'\x48\xB8' + struct.pack('<Q', base + 5862656) + b'\xFF\xD0'  # Load
sc += b'\x31\xC0\x48\x83\xC4\x28\xC3'                                  # ret
```

**结论**：❌ 成功率 ~50%，游戏可能崩溃。远程线程调 IL2CPP 函数不稳定。

### 方案 B：线程劫持 + GameObject.Find

**结论**：❌ 劫持破坏 IL2CPP 上下文，Find 返回 NULL。

### 方案 C：ShowWindow Hook + 组件扫描

**结论**：❌ 钩子执行太重导致游戏未响应；MainManager 未初始化时字段为空。

### 方案 D：version.dll 代理 + AlarmManager.Update Hook ✅

**最终可行方案**。

```
1. 编译 version.dll（转发所有 version.dll 导出到 System32）
2. 放入游戏目录，游戏启动时优先加载
3. DllMain 启动后台线程
4. 后台线程等 GameAssembly.dll 加载
5. Hook AlarmManager.Update (RVA 0x57A060)
6. 第10帧（布局稳定后）写 preHour=-1, preMinute=-1
7. 还原原始 Update，CallOriginal 触发闹钟检测
```

---

## 七、内存布局关键偏移

### Il2CppObject 头部

```
Il2CppObject:
├── klass* : pointer (8 bytes)    // offset 0x00
└── monitor : pointer (8 bytes)   // offset 0x08
```

所有 IL2CPP 托管对象的前 16 字节都是这个头部。

### GameObject 组件数组

Unity 2022 使用 `ComponentPair`（16 字节）存储组件：
```
ComponentPair:
├── component* : pointer (8 bytes)
└── typeIndex : int (4 bytes) + padding (4 bytes)
```

扫描方法（16 字节步进）：
```python
ca = read_ptr(hp, gameobject_ptr + 0x30)  # 组件数组指针
for j in range(10):
    comp = read_ptr(hp, ca + j * 16)       # ComponentPair.component
    if comp < 0x10000: break
    klass = read_ptr(hp, comp)              # 检查 klass 范围
    if klass > 0x7FFF00000000: continue     # 跳过 Unity 引擎组件
    # 检查特定字段
```

### List<T> 结构

```
List<T>:
├── klass* : pointer    // 0x00
├── monitor : pointer   // 0x08
├── _items : T[]*       // 0x10
├── _size : int         // 0x18
└── _version : int      // 0x1C
```

### CONTEXT 结构（x64）

```
CONTEXT:
├── P1Home ~ P6Home     // 0x00 ~ 0x28
├── ContextFlags : DWORD // 0x30 ← 设置前必须在此偏移！
├── MxCsr : DWORD        // 0x34
├── SegCs ~ SegSs        // 0x38 ~ 0x42
├── EFlags : DWORD       // 0x44
├── Dr0 ~ Dr7            // 0x48 ~ 0x70
├── Rax ~ R15            // 0x78 ~ 0xF0
├── Rip : QWORD          // 0xF8
└── Rsp : QWORD          // 0x98
```

---

## 八、提取的 UI 资源

12 张闹钟 UI 贴图，位于 `sharedassets0.assets`：

| 贴图 | 尺寸 | 用途 |
|------|------|------|
| `ALARM_SET_UI_HAICHI` | 280×346 | 闹钟设置背景 |
| `Alarm_btn_on` / `Alarm_btn_off` | 236×50 | 开关按钮 |
| `Alarm_btn_add` | 236×49 | 添加按钮 |
| `Alarm_btn_delete_jp/en` | 236×49 | 删除按钮 |
| `Alarm_btn_setalarm_jp/en` | 236×49 | 设置闹钟按钮 |
| `Alarm_btn_arrow` | 25×16 | 箭头图标 |
| `Alarm_time` | 234×49 | 时间显示 |
| `Menu_btn_Alarm_enable_jp/en` | 236×49 | 菜单闹钟开关 |

提取代码：
```python
from UnityPy import Environment
env = Environment("sharedassets0.assets")
for obj in env.objects:
    if obj.type.name == 'Texture2D' and 'Alarm' in obj.read().m_Name:
        obj.read().image.save(f"{name}.png")
```

---

## 九、已知问题和注意事项

1. **preHour/preMinute 的时序**：在首帧 Update 时，AlarmManager.Start() 已将 preHour 设为当前小时，preMinute 设为当前分钟。因此在第 1 帧 Hook 中写 -1，下一帧就会触发。但如果 Trigger 太早（ShowWindow 阶段），字段还是 0。

2. **双 DllMain**：version.dll 可能被加载两次（DesktopMate.exe 和子进程），需要用 `InterlockedExchange` 防止重复初始化。

3. **线程安全**：从远程线程（CreateRemoteThread）调用 IL2CPP 托管函数会导致不稳定或崩溃。必须从主线程调用。

4. **GameAssembly.dll 基址**：每次启动因 ASLR 不同，需通过 `Module32First` 动态获取。

5. **CONTEXT.ContextFlags**：偏移是 0x30，不是 0x00。

6. **WriteProcessMemory 权限**：需要 `PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ`。

7. **GitHub KnownDLLs**：`version.dll` 和 `winhttp.dll` 可能被 Windows KnownDLLs 保护。如果代理 DLL 不加载，检查注册表 `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs`。

---

## 十、提权 API 速查

```python
import ctypes
k32 = ctypes.WinDLL('kernel32', use_last_error=True)

# 进程
OpenProcess(0x0008|0x0010|0x0020|0x0400, False, pid)  # VM_OP|VM_R|VM_W|QUERY
VirtualAllocEx(hp, None, size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE)
WriteProcessMemory(hp, addr, data, len, &written)
ReadProcessMemory(hp, addr, buf, len, &read)
CreateRemoteThread(hp, None, 0, shellcode_addr, None, 0, None)

# 模块枚举
CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
Module32First/Module32Next

# 线程枚举 (PowerShell 方式)
Get-Process -Id <pid> | Select-Object -ExpandProperty Threads | Select-Object Id

# 线程劫持
OpenThread(THREAD_ALL_ACCESS, 0, tid)
SuspendThread/ResumeThread
GetThreadContext/SetThreadContext  # ContextFlags at offset 0x30!

# 函数 Hook
VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old)
# jmp [rip+0]; dq target_address → FF 25 00 00 00 00 <8-byte-addr>
```

## 十一、编译命令

```bash
# version.dll 代理
gcc -shared -O2 -o version.dll dm_alarm_mod.c -lkernel32 -luser32
```

## 十二、项目地址

https://github.com/Tukist/dm-alarm-trigger
