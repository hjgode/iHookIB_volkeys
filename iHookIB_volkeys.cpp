// iHookIB_volkeys.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "iHookIB_volkeys.h"

#include "hooks.h"
#include "registry.h"

#pragma message( "Compiling " __FILE__ )

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE           g_hInst;            // current instance
HWND                g_hWndMenuBar;      // menu bar handle

    TCHAR szAppName[] = L"iHookIB_volkeys 1.0";
    bool bForwardKey=false;
	bool bRestoreVolumeKeys=true;
    NOTIFYICONDATA nid;
	TCHAR szClassNameTargetWindow[MAX_PATH];// =L"IntermecBrowser"

// Forward declarations of functions included in this code module:
ATOM            MyRegisterClass(HINSTANCE, LPTSTR);
BOOL            InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

HICON g_hIcon[3];
void RemoveIcon(HWND hWnd);
void ChangeIcon(int idIcon);
HWND getTargetSubWindow(int* iResult);
HWND getTargetWindow();

// Global variables can still be your...friend.
// CIHookDlg* g_This            = NULL;         // Needed for this kludgy test app, allows callback to update the UI
HINSTANCE   g_hHookApiDLL   = NULL;         // Handle to loaded library (system DLL where the API is located)

HWND        g_hWndIEM6      = NULL;         // handle to browser component window
DWORD       g_dwTimer       = 1001;         //our timer id
UINT        g_uTimerId      = 0;            //timer id

// Global functions: The original Open Source
BOOL g_HookDeactivate();
BOOL g_HookActivate(HINSTANCE hInstance);

void doReadReg(){
	OpenCreateKey(L"Software\\Intermec\\iHookIB_volkeys");
	//read ClassName
	TCHAR szTemp[MAX_PATH];
	if(RegReadStr(L"ClassName", szTemp)==0)
		wsprintf(szClassNameTargetWindow, szTemp);
	else{
		wsprintf(szClassNameTargetWindow, L"IntermecBrowser");
		RegWriteStr(L"ClassName", szClassNameTargetWindow);
	}
	//forward keys
	DWORD dwTemp;
	if(RegReadDword(L"ForwardKeys", &dwTemp)==0)
		dwTemp==0?bForwardKey=false:bForwardKey=true;
	else{
		bForwardKey=false;
		dwTemp=0;
		RegWriteDword(L"ForwardKeys", &dwTemp);
	}
	//restore volume keys
	dwTemp=0;
	if(RegReadDword(L"RestoreVolKeys", &dwTemp)==0){
		if(dwTemp==0)
			bRestoreVolumeKeys=false;
		else
			bRestoreVolumeKeys=true;
	}
	else{
		bRestoreVolumeKeys=true;
		dwTemp=1;
		RegWriteDword(L"RestoreVolKeys", &dwTemp);
	}	
}

#ifndef VOLUME_UP_HOTKEY_ID
	#define VOLUME_UP_HOTKEY_ID 2
	#define VOLUME_DOWN_HOTKEY_ID 3
#endif

void registerVolumeKeys(){
	HWND hWndTaskBar = FindWindow(L"HHTaskBar", NULL);
	if(hWndTaskBar==NULL)
		return;
	/*
	RegisterHotKey(hWndTaskBar,
	(int)SOFTKEY1_HOTKEY_ID, // VK_F1
	(int)MOD_KEYUP,
	(int)VK_TSOFT1); // LEFT MENU

	RegisterHotKey(hWndTaskBar,
	(int)SOFTKEY2_HOTKEY_ID, // VK_F2
	(int)MOD_KEYUP,
	(int)VK_TSOFT2); // RIGHT MENU

	*/
	if(RegisterHotKey(hWndTaskBar,
		 (int)VOLUME_UP_HOTKEY_ID, // 2
		 (int)MOD_KEYUP,
		 (int)VK_TVOLUMEUP)){ // VOLUME UP
			 DEBUGMSG(1,(L"RegisterHotKey success for VK_TVOLUMEUP\n"));
	}
	else{
		DEBUGMSG(1,(L"RegisterHotKey failed for VK_TVOLUMEUP: %i\n", GetLastError()));
	}
	 if(RegisterHotKey(hWndTaskBar,
		 (int)VOLUME_DOWN_HOTKEY_ID, // 3
		 (int)MOD_KEYUP,
		 (int)VK_TVOLUMEDOWN)){ // VOLUME DOWN
			 DEBUGMSG(1,(L"RegisterHotKey success for VK_TVOLUMEDOWN\n"));
	}
	else{
		DEBUGMSG(1,(L"RegisterHotKey failed for VK_TVOLUMEDOWN: %i\n", GetLastError()));
	}
}

enum Volumes : int
{
    OFF = 0,
    LOW = 858993459,
    NORMAL = 1717986918,
    MEDIUM = -1717986919,
    HIGH = -858993460,
    VERY_HIGH = -1
};

DWORD volumes[]={0, 0x33333333, 0x66666666, 0x99999999, 0xCCCCCCCC, 0xFFFFFFFF};
int volumeCounts=6;

void increaseVolume(BOOL bIncrease){
	DWORD oldV=0;
	UINT32 newV=0;
	MMRESULT mRes =waveOutGetVolume(NULL, &oldV);
	if(mRes!=MMSYSERR_NOERROR){
		DEBUGMSG(1, (L"waveOutGetVolume failed with 0x%08x\n", mRes));
		return;
	}
	DEBUGMSG(1, (L"old volume is 0x%08x\n", oldV));
	int currLevel=oldV/0x33333333;
	if(oldV % 0x33333333 != 0)
		currLevel++;
	if(bIncrease){
		if(currLevel<5)
			currLevel++;
	}
	else{
		if(currLevel>0)
			currLevel--;
	}
	newV=currLevel * 0x33333333;
	DEBUGMSG(1, (L"new volume is 0x%08x\n", newV));
	waveOutSetVolume(NULL, newV);
	MessageBeep(MB_OK);
}

//
#pragma data_seg(".HOOKDATA")                                   //  Shared data (memory) among all instances.
    HHOOK g_hInstalledLLKBDhook = NULL;                     // Handle to low-level keyboard hook
    //HWND hWnd = NULL;                                         // If in a DLL, handle to app window receiving WM_USER+n message
#pragma data_seg()

#pragma comment(linker, "/SECTION:.HOOKDATA,RWS")       //linker directive

// The command below tells the OS that this EXE has an export function so we can use the global hook without a DLL
__declspec(dllexport) LRESULT CALLBACK g_LLKeyboardHookCallback(
   int nCode,      // The hook code
   WPARAM wParam,  // The window message (WM_KEYUP, WM_KEYDOWN, etc.)
   LPARAM lParam   // A pointer to a struct with information about the pressed key
) 
{
    /*  typedef struct {
        DWORD vkCode;
        DWORD scanCode;
        DWORD flags;
        DWORD time;
        ULONG_PTR dwExtraInfo;
    } KBDLLHOOKSTRUCT, *PKBDLLHOOKSTRUCT;*/
    
    // Get out of hooks ASAP; no modal dialogs or CPU-intensive processes!
    // UI code really should be elsewhere, but this is just a test/prototype app
    // In my limited testing, HC_ACTION is the only value nCode is ever set to in CE
    static int iActOn = HC_ACTION;
    bool processed_key=false;
    int iResult=0;
    if (nCode == iActOn) 
    { 
        PKBDLLHOOKSTRUCT pkbhData = (PKBDLLHOOKSTRUCT)lParam;

		HWND hwndTargetWindowHandle=getTargetWindow();//FindWindow(L"KeyTest3AK",L"KeyTest3AK");
        if(hwndTargetWindowHandle==NULL)  // if IE is not loaded or not in foreground or browser window not found
            return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);

        //we are only interested in FKey press/release
        if(pkbhData->vkCode == VK_F6 || pkbhData->vkCode ==VK_F7){
            DEBUGMSG(1,(L"found function key 0x%08x ...\n", pkbhData->vkCode));
            if(processed_key==false){
                if (wParam == WM_KEYUP)
                {
                    
					if(pkbhData->vkCode==VK_F7){
						increaseVolume(false);//do Volume down
						DEBUGMSG(1, (L"decreasing Volume\n"));
					}
					else if(pkbhData->vkCode==VK_F6){
						increaseVolume(true);//do Volume up
						DEBUGMSG(1, (L"increasing Volume\n"));
					}
                    processed_key=true;
                }
                else if (wParam == WM_KEYDOWN)
                {
                    //ignore WM_KEYDOWN
					processed_key=true;
                }
            }
        }
    }//nCode == iActOn
    else
        DEBUGMSG(1, (L"Got unknwon action code: 0x%08x\n", nCode));

    //shall we forward processed keys?
    if (processed_key)
    {
        processed_key=false; //reset flag
        if (bForwardKey){
            return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);
        }
        else
            return true;
    }
    else
        return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);

}

BOOL g_HookActivate(HINSTANCE hInstance)
{
    // We manually load these standard Win32 API calls (Microsoft says "unsupported in CE")
    SetWindowsHookEx        = NULL;
    CallNextHookEx          = NULL;
    UnhookWindowsHookEx = NULL;

    // Load the core library. If it's not found, you've got CErious issues :-O
    //TRACE(_T("LoadLibrary(coredll.dll)..."));
    g_hHookApiDLL = LoadLibrary(_T("coredll.dll"));
    if(g_hHookApiDLL == NULL) return false;
    else {
        // Load the SetWindowsHookEx API call (wide-char)
        //TRACE(_T("OK\nGetProcAddress(SetWindowsHookExW)..."));
        SetWindowsHookEx = (_SetWindowsHookExW)GetProcAddress(g_hHookApiDLL, _T("SetWindowsHookExW"));
        if(SetWindowsHookEx == NULL) return false;
        else
        {
            // Load the hook.  Save the handle to the hook for later destruction.
            //TRACE(_T("OK\nCalling SetWindowsHookEx..."));
            g_hInstalledLLKBDhook = SetWindowsHookEx(WH_KEYBOARD_LL, g_LLKeyboardHookCallback, hInstance, 0);
            if(g_hInstalledLLKBDhook == NULL) return false;
        }

        // Get pointer to CallNextHookEx()
        //TRACE(_T("OK\nGetProcAddress(CallNextHookEx)..."));
        CallNextHookEx = (_CallNextHookEx)GetProcAddress(g_hHookApiDLL, _T("CallNextHookEx"));
        if(CallNextHookEx == NULL) return false;

        // Get pointer to UnhookWindowsHookEx()
        //TRACE(_T("OK\nGetProcAddress(UnhookWindowsHookEx)..."));
        UnhookWindowsHookEx = (_UnhookWindowsHookEx)GetProcAddress(g_hHookApiDLL, _T("UnhookWindowsHookEx"));
        if(UnhookWindowsHookEx == NULL) return false;
    }
    //TRACE(_T("OK\nEverything loaded OK\n"));
    return true;
}


BOOL g_HookDeactivate()
{
    //TRACE(_T("Uninstalling hook..."));
    if(g_hInstalledLLKBDhook != NULL)
    {
        UnhookWindowsHookEx(g_hInstalledLLKBDhook);     // Note: May not unload immediately because other apps may have me loaded
        g_hInstalledLLKBDhook = NULL;
    }

    //TRACE(_T("OK\nUnloading coredll.dll..."));
    if(g_hHookApiDLL != NULL)
    {
        FreeLibrary(g_hHookApiDLL);
        g_hHookApiDLL = NULL;
    }
    //TRACE(_T("OK\nEverything unloaded OK\n"));
    return true;
}


int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
    MSG msg;

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow)) 
    {
        return FALSE;
    }

    HACCEL hAccelTable;
    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_IHOOKIE6));

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
    WNDCLASS wc;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IHOOKIE6));
    wc.hCursor       = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = szWindowClass;

    return RegisterClass(&wc);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;
    TCHAR szTitle[MAX_LOADSTRING];      // title bar text
    TCHAR szWindowClass[MAX_LOADSTRING];    // main window class name

    g_hInst = hInstance; // Store instance handle in our global variable

    // SHInitExtraControls should be called once during your application's initialization to initialize any
    // of the device specific controls such as CAPEDIT and SIPPREF.
    SHInitExtraControls();
#ifdef INHTML5
    #ifdef STAGING
        wsprintf(szTitle, L"%s", L"iHookStaging");
        wsprintf(szWindowClass, L"%s", L"iHookStaging");
    #else
        wsprintf(szTitle, L"%s", L"iHookIN5");
        wsprintf(szWindowClass, L"%s", L"iHookIN5");
    #endif
#else
    wsprintf(szTitle, L"%s", L"iHookIB_volkeys");
    wsprintf(szWindowClass, L"%s", L"iHookIB_volkeys");
#endif
    //LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING); 
    //LoadString(hInstance, IDC_IHOOKIE6, szWindowClass, MAX_LOADSTRING);

    //If it is already running, then focus on the window, and exit
    hWnd = FindWindow(szWindowClass, szTitle);  
    if (hWnd) 
    {
        // set focus to foremost child window
        // The "| 0x00000001" is used to bring any owned windows to the foreground and
        // activate them.
        SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
        return 0;
    } 

    if (!MyRegisterClass(hInstance, szWindowClass))
    {
        return FALSE;
    }

    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

	doReadReg();

    // When the main window is created using CW_USEDEFAULT the height of the menubar (if one
    // is created is not taken into account). So we resize the window after creating it
    // if a menubar is present
    if (g_hWndMenuBar)
    {
        RECT rc;
        RECT rcMenuBar;

        GetWindowRect(hWnd, &rc);
        GetWindowRect(g_hWndMenuBar, &rcMenuBar);
        rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
        
        MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
    }

    ShowWindow   (hWnd , SW_HIDE); // nCmdShow);
    UpdateWindow(hWnd);

    //Create a Notification icon
    HICON hIcon;
    hIcon=(HICON) LoadImage (g_hInst, MAKEINTRESOURCE (IHOOK_STARTED), IMAGE_ICON, 16,16,0);
    nid.cbSize = sizeof (NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE;
    // NIF_TIP not supported    
    nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
    nid.hIcon = hIcon;
    nid.szTip[0] = '\0';
    BOOL res = Shell_NotifyIcon (NIM_ADD, &nid);
    if(!res){
        DEBUGMSG(1 ,(L"Could not add taskbar icon. LastError=%i\r\n", GetLastError() ));
    }

    //load icon set
    g_hIcon[0] =(HICON) LoadImage (g_hInst, MAKEINTRESOURCE (IDI_ICON_BAD), IMAGE_ICON, 16,16,0);
    g_hIcon[1] =(HICON) LoadImage (g_hInst, MAKEINTRESOURCE (IDI_ICON_WARN), IMAGE_ICON, 16,16,0);
    g_hIcon[2] =(HICON) LoadImage (g_hInst, MAKEINTRESOURCE (IDI_ICON_OK), IMAGE_ICON, 16,16,0);

#ifdef DEBUG
    if (!res)
        DEBUGMSG(1, (L"InitInstance: %i\n", GetLastError()));
#endif
    
    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    //PAINTSTRUCT ps;
    //HDC hdc;
	static BOOL registerDone=false;
	int iResult = 0;
    static SHACTIVATEINFO s_sai;
    
    switch (message) 
    {
        case WM_COMMAND:
            wmId    = LOWORD(wParam); 
            wmEvent = HIWORD(wParam); 
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_HELP_ABOUT:
                    DialogBox(g_hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
                    break;
                case IDM_OK:
                    SendMessage (hWnd, WM_CLOSE, 0, 0);             
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_TIMER:  //used to update notification icon
            if(wParam==g_dwTimer){
                
                getTargetSubWindow(&iResult); //0=NotFound, 1=Found, 2=Found and Foreground
                DEBUGMSG(1,(L"Timer: getTargetSubWindow(), iResult=%i\n", iResult));
                ChangeIcon(iResult);
				if(bRestoreVolumeKeys){
					if(iResult==0 && registerDone==false){
						registerVolumeKeys();
						registerDone=true;
					}
					if(iResult!=0 && registerDone){
						registerDone=false;
					}
				}
            }
            return 0;
            break;
        case WM_CREATE:
            if (g_HookActivate(g_hInst))
            {
                MessageBeep(MB_OK);
                //system bar icon
                //ShowIcon(hWnd, g_hInst);
                DEBUGMSG(1, (L"Hook loaded OK"));
            }
            else
            {
                MessageBeep(MB_ICONEXCLAMATION);
                MessageBox(hWnd, L"Could not hook. Already running a copy of iHook3? Will exit now.", L"iHook3", MB_OK | MB_ICONEXCLAMATION);
                PostQuitMessage(-1);
            }
            g_uTimerId = SetTimer(hWnd, g_dwTimer, 5000, NULL);
            //TRACE(_T("Hook did not success"));
            return 0;

            SHMENUBARINFO mbi;

            memset(&mbi, 0, sizeof(SHMENUBARINFO));
            mbi.cbSize     = sizeof(SHMENUBARINFO);
            mbi.hwndParent = hWnd;
            mbi.nToolBarId = IDR_MENU;
            mbi.hInstRes   = g_hInst;

            if (!SHCreateMenuBar(&mbi)) 
            {
                g_hWndMenuBar = NULL;
            }
            else
            {
                g_hWndMenuBar = mbi.hwndMB;
            }

            // Initialize the shell activate info structure
            memset(&s_sai, 0, sizeof (s_sai));
            s_sai.cbSize = sizeof (s_sai);
            if(true){
                int iResult = 0;
                getTargetSubWindow(&iResult);
                DEBUGMSG(1,(L"Timer: getTargetSubWindow(), iResult=%i\n", iResult));
                ChangeIcon(iResult);
            }
            break;
        case WM_PAINT:
            PAINTSTRUCT ps;    
            RECT rect;    
            HDC hdc;     // Adjust the size of the client rectangle to take into account    
            // the command bar height.    
            GetClientRect (hWnd, &rect);    
            hdc = BeginPaint (hWnd, &ps);     
            DrawText (hdc, TEXT ("iHookIB_volkeys loaded"), -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);    
            EndPaint (hWnd, &ps);     
            return 0;
            break;
        case WM_DESTROY:
            CommandBar_Destroy(g_hWndMenuBar);
            //taskbar system icon
            RemoveIcon(hWnd);
            MessageBeep(MB_OK);
            g_HookDeactivate();
            //Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;

        case WM_ACTIVATE:
            // Notify shell of our activate message
            SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);
            break;
        case WM_SETTINGCHANGE:
            SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
            break;
        case MYMSG_TASKBARNOTIFY:
                switch (lParam) {
                    case WM_LBUTTONUP:
                        //ShowWindow(hWnd, SW_SHOWNORMAL);
                        SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOREPOSITION | SWP_SHOWWINDOW);
                        if (MessageBox(hWnd, L"Hook is loaded. End hooking?", szAppName, 
                            MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST)==IDYES)
                        {
                            g_HookDeactivate();
                            Shell_NotifyIcon(NIM_DELETE, &nid);
                            PostQuitMessage (0) ; 
                        }
                        ShowWindow(hWnd, SW_HIDE);
                    }
            return 0;
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            {
                // Create a Done button and size it.  
                SHINITDLGINFO shidi;
                shidi.dwMask = SHIDIM_FLAGS;
                shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN | SHIDIF_EMPTYMENU;
                shidi.hDlg = hDlg;
                SHInitDialog(&shidi);
            }
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, message);
            return TRUE;

    }
    return (INT_PTR)FALSE;
}

void ChangeIcon(int idIcon)
{
//    NOTIFYICONDATA nid;
    int nIconID=1;
    nid.cbSize = sizeof (NOTIFYICONDATA);
    //nid.hWnd = hWnd;
    //nid.uID = idIcon;//   nIconID;
    //nid.uFlags = NIF_ICON | NIF_MESSAGE;   // NIF_TIP not supported
    //nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
    nid.hIcon = g_hIcon[idIcon];    // (HICON)LoadImage (g_hInst, MAKEINTRESOURCE (ID_ICON), IMAGE_ICON, 16,16,0);
    //nid.szTip[0] = '\0';

    BOOL r = Shell_NotifyIcon (NIM_MODIFY, &nid);
    if(!r){
        DEBUGMSG(1 ,(L"Could not change taskbar icon. LastError=%i\r\n", GetLastError() ));
    }
}

void RemoveIcon(HWND hWnd)
{
    NOTIFYICONDATA nid;

    memset (&nid, 0, sizeof nid);
    nid.cbSize = sizeof (NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;

    Shell_NotifyIcon (NIM_DELETE, &nid);

}

//search for a child window with class name
static BOOL bStopScan=FALSE;
BOOL scanWindow(HWND hWndStart, TCHAR* szClass){
    HWND hWnd = NULL;
    HWND hWnd1 = NULL;  

    hWnd = hWndStart;

    TCHAR cszWindowString [MAX_PATH]; // = new TCHAR(MAX_PATH);
    TCHAR cszClassString [MAX_PATH]; //= new TCHAR(MAX_PATH);
    TCHAR cszTemp [MAX_PATH]; //= new TCHAR(MAX_PATH);
    
    BOOL bRet=FALSE;

    while (hWnd!=NULL && !bStopScan){
        //do some formatting

        GetClassName(hWnd, cszClassString, MAX_PATH);
        GetWindowText(hWnd, cszWindowString, MAX_PATH);

        wsprintf(cszTemp, L"\"%s\"  \"%s\"\t0x%08x\n", cszClassString, cszWindowString, hWnd);//buf);
        DEBUGMSG(1, (cszTemp));

        if(wcsicmp(cszClassString, szClass)==0){
            DEBUGMSG(1 , (L"\n################### found target window, hwnd=0x%08x\n\n", hWnd));
            //set global hwnd
            g_hWndIEM6=hWnd;    //store in global var
            hWnd=NULL;
            hWndStart=NULL;
            bRet=TRUE;
            bStopScan=TRUE;
            return TRUE;    //exit loop
        }
        // Find next child Window
        hWnd1 = GetWindow(hWnd, GW_CHILD);
        if( hWnd1 != NULL ){ 
            scanWindow(hWnd1, szClass);
        }
        //Find next neighbor window
        hWnd=GetWindow(hWnd,GW_HWNDNEXT);       // Get Next Window
    }
    return bRet;
}

/// look for IE window and test if in foreground
/// if found and in foreground, return window handle
/// else return NULL
/// result will be 0 or 1 or 2
/// 2 = target found and foreground
/// 1 = target found but not foreground
/// 0 = target not found
HWND getTargetSubWindow(int* iResult){
    HWND hwnd_iem = FindWindow(szClassNameTargetWindow, NULL);    // try IB window
    if((hwnd_iem!=NULL) && (hwnd_iem==GetForegroundWindow()))
        *iResult=2;
    else if(hwnd_iem!=NULL)
        *iResult=1;
    else
        *iResult=0;
	/*
#if DEBUG
	if(hwnd_iem==NULL){
		hwnd_iem=FindWindow(L"DesktopExplorerWindow", NULL);
		*iResult=0;
	}
	DWORD oldV=0;
	MMRESULT mRes =waveOutGetVolume(NULL, &oldV);
	if(mRes!=MMSYSERR_NOERROR){
		DEBUGMSG(1, (L"waveOutGetVolume failed with 0x%08x\n", mRes));
	}
	else
		DEBUGMSG(1, (L"old volume is 0x%08x\n", oldV));
#endif
	*/
    return hwnd_iem;
}

enum IETYPE{
    None = 0,
    IEMobile,
    IExplore
} myIETYPE;

HWND getTargetWindow(){
	//DEBUGMSG(1, (L"using hardcoded IEMobile window\n"));
	//g_hWndIEM6 = (HWND)0x7c0d28A0;
	//return;
	//TEST FOR Window
	g_hWndIEM6=FindWindow(szClassNameTargetWindow, NULL);

	/*
#if DEBUG
	if(g_hWndIEM6==NULL){
		g_hWndIEM6=FindWindow(L"DesktopExplorerWindow", NULL);
	}
#endif
	*/
	return g_hWndIEM6;  //return global var

}