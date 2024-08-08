#pragma 
#include "framework.h"
#include "Name.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "iostream";
#include "thread"

#include "chrono"
#include "shellapi.h"
#include <windowsx.h>
#include <tuple>

#define MAX_LOADSTRING 100
#define WM_TRAYMESSAGE (WM_USER + 1)
#define DIV_FACTOR (1024.0f * 1024.0f)
#define ONE_SECOND 1000000

class UIManager
{
public:
	static UIManager* instance;
	UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
	~UIManager();
private:
	HINSTANCE hInst;                                // current instance
	WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
	WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

	UINT64 lastDlCount = -1;
	UINT64 lastUlCount = -1;
	int cacheIndex = -1;
	bool running = false;

	HWND roothWnd;
	HWND speedTxt;
	NOTIFYICONDATA trayIcon;

	static void UpdateInfo();
	std::tuple<ULONG64, ULONG64> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces);
	ATOM MyRegisterClass(HINSTANCE hInstance);
	HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
	void OnSelectItem(int sel);
	static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

