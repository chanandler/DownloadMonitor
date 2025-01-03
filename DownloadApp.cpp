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


//TODO:
//Fix high value difference when waking from sleep
//Basic auto-update
//Convert configmanager to unicode

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