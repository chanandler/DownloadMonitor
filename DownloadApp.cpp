// Name.cpp : Defines the entry point for the application.
//
#pragma once
#define WIN32_NO_STATUS
#define WM_TRAYMESSAGE (WM_USER + 1)
#include "framework.h"
#include "Name.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "iostream";
#include "thread"
//#include "Netioapi.h"
//#include "windows.h"

#include "chrono"
#include "shellapi.h"
#include <windowsx.h>
#include <tuple>

#include "UIManager.h"

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
std::tuple<ULONG64, ULONG64> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces);
void UpdateInfo();
HWND hWnd;
HWND speedTxt;
NOTIFYICONDATA trayIcon;

UINT64 lastDlCount = 0;
UINT64 lastUlCount = 0;
int cacheIndex = -1;
bool running = false;


//TODO:
//Save last window pos
//Have persistent config for things such as download units, window colour, etc.
//Add some kind of license key systsm to copy DU meter

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	UIManager* uiManager = new UIManager(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NAME));

	MSG msg;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	delete uiManager;
	return (int)msg.wParam;
}