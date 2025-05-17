#include "Installer.h"

Installer* Installer::instance = nullptr;

Installer::Installer(HINSTANCE hInstance, HINSTANCE hPrevInstance)
{
	instance = this;
	hInst = hInstance;
	hPrevInst = hPrevInstance;
}

//TODO
//Attempt to close the process if it's open
//Attempt to update the service if it exists

bool Installer::Init(LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInst);
	UNREFERENCED_PARAMETER(lpCmdLine);
	
	// Initialize global strings
	LoadStringW(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInst, IDC_DOWNLOADMONITORINSTALLER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInst);

	wcscpy_s(appVer, L"0.98");
	wcscpy_s(installerVer, L"0.1");

	// Perform application initialization:
	hWnd = InitInstance(hInst, nCmdShow);
	if (!hWnd)
	{
		return false;
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
		return false;
	}

	if (!wcscmp(lpCmdLine, L"-installsvc")) //Install service
	{
		defferedSvcInstall = true;
		
		//return true;
	}

	return true;
}

int Installer::MainLoop()
{
	HACCEL hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_DOWNLOADMONITORINSTALLER));

	MSG msg;

	const WCHAR* titleFontName = L"Arial Rounded MT Bold";
	const WCHAR* descFontName = L"Lato Light";
	//Fonts
	titleFont = CreateNewFont(10, 23, (WCHAR*)&titleFontName);
	descFont = CreateNewFont(8, 20, (WCHAR*)&descFontName);

	//Create common controls we will re-use later

	title = CreateWindow(
		L"STATIC",  // Predefined class
		L"",      // Text 
		WS_VISIBLE | WS_CHILD,  // Styles 
		200,         // x position 
		10,         // y position 
		250,        // Width
		80,        // Height
		hWnd,     // Parent window
		NULL,       // No menu.
		hInst,
		NULL);      // Pointer not needed.

	WCHAR buf[256];
	swprintf_s(buf, L"DownloadMonitor installer v%s", installerVer);

	installerVersion = CreateWindow(
		L"STATIC",
		buf,
		WS_VISIBLE | WS_CHILD,
		250,
		430,
		205,
		22,
		hWnd,
		(HMENU)IDC_SVCCHK,
		hInst,
		NULL);

	desc = CreateWindow(
		L"STATIC",
		L"",
		WS_VISIBLE | WS_CHILD,
		200,
		90,
		250,
		80,
		hWnd,
		(HMENU)IDC_SVCCHK,
		hInst,
		NULL);

	chckbox = CreateWindow(
		L"BUTTON",
		L"Install process monitor service",
		WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | BS_CHECKBOX,
		200,
		160,
		250,
		50,
		hWnd,
		(HMENU)IDC_SVCCHK,
		hInst,
		NULL);

	nextBtn = CreateWindow(
		L"BUTTON",
		L"Next",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		350,
		360,
		100,
		50,
		hWnd,
		(HMENU)IDC_NEXT,
		hInst,
		NULL);

	backBtn = CreateWindow(
		L"BUTTON",
		L"Back",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		245,
		360,
		100,
		50,
		hWnd,
		(HMENU)IDC_BACK,
		hInst,
		NULL);

	cancelBtn = CreateWindow(
		L"BUTTON",
		L"Cancel",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		245,
		360,
		100,
		50,
		hWnd,
		(HMENU)IDC_CANCEL,
		hInst,
		NULL);

	closeBtn = CreateWindow(
		L"BUTTON",
		L"Close",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		350,
		360,
		100,
		50,
		hWnd,
		(HMENU)IDC_CANCEL,
		hInst,
		NULL);

	sideImage = CreateWindow(
		L"STATIC",
		L"ImageWindow",
		WS_CHILD | WS_GROUP | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE,
		0,
		0,
		180,
		500,
		hWnd,
		NULL,
		hInst,
		NULL);

	HBITMAP bmp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SIDE_ARROWS));
	SendMessage(sideImage, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)bmp);
	UpdateWindow(sideImage);

	SendMessage(title, WM_SETFONT, (WPARAM)titleFont, TRUE);
	SendMessage(desc, WM_SETFONT, (WPARAM)descFont, TRUE);

	if(defferedSvcInstall)
	{
		BuildUI(UIState::INSTALLING);

		const wchar_t* svcPath = L"C:\\Program Files\\DownloadApp";
		BOOL result = CreateDirectory(svcPath, NULL);

		if (result || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			WCHAR fullpath[256];
			swprintf_s(fullpath, L"%s\\DownloadMonitorSvc.exe", svcPath);

			CloseServiceIfExists();
			if (ExtractResourceToFile(L"SERVICE_EXE", fullpath))
			{
				bool svcResult = InstallService(fullpath);
				if(!svcResult)
				{
					BuildUI(UIState::FAILED);
				}
				else 
				{
					OnInstallSuccess();
				}
			}
			else
			{
				BuildUI(UIState::FAILED);
			}
		}
		else
		{
			BuildUI(UIState::FAILED);
		}
	}
	else
	{
		BuildUI(UIState::WELCOME);
	}

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	DeleteObject(bmp);
	DeleteObject(titleFont);
	DeleteObject(descFont);
	return (int)msg.wParam;
}

void Installer::BuildUI(UIState state)
{
	switch (state)
	{
		case UIState::WELCOME:
		{
			ShowWindow(nextBtn, SW_SHOW);
			ShowWindow(cancelBtn, SW_SHOW);
			ShowWindow(backBtn, SW_HIDE);
			ShowWindow(chckbox, SW_HIDE);
			ShowWindow(closeBtn, SW_HIDE);

			ShowWindow(title, SW_SHOW);
			Static_SetText(title, L"Welcome to the Download Monitor Setup Wizard");

			ShowWindow(desc, SW_SHOW);
			WCHAR buf[512];
			swprintf_s(buf, L"This will install Download Monitor v%s to your computer", appVer);
			Static_SetText(desc, buf);

			break;
		}
		case UIState::OPTIONS:
		{
			ShowWindow(nextBtn, SW_SHOW);
			ShowWindow(backBtn, SW_SHOW);
			ShowWindow(cancelBtn, SW_HIDE);
			ShowWindow(chckbox, SW_SHOW);
			ShowWindow(closeBtn, SW_HIDE);

			ShowWindow(title, SW_SHOW);
			Static_SetText(title, L"Customise your installation");

			ShowWindow(desc, SW_SHOW);
			Static_SetText(desc, L"The process monitor service requires elevated privileges to install");

			break;
		}
		case UIState::INSTALLING:
		{
			ShowWindow(nextBtn, SW_HIDE);
			ShowWindow(chckbox, SW_HIDE);
			ShowWindow(cancelBtn, SW_HIDE);
			ShowWindow(backBtn, SW_HIDE);
			ShowWindow(closeBtn, SW_HIDE);

			ShowWindow(title, SW_SHOW);
			Static_SetText(title, L"Installing your selection...");
			Static_SetText(desc, L"");

			//TODO progress bar
			break;
		}
		case UIState::COMPLETE:
		{
			ShowWindow(title, SW_SHOW);
			Static_SetText(title, L"Instalation complete");
			ShowWindow(closeBtn, SW_SHOW);
			ShowWindow(desc, SW_HIDE);

			Button_SetText(chckbox, L"Start DownloadMonitor on exit");
			ShowWindow(chckbox, SW_SHOW);

			break;
		}
		case UIState::FAILED:
		{
			ShowWindow(title, SW_SHOW);
			Static_SetText(title, L"Instalation failed");
			ShowWindow(closeBtn, SW_SHOW);
			ShowWindow(desc, SW_SHOW);
			Static_SetText(desc, L"Sorry, something went wrong with the instalation. Please try again");

			ShowWindow(chckbox, SW_HIDE);
			break;
		}
	}

	InvalidateRect(hWnd, NULL, TRUE);
	currentState = state;
}

void Installer::CloseServiceIfExists()
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SC_HANDLE hService = OpenService(hSCManager, L"DownloadMonitor_Service", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);

	SERVICE_STATUS status = {};
	ControlService(hService, SERVICE_CONTROL_STOP, &status);
	Sleep(1000); // Give it time to stop (optional: poll until stopped)
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
}

bool Installer::InstallService(LPCWSTR path)
{
	SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!scm)
	{
		return false;
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
		//We already have service, exe has been updated so just start it again
		service = OpenService(scm, L"DownloadMonitor_Service", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);

		if(service == NULL)
		{
			DWORD err = GetLastError();
			WCHAR buf[256];
			swprintf_s(buf, L"Failed to install service! Error %d", err);
			MessageBox(hWnd, buf, L"Error", MB_ICONERROR);

			return false;
		}
	}

	if (service)
	{
		BOOL started = StartServiceW(service, 0, NULL);

		if (!started)
		{
			MessageBox(hWnd, L"Failed to start service!", L"Error", MB_ICONERROR);
			CloseServiceHandle(service);
			return false;
		}
	}

	CloseServiceHandle(scm);
	return true;
}

HWND Installer::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, 0, 500, 500, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

ATOM Installer::MyRegisterClass(HINSTANCE hInstance)
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
	wcex.lpszMenuName = NULL;// MAKEINTRESOURCEW(IDC_DOWNLOADMONITORINSTALLER);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

void Installer::OnInstallSuccess()
{
	BuildUI(UIState::COMPLETE);
}

void Installer::LaunchDownloadMonitor()
{
	//TODO find a way to lanch this unelevated
	WCHAR startupBuf[512];
	swprintf_s(startupBuf, L"C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\DownloadApp.exe", username);
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.lpFile = startupBuf;
	sei.nShow = SW_SHOWNORMAL;
	ShellExecuteExW(&sei);
}

HFONT Installer::CreateNewFont(int width, int height, LPWSTR name)
{
	HFONT newFont = CreateFont(height, width, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, name);
	return newFont;
}

void Installer::BeginInstall()
{
	wchar_t startupBuf[512];
	swprintf_s(startupBuf, L"C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\DownloadApp.exe", username);

	system("taskkill /im DownloadApp.exe /f"); //TODO a less jank implementation of this
	Sleep(1000);

	if (!ExtractResourceToFile(L"APP_EXE", startupBuf))
	{
		BuildUI(UIState::FAILED);
		return;
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
			else
			{
				ExitProcess(0);
			}
		}
	}
	else
	{
		OnInstallSuccess();
	}
}

LRESULT CALLBACK Installer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			switch (wmId)
			{
				case IDC_NEXT:
				{
					instance->BuildUI(static_cast<UIState>(instance->currentState + 1));

					if(instance->currentState == UIState::INSTALLING)
					{
						instance->BeginInstall();
					}
					
					break;
				}
				case IDC_CANCEL:
				{
					DestroyWindow(hWnd);
					break;
				}
				case IDC_BACK:
				{
					if(instance->currentState > 0)
					{
						instance->BuildUI(static_cast<UIState>(instance->currentState - 1));
					}
					break;
				}
				case IDM_ABOUT:
				{
					DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
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
		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = GetDC(hWnd);
			SetBkMode(hdc, TRANSPARENT);

			ReleaseDC(hWnd, hdc);
			return (LRESULT)GetStockObject(HOLLOW_BRUSH);
		}
		break;
		case WM_DESTROY:
			if(Button_GetCheck(instance->chckbox))
			{
				instance->LaunchDownloadMonitor();
			}
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

bool Installer::ExtractResourceToFile(LPCWSTR resName, LPCWSTR outPath)
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
INT_PTR CALLBACK Installer::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
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
