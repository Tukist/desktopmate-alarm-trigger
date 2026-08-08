/**
 * version.dll proxy - Hook AlarmManager.Update (RVA from Il2CppDumper)
 * Update runs every frame AFTER Awake/Start, so AlarmManager is initialized
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HMODULE g_real_ver = NULL;
static void* g_ShowWindow_orig = NULL;
static LONG g_sw_hooked = 0;
static void* g_Update_orig = NULL;
static LONG g_triggered = 0;
static LONG g_inited = 0;
static unsigned char g_orig_bytes[14] = {0};  /* Saved original bytes */

void log_msg(const char* msg) {
    FILE* f=fopen("C:/Users/HP/Desktop/dm_mod.log","a");
    if(f){fprintf(f,"%s\n",msg);fflush(f);fclose(f);}
}

#define RVA_UPDATE 0x57A060  /* AlarmManager.Update from Il2CppDumper */

static void load_real(){
    if(g_real_ver)return;
    char sp[MAX_PATH];GetSystemDirectoryA(sp,MAX_PATH);
    strcat(sp,"\\version.dll");g_real_ver=LoadLibraryA(sp);
}

#define FW(ret,name,params,args) __declspec(dllexport) ret WINAPI name params{load_real();typedef ret(WINAPI*fn)params;fn f=(fn)GetProcAddress(g_real_ver,#name);return f?f args:(ret)0;}
FW(DWORD,GetFileVersionInfoSizeA,(LPCSTR a,LPDWORD b),(a,b))
FW(DWORD,GetFileVersionInfoSizeW,(LPCWSTR a,LPDWORD b),(a,b))
FW(BOOL,GetFileVersionInfoA,(LPCSTR a,DWORD b,DWORD c,LPVOID d),(a,b,c,d))
FW(BOOL,GetFileVersionInfoW,(LPCWSTR a,DWORD b,DWORD c,LPVOID d),(a,b,c,d))
FW(BOOL,VerQueryValueA,(LPCVOID a,LPCSTR b,LPVOID*c,PUINT d),(a,b,c,d))
FW(BOOL,VerQueryValueW,(LPCVOID a,LPCWSTR b,LPVOID*c,PUINT d),(a,b,c,d))
__declspec(dllexport) BOOL WINAPI GetFileVersionInfoByHandle(DWORD a,DWORD b,DWORD c,LPVOID d){load_real();void*fp=GetProcAddress(g_real_ver,"GetFileVersionInfoByHandle");typedef BOOL(WINAPI*fn)(DWORD,DWORD,DWORD,LPVOID);return fp?((fn)fp)(a,b,c,d):FALSE;}

static int hook_func(void* target, void* hook, void** orig) {
    DWORD old;
    if(!VirtualProtect(target,14,PAGE_EXECUTE_READWRITE,&old))return 0;
    /* Save original bytes for trampoline */
    memcpy(g_orig_bytes, target, 14);
    unsigned char*p=(unsigned char*)target;
    p[0]=0xFF;p[1]=0x25;p[2]=0x00;p[3]=0x00;p[4]=0x00;p[5]=0x00;
    *(void**)(p+6)=hook;
    VirtualProtect(target,14,old,&old);
    return 1;
}

/* Call the original function by temporarily restoring bytes, calling, then re-hooking */
typedef void (*Update_fn)(void*);
static Update_fn g_orig_fn = NULL;

static void Update_hook(void* __this);

static void CallOriginal(void* __this) {
    if(!g_orig_bytes[0]) return;
    DWORD old;
    void* target = (void*)((SIZE_T)GetModuleHandleA("GameAssembly.dll") + RVA_UPDATE);
    /* Restore original bytes */
    VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(target, g_orig_bytes, 14);
    VirtualProtect(target, 14, old, &old);
    /* Call */
    ((Update_fn)target)(__this);
    /* Re-hook */
    VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old);
    unsigned char*p=(unsigned char*)target;
    p[0]=0xFF;p[1]=0x25;p[2]=0x00;p[3]=0x00;p[4]=0x00;p[5]=0x00;
    *(void**)(p+6)=Update_hook;
    VirtualProtect(target, 14, old, &old);
}

typedef BOOL(WINAPI*ShowWindow_t)(HWND,int);

/* Hide the white startup frame */
static BOOL WINAPI ShowWindow_hook(HWND hwnd, int nCmdShow) {
    /* Make window transparent on first call */
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
        GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    return ((ShowWindow_t)g_ShowWindow_orig)(hwnd, nCmdShow);
}

/* AlarmManager.Update hook - runs on main thread every frame */
static void Update_hook(void* __this){
    static int frame=0;
    frame++;
    if(frame<10)goto call_orig;  /* Wait 10 frames for layout */
    if(!g_triggered){
        log_msg("Update hook fired!");
        *(int*)((unsigned char*)__this+0xD8)=-1;
        *(int*)((unsigned char*)__this+0xDC)=-1;
        char buf[128];
        snprintf(buf,sizeof(buf),"preHour=%d preMinute=%d",*(int*)((unsigned char*)__this+0xD8),*(int*)((unsigned char*)__this+0xDC));
        log_msg(buf);
        InterlockedExchange(&g_triggered,1);
    }
call_orig:
    CallOriginal(__this);
}

/* Background: wait for GA, then hook AlarmManager.Update */
DWORD WINAPI BgThread(LPVOID p){
    HMODULE ga=NULL;
    for(int i=0;i<120;i++){
        ga=GetModuleHandleA("GameAssembly.dll");
        if(ga)break;
        Sleep(1000);
    }
    if(!ga){log_msg("GA timeout");return 1;}
    log_msg("GA loaded, hooking Update...");
    
    SIZE_T base=(SIZE_T)ga;
    void* update_addr=(void*)(base+RVA_UPDATE);
    if(hook_func(update_addr,Update_hook,&g_Update_orig)){
        log_msg("Update hooked!");
    }else{
        log_msg("Update hook failed");
    }
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE h,DWORD r,LPVOID p){
    if(r==DLL_PROCESS_ATTACH){
        if(InterlockedExchange(&g_inited,1))return TRUE;
        log_msg("=== version.dll LOADED ===");
        HANDLE ht=CreateThread(NULL,0,BgThread,NULL,0,NULL);
        if(ht)CloseHandle(ht);
    }
    return TRUE;
}
