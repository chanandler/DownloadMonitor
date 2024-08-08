#include "UIManager.h"

UIManager* UIManager::instance = NULL;


UIManager::UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	instance = this;

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_NAME, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	roothWnd = InitInstance(hInstance, nCmdShow);
	// Perform application initialization:
	if (roothWnd == NULL)
	{
		return;
	}

	speedTxt = CreateWindow(L"STATIC", L"SPEED", WS_VISIBLE | WS_CHILDWINDOW | SS_CENTER, 10, 10, 160, 20, roothWnd, NULL, hInstance, NULL);
	running = true;
	std::thread mainThread = std::thread(UpdateInfo);
	mainThread.detach();

	//Auto move the window to the top right of the screen
	//Could make this a configurable default?
	SetWindowPos(roothWnd, HWND_TOPMOST, 1200, 0, 0, 0, SWP_NOSIZE);
}

UIManager::~UIManager()
{
	Shell_NotifyIcon(NIM_DELETE, &trayIcon);
	running = false;
}

void UIManager::UpdateInfo()
{
	WCHAR buf[100];
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));
	if (!interfaces)
	{
		SendMessage(instance->roothWnd, WM_CLOSE, NULL, NULL);
		return;
	}
	while (instance->running)
	{
		std::tuple<ULONG64, ULONG64> speedInfo = instance->GetAdaptorInfo(instance->roothWnd, interfaces);

		ULONG64 dl = std::get<0>(speedInfo);
		ULONG64 ul = std::get<1>(speedInfo);

		ULONG dlMbps = dl / DIV_FACTOR;
		ULONG ulMbps = ul / DIV_FACTOR;
		
		bool dlKbps = false;
		bool ulKbps = false;

		//If <1 Mbps, display as Kbps
		if(dlMbps == 0)
		{
			dl /= 1024.0f;
			dlKbps = true;
		}
		else 
		{
			dl = dlMbps;
		}

		if (ulMbps == 0) {

			ul /= 1024.0f;
			ulKbps = true;
		}
		else 
		{
			ul = ulMbps;
		}
		
		swprintf_s(buf, L"↓ %llu %s | ↑ %llu %s", dl, dlKbps ? L"Kbps" : L"Mbps", ul, ulKbps ? L"Kbps" : L"Mbps");
		SetWindowText(instance->speedTxt, buf);
		memset(buf, 0, 100);

		std::this_thread::sleep_for(std::chrono::microseconds(ONE_SECOND));
	}

	free(interfaces);
}

std::tuple<ULONG64, ULONG64> UIManager::GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces)
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
				//Convert to bits
				ULONG64 dlBits = (row->InOctets * 8.0f);
				if (lastDlCount == -1) //Fix massive delta when first running program and lastDl/UlCount is not set
				{
					lastDlCount = dlBits;
				}

				dl = dlBits - lastDlCount;
				lastDlCount = dlBits;

				ULONG64 ulBits = (row->OutOctets * 8.0f);

				if (lastUlCount == -1)
				{
					lastUlCount = ulBits;
				}

				ul = ulBits - lastUlCount;
				lastUlCount = ulBits;
			}
		}
	}

	//GetIfTable2 allocates memory for the tables, which we must delete
	FreeMibTable(interfaces[0]);

	return std::make_tuple(dl, ul);
}

ATOM UIManager::MyRegisterClass(HINSTANCE hInstance)
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

HWND UIManager::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, szWindowClass, szTitle, WS_POPUP,
		CW_USEDEFAULT, 0, 180, 32, nullptr, nullptr, hInstance, nullptr);

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

void UIManager::OnSelectItem(int sel)
{
	if (sel == -1)
	{
		SendMessage(roothWnd, WM_CLOSE, NULL, NULL);
	}
}

static int xClick;
static int yClick;
LRESULT CALLBACK UIManager::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
			SetForegroundWindow(instance->roothWnd); //TrackPopupMenu requires the parent window to be in foreground, otherwise the popupmenu won't be destroyed when clicking off it
			instance->OnSelectItem(TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, instance->roothWnd, NULL));

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
			SetWindowPos(hWnd, HWND_TOPMOST, xWindow, yWindow, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
	}
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), instance->roothWnd, About);
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

INT_PTR CALLBACK UIManager::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
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
