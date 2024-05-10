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
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_NAME, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	hWnd = InitInstance(hInstance, nCmdShow);
	// Perform application initialization:
	if (hWnd == NULL)
	{
		return FALSE;
	}

	speedTxt = CreateWindow(L"STATIC", L"SPEED", WS_VISIBLE | WS_CHILDWINDOW | SS_CENTER, 10, 10, 150, 20, hWnd, NULL, hInstance, NULL);
	running = true;
	std::thread mainThread = std::thread(UpdateInfo);
	mainThread.detach();

	//Auto move the window to the top right of the screen
	//Could make this a configurable default?
	SetWindowPos(hWnd, NULL, 1200, 0, 0, 0, SWP_NOSIZE);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NAME));

	MSG msg;
	running = true;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	Shell_NotifyIcon(NIM_DELETE, &trayIcon);
	running = false;
	return (int)msg.wParam;
}

void UpdateInfo()
{
	WCHAR buf[100];
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));
	if (!interfaces)
	{
		SendMessage(hWnd, WM_CLOSE, NULL, NULL);
		return;
	}
	while (running)
	{
		std::tuple<ULONG64, ULONG64> speedInfo = GetAdaptorInfo(hWnd, interfaces);

		ULONG64 dl = std::get<0>(speedInfo);
		ULONG64 ul = std::get<1>(speedInfo);

		dl /= 1048576.0f; //1024 * 2
		ul /= 1048576.0f; 
		swprintf_s(buf, L"↓%llu Mbps | ↑%llu Mbps", dl, ul);
		SetWindowText(speedTxt, buf);
		memset(buf, 0, 100);

		std::this_thread::sleep_for(std::chrono::microseconds(1000000));
	}

	free(interfaces);
}

std::tuple<ULONG64, ULONG64> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces)
{
	ULONG64 dl = -1;
	ULONG64 ul = -1;

	if (GetIfTable2(interfaces) == NO_ERROR)
	{
		MIB_IF_ROW2* row;

		if (cacheIndex == -1)
		{
			for (int i = 0; i < interfaces[0]->NumEntries; i++)
			{
				row = &interfaces[0]->Table[i];
				if (row)
				{
					//Find wireless interface
					//if (row->Type != IF_TYPE_IEEE80211)
					if (row->InOctets <= 0 || row->MediaConnectState != MediaConnectStateConnected /*|| row->Type != MIB_IF_TYPE_ETHERNET*/)
					{
						continue;
					}

					cacheIndex = i; //Save the index so we can grab it immediately each time 
					break;
				}
			}
		}

		if (cacheIndex != -1)
		{
			row = &interfaces[0]->Table[cacheIndex];
			if (row)
			{
				dl = (row->InOctets * 8.0f) - lastDlCount;
				lastDlCount = row->InOctets * 8.0f;

				ul = (row->OutOctets * 8.0f) - lastUlCount;
				lastUlCount = row->OutOctets * 8.0f;

			}
		}
	}

	//GetIfTable2 allocates memory for the tables, which we must delete
	FreeMibTable(interfaces[0]);

	return std::make_tuple(dl, ul);
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NAME));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = L"";//MAKEINTRESOURCEW(IDC_NAME);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
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
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, szWindowClass, szTitle, WS_POPUP,
		CW_USEDEFAULT, 0, 175, 32, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return NULL;
	}

	DWORD currentStyle = GetWindowLong(hWnd, GWL_STYLE);

	SetWindowLong(hWnd, GWL_STYLE, (currentStyle & ~WS_MAXIMIZEBOX & ~WS_MINIMIZEBOX));

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	//Tray icon
	trayIcon.cbSize = sizeof(NOTIFYICONDATA);
	trayIcon.hWnd = hWnd;
	trayIcon.uID = 1;
	trayIcon.uCallbackMessage = WM_TRAYMESSAGE;
	trayIcon.uVersion = NOTIFYICON_VERSION;
	trayIcon.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
	LoadString(hInst, IDS_APP_TITLE, trayIcon.szTip, 128);
	trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	Shell_NotifyIcon(NIM_ADD, &trayIcon);

	
	return hWnd;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//


void OnSelectItem(int sel)
{
	if (sel == -1)
	{
		SendMessage(hWnd, WM_CLOSE, NULL, NULL);
	}
}


static int xClick;
static int yClick;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_TRAYMESSAGE:
		switch (lParam)
		{
		case WM_RBUTTONDOWN:
		{
			POINT point;
			GetCursorPos(&point);

			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, MF_STRING, -1, L"Exit");
			OnSelectItem(TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, hWnd, NULL));

			break;
		}
		default:
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		}
		break;

	case WM_LBUTTONDOWN:
		//Restrict mouse input to current window
		SetCapture(hWnd);

		//Get the click position
		xClick = LOWORD(lParam);
		yClick = HIWORD(lParam);
		break;

	case WM_LBUTTONUP:
		//Window no longer requires all mouse input
		ReleaseCapture();
		break;

	case WM_MOUSEMOVE:
	{
		if (GetCapture() == hWnd)  //Check if this window has mouse input
		{
			//Get the window's screen coordinates
			RECT rcWindow;
			GetWindowRect(hWnd, &rcWindow);

			//Get the current mouse coordinates
			int xMouse = LOWORD(lParam);
			int yMouse = HIWORD(lParam);

			//Calculate the new window coordinates
			int xWindow = rcWindow.left + xMouse - xClick;
			int yWindow = rcWindow.top + yMouse - yClick;

			//Set the window's new screen position (don't resize or change z-order)
			SetWindowPos(hWnd, NULL, xWindow, yWindow, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
	}
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
