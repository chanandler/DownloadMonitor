// DownloadMonitorInstaller.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "DownloadMonitorInstaller.h"
#include "shellapi.h"
#include "windowsx.h"

#define MAX_LOADSTRING 100
#define IDC_SVCCHK WM_USER + 1
#define IDC_CONTINUE WM_USER + 2

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND                InitInstance(HINSTANCE, int);
HWND hWnd;
HWND chckbox;
HWND continueBtn;
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
bool ExtractResourceToFile(LPCWSTR resName, LPCWSTR outPath);
void InstallService(LPCWSTR path);
void BeginInstall();
wchar_t username[512];
void OnInstallSuccess();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_DOWNLOADMONITORINSTALLER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	hWnd = InitInstance(hInstance, nCmdShow);
	if (!hWnd)
	{
		return FALSE;
	}

	DWORD bufSize = 512;
	BOOL result = GetUserName(&username[0], &bufSize);
	if (result)
	{
		wchar_t startupBuf[512];
		swprintf_s(startupBuf, L"C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\DownloadApp.exe", username);

	}
	else
	{
		MessageBox(hWnd, L"Failed getting username!", L"Error", MB_ICONERROR);
		return FALSE;
	}

	if (!wcscmp(lpCmdLine, L"-installsvc")) //Install service
	{
		const wchar_t* svcPath = L"C:\\Program Files\\DownloadApp";
		BOOL result = CreateDirectory(svcPath, NULL);

		if (result || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			WCHAR fullpath[256];
			swprintf_s(fullpath, L"%s\\DownloadMonitorSvc.exe", svcPath);

			if (ExtractResourceToFile(L"SERVICE_EXE", fullpath))
			{
				InstallService(fullpath);
			}
		}
		else
		{
			MessageBox(hWnd, L"Failed to create path!", L"Error", MB_ICONERROR);
		}

		OnInstallSuccess();
		return FALSE;
	}

	

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DOWNLOADMONITORINSTALLER));

	MSG msg;
	//Basic UI

	HWND title = CreateWindow(
		L"STATIC",  // Predefined class
		L"Install download monitor",      // Text 
		WS_VISIBLE | WS_CHILD,  // Styles 
		10,         // x position 
		10,         // y position 
		180,        // Width
		25,        // Height
		hWnd,     // Parent window
		NULL,       // No menu.
		(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.

	chckbox = CreateWindow(
		L"BUTTON",
		L"Install process monitor service (Requires elevation)",
		WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | BS_CHECKBOX,
		10,
		80,
		400,
		50,
		hWnd,
		(HMENU)IDC_SVCCHK,
		(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
		NULL);

	continueBtn = CreateWindow(
		L"BUTTON",
		L"Continue",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		10,
		150,
		100,
		50,
		hWnd,
		(HMENU)IDC_CONTINUE,
		(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
		NULL);

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

void OnInstallSuccess()
{
	wchar_t startupBuf[512];
	swprintf_s(startupBuf, L"C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\DownloadApp.exe", username);
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.lpFile = startupBuf;
	sei.nShow = SW_SHOWNORMAL;

	ShellExecuteExW(&sei);
	MessageBox(hWnd, L"Thankyou for installing DownloadMonitor!", L"Success", MB_ICONINFORMATION);
}

void BeginInstall()
{
	wchar_t startupBuf[512];
	swprintf_s(startupBuf, L"C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\DownloadApp.exe", username);

	if (!ExtractResourceToFile(L"APP_EXE", startupBuf))
	{
		MessageBox(hWnd, L"Failed to install application!", L"Error", MB_ICONERROR);
		ExitProcess(-1);
	}

	if (chckbox && Button_GetCheck(chckbox))
	{
		WCHAR path[MAX_PATH];

		if (GetModuleFileName(NULL, path, MAX_PATH))
		{
			SHELLEXECUTEINFO sei = { sizeof(sei) };
			sei.lpVerb = L"runas";
			sei.lpFile = path;
			sei.lpParameters = L"-installsvc";
			sei.nShow = SW_SHOWNORMAL;

			if (!ShellExecuteExW(&sei))
			{
				MessageBox(hWnd, L"Installing service requires elevation!", L"Error", MB_ICONERROR);
			}
		}
	}
	else
	{
		OnInstallSuccess();
	}

	ExitProcess(0);
}

void InstallService(LPCWSTR path)
{
	SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!scm)
	{
		return;
	}

	SC_HANDLE service = CreateServiceW(
		scm,
		L"DownloadMonitor_Service",
		L"Download Monitor Service",
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		path,
		NULL, NULL, NULL, NULL, NULL);

	if (service == NULL)
	{
		DWORD err = GetLastError();

		WCHAR buf[256];
		swprintf_s(buf, L"Failed to install service! Error %d", err);
		MessageBox(hWnd, buf, L"Error", MB_ICONERROR);
		return;
	}

	if (service)
	{
		BOOL started = StartServiceW(service, 0, NULL);

		if (!started)
		{
			MessageBox(hWnd, L"Failed to start service!", L"Error", MB_ICONERROR);

		}
		CloseServiceHandle(service);
	}

	CloseServiceHandle(scm);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DOWNLOADMONITORINSTALLER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DOWNLOADMONITORINSTALLER);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 500, 500, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_COMMAND:
		{
			if ((HWND)lParam == continueBtn) //IDC_CONTINUE isn't working for some reason
			{
				BeginInstall();
			}
			int wmId = LOWORD(wParam);
			switch (wmId)
			{
				/*case IDC_SVCCHK:
				{
					break;
				}*/
				case IDC_CONTINUE:
				{
					break;
				}
				case IDM_ABOUT:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
					break;
				}
				case IDM_EXIT:
				{
					DestroyWindow(hWnd);
					break;
				}
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

bool ExtractResourceToFile(LPCWSTR resName, LPCWSTR outPath)
{
	HRSRC hRes = FindResourceW(NULL, resName, RT_RCDATA);
	if (!hRes)
	{
		return false;
	}

	HGLOBAL hData = LoadResource(NULL, hRes);

	if (!hData)
	{
		return FALSE;
	}

	DWORD size = SizeofResource(NULL, hRes);
	LPVOID pData = LockResource(hData);

	HANDLE hFile = CreateFileW(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();

		WCHAR buf[256];
		swprintf_s(buf, L"Failed to extract application! Error %d", err);
		MessageBox(hWnd, buf, L"Error", MB_ICONERROR);
		return false;
	}

	DWORD written;
	WriteFile(hFile, pData, size, &written, NULL);
	CloseHandle(hFile);
	return written == size;
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
