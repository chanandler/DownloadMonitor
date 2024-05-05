// Name.cpp : Defines the entry point for the application.
//
#pragma once
#define WIN32_NO_STATUS
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
ULONG64 GetAdaptorInfo(HWND hWnd);
void UpdateInfo();
HWND hWnd;
HWND speedTxt;

UINT64 lastCount = 0;
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
	HWND hWnd = InitInstance(hInstance, nCmdShow);
	// Perform application initialization:
	if (hWnd == NULL)
	{
		return FALSE;
	}

	speedTxt = CreateWindow(L"STATIC", L"SPEED", WS_VISIBLE | WS_CHILD | SS_LEFT, 10, 10, 100, 100, hWnd, NULL, hInstance, NULL);
	running = true;
	std::thread mainThread = std::thread(UpdateInfo);
	mainThread.detach();

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
	running = false;
	return (int)msg.wParam;
}

void UpdateInfo()
{
	int speed = GetAdaptorInfo(hWnd);

	WCHAR buf[100];
	swprintf_s(buf, L"%d", speed);

	while (running)
	{
		ULONG64 speed = GetAdaptorInfo(hWnd);
		speed /= 1048576.0f;
		swprintf_s(buf, L"%llu", speed);
		SetWindowText(speedTxt, buf);
		memset(buf, 0, 100);

		std::this_thread::sleep_for(std::chrono::microseconds(1000000));
	}
}

ULONG64 GetAdaptorInfo(HWND hWnd)
{
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));

	if (!interfaces)
	{
		return -1;
	}

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
					if (row->Type != IF_TYPE_IEEE80211)
					{
						continue;
					}

					cacheIndex = i;
					break;
				}
			}
		}

		if (cacheIndex != -1) 
		{
			row = &interfaces[0]->Table[cacheIndex];
			if (!row) 
			{
				return -1;
			}
			ULONG64 ret = (row->InOctets * 8.0f) - lastCount;
			lastCount = row->InOctets * 8.0f;

			//ULONG64 ret = row->ReceiveLinkSpeed;
			free(interfaces);
			return ret;
		}
	}


	//if (GetIfTable2(interfaces, &size, TRUE) != NO_ERROR)
	//{
	//	free(interfaces);

	//	return -1;
	//}
	//MIB_IFROW* pIfRow;
	//bool skipFirst = false;
	//for (int i = 0; i < interfaces->dwNumEntries; i++)
	//{

	//	if (pIfRow->dwType == IF_TYPE_IEEE80211 && (pIfRow->dwOperStatus != IF_OPER_STATUS_DISCONNECTED && pIfRow->dwOperStatus != IF_OPER_STATUS_NON_OPERATIONAL))
	//	{
	//		DWORD ret = pIfRow->dwSpeed;
	//		free(interfaces);
	//		return ret;
	//	}

	/* if (!skipFirst && row.dwSpeed > 0)
	 {
		 skipFirst = true;
	 }
	 else if(row.dwSpeed > 0)
	 {


	 }*/
	//}
	/*free(interfaces);*/

	return -1;
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
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NAME);
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

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return NULL;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

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
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
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
