#!/usr/bin/env python3
"""Desktop Mate 0秒闹钟触发器"""
import sys,os,ctypes,subprocess,time,json,winreg,shutil
ctypes.windll.user32.ShowWindow(ctypes.windll.kernel32.GetConsoleWindow(),0)
from datetime import datetime

GAME=None
# Auto-detect Desktop Mate game path
import glob as _glob
candidates=[
    os.path.join(os.path.dirname(os.path.abspath(__file__)),"Desktop.Mate-P2P"),
    os.path.join(os.path.dirname(os.path.abspath(__file__)),"DesktopMate"),
    r"C:\Users\HP\Desktop\mike\Desktop.Mate-P2P",
]
# Steam library
for lib in [r"C:\Program Files (x86)\Steam\steamapps\common",
            r"D:\Steam\steamapps\common",
            r"E:\Steam\steamapps\common"]:
    if os.path.isdir(lib):
        candidates.append(os.path.join(lib,"Desktop Mate"))
        for d in _glob.glob(lib+"/*"):
            if 'desktop' in d.lower() and 'mate' in d.lower():
                candidates.append(d)
# Check current directory
for d in _glob.glob("*Desktop*Mate*"):
    candidates.append(os.path.abspath(d))

for c in candidates:
    if os.path.exists(os.path.join(c,"DesktopMate.exe")):
        GAME=c;break
if not GAME and len(sys.argv)>1:
    GAME=sys.argv[1]
if not GAME:
    import tkinter.messagebox as _mb
    _mb.showerror("错误","未找到 Desktop Mate 游戏。\n\n请将本程序放到游戏目录下运行，\n或使用命令行指定路径：\n  dm_fire.exe \"游戏路径\"")
    sys.exit(1)
EXE=os.path.join(GAME,"DesktopMate.exe")
REG=r"Software\infiniteloop\DesktopMate"
REGV="SaveData_h967477940"
LOG=r"C:\Users\HP\Desktop\dm_mod.log"

# Find version.dll
if getattr(sys,'frozen',False):
    PROXY=os.path.join(sys._MEIPASS,"version.dll")
else:
    PROXY=os.path.join(os.path.dirname(os.path.abspath(__file__)),"version.dll")

print("[1/3] Stop game + deploy proxy...")
subprocess.run("taskkill /F /IM DesktopMate.exe",capture_output=True,shell=True)
time.sleep(2)
dst=os.path.join(GAME,"version.dll")
if os.path.exists(dst):os.remove(dst)
shutil.copy(PROXY,dst)

t=datetime.now()
k=winreg.OpenKey(winreg.HKEY_CURRENT_USER,REG)
d,_=winreg.QueryValueEx(k,REGV);winreg.CloseKey(k)
data=json.loads(d[:d.index(0)].decode("utf-8"))
data["alarmDataList"]=[{"hour":t.hour,"minute":t.minute,"isToggle":True,"alarmElement":{"instanceID":-1774}}]
js=json.dumps(data,ensure_ascii=False).encode("utf-8")+"\x00".encode("utf-8")
k=winreg.OpenKey(winreg.HKEY_CURRENT_USER,REG,0,winreg.KEY_SET_VALUE)
winreg.SetValueEx(k,REGV,0,winreg.REG_BINARY,js);winreg.CloseKey(k)
print(f"[2/3] Alarm: {t.hour:02d}:{t.minute:02d}")

subprocess.Popen([EXE],cwd=GAME)
print("[3/3] Launching game...")
for i in range(60):
    time.sleep(1)
    if os.path.exists(LOG):
        with open(LOG,"r") as f:
            if "preHour=-1" in f.read():
                print("[OK] Alarm triggered!")
                break
    if i%10==9:print(f"   ...{i+1}s")
