#include "UIManager.h"
#include "NetworkManager.h"

UIManager* UIManager::instance = NULL;
NetworkManager* UIManager::netManager = NULL;


UIManager::UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	instance = this;
	netManager = new NetworkManager();

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_DOWNLOAD_APP, szWindowClass, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_CHILD_STATIC_NAME, szChildStaticWindowClass, MAX_LOADSTRING);
	RegisterWindowClass(hInstance);
	ATOM a = RegisterChildWindowClass(hInstance);
	roothWnd = InitInstance(hInstance, nCmdShow);
	// Perform application initialization:
	if (roothWnd == NULL)
	{
		return;
	}

	//speedTxt = CreateWindow(L"STATIC", L"SPEED", WS_VISIBLE | WS_CHILDWINDOW | SS_CENTER, 10, 10, 200, 20, roothWnd, NULL, hInstance, NULL);


	dlChildWindow = CreateWindow(szChildStaticWindowClass, L"DL_SPEED",  WS_VISIBLE | WS_CHILDWINDOW | WS_BORDER | WS_EX_CLIENTEDGE, 10, 10, 100, 20, roothWnd, NULL, hInstance, NULL);
	ulChildWindow = CreateWindow(szChildStaticWindowClass, L"UL_SPEED", WS_VISIBLE | WS_CHILDWINDOW | WS_BORDER | WS_EX_CLIENTEDGE, 110, 10, 100, 20, roothWnd, NULL, hInstance, NULL);

	/*dlTxt = CreateWindow(L"TEXT", L"DL_SPEED", WS_VISIBLE | WS_CHILDWINDOW | SS_OWNERDRAW, 0, 0, 200, 20, dlParent, NULL, hInstance, NULL);
	ulTxt = CreateWindow(L"STATIC", L"UL_SPEED", WS_VISIBLE | WS_CHILDWINDOW | SS_OWNERDRAW, 0, 0, 200, 20, ulParent, NULL, hInstance, NULL);*/

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
	delete netManager;
}

void UIManager::UpdateInfo()
{
	WCHAR dlBuf[100];
	WCHAR ulBuf[100];
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));
	if (!interfaces)
	{
		SendMessage(instance->roothWnd, WM_CLOSE, NULL, NULL);
		return;
	}
	while (instance->running)
	{
		std::tuple<double, double> speedInfo = netManager->GetAdaptorInfo(instance->roothWnd, interfaces);

		double dl = std::get<0>(speedInfo);
		double ul = std::get<1>(speedInfo);

		double dlMbps = dl / DIV_FACTOR;
		double ulMbps = ul / DIV_FACTOR;

		bool dlKbps = false;
		bool ulKbps = false;

		//If <1 Mbps, display as Kbps
		if (dlMbps < 1.0)
		{
			dl /= KILOBYTE;
			dlKbps = true;
		}
		else
		{
			dl = dlMbps;
		}

		if (ulMbps < 1.0)
		{

			ul /= KILOBYTE;
			ulKbps = true;
		}
		else
		{
			ul = ulMbps;
		}

		swprintf_s(dlBuf, L"↓ %.1lf %s", dl, dlKbps ? L"kbps" : L"Mbps");
		swprintf_s(ulBuf, L"↑ %.1lf %s",ul, ulKbps ? L"kbps" : L"Mbps");

		SetWindowText(instance->dlChildWindow, dlBuf);
		SetWindowText(instance->ulChildWindow, ulBuf);

		memset(dlBuf, 0, 100);
		memset(ulBuf, 0, 100);

		std::this_thread::sleep_for(std::chrono::microseconds(ONE_SECOND));
	}

	free(interfaces);
}

ATOM UIManager::RegisterWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LOGO));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = L"";//MAKEINTRESOURCEW(IDC_NAME);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_LOGO));

	return RegisterClassEx(&wcex);
}

ATOM UIManager::RegisterChildWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW | WS_EX_CLIENTEDGE;
	wcex.lpfnWndProc = ChildProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LOGO));
	wcex.hCursor = NULL;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = L"";//MAKEINTRESOURCEW(IDC_NAME);
	wcex.lpszClassName = szChildStaticWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_LOGO));

	return RegisterClassEx(&wcex);
}

HWND UIManager::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE, szWindowClass, szTitle, WS_POPUP,
		CW_USEDEFAULT, 0, 220, 32, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return NULL;
	}
													//Opacity
	SetLayeredWindowAttributes(hWnd, RGB(50, 50, 50), 235, LWA_COLORKEY | LWA_ALPHA);

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
	trayIcon.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_LOGO));
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
	if (sel == 0)
	{
		DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), instance->roothWnd, About);
	}
}

//For child windows which hold DL/UL texts
LRESULT CALLBACK UIManager::ChildProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		break;
	}
	//Pass these on to the root window
	case WM_LBUTTONDOWN:
		WndProc(instance->roothWnd, message, wParam, lParam);
		break;
	case WM_LBUTTONUP:
		WndProc(instance->roothWnd, message, wParam, lParam);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);				//CHILD WINDOW BACKGROUND COLOUR
		FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(252, 248, 200)));
		SetBkMode(hdc, TRANSPARENT);
		if (hWnd == instance->dlChildWindow)
		{
			SetTextColor(hdc, RGB(200, 10, 10));
			DrawText(hdc, instance->dlBuf, lstrlenW(instance->dlBuf), &ps.rcPaint, DT_CENTER | DT_VCENTER);
		}
		else if (hWnd == instance->ulChildWindow)
		{
			SetTextColor(hdc, RGB(10, 200, 10));
			DrawText(hdc, instance->ulBuf, lstrlenW(instance->ulBuf), &ps.rcPaint, DT_CENTER | DT_VCENTER);
		}
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_SETTEXT:
	{
		//Intercept this message and handle it ourselves
		LPWSTR msg = (LPWSTR)lParam;

		if (hWnd == instance->dlChildWindow) 
		{
			wcscpy(instance->dlBuf, msg);
		}
		else if (hWnd == instance->ulChildWindow)
		{
			wcscpy(instance->ulBuf, msg);
		}
		RECT rc;
		GetClientRect(hWnd, &rc);
		//Force a repaint (Will call WM_PAINT)
		InvalidateRect(hWnd, &rc, FALSE);
		break;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

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
			AppendMenu(menu, MF_STRING, 0, L"About");
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

	case WM_CONTEXTMENU:
	{
		POINT point;
		GetCursorPos(&point);

		HMENU menu = CreatePopupMenu();
		AppendMenu(menu, MF_STRING, -1, L"Exit");
		AppendMenu(menu, MF_STRING, 0, L"About");
		SetForegroundWindow(instance->roothWnd); //TrackPopupMenu requires the parent window to be in foreground, otherwise the popupmenu won't be destroyed when clicking off it
		instance->OnSelectItem(TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, instance->roothWnd, NULL));
		break;
	}

	case WM_LBUTTONDOWN:
		//Restrict mouse input to current window
		SetCapture(hWnd);
		break;

	case WM_LBUTTONUP:
		//Window no longer requires all mouse input
		ReleaseCapture();
		break;

	case WM_MOUSEMOVE:
	{
		if (GetCapture() == hWnd)  //Check if this window has mouse input
		{
			RECT windowRect;
			POINT mousePos;
			GetWindowRect(hWnd, &windowRect);

			//Get the current mouse coordinates
			mousePos.x = (int)(short)LOWORD(lParam);
			mousePos.y = (int)(short)HIWORD(lParam);

			int windowHeight = windowRect.bottom - windowRect.top;
			int windowWidth = windowRect.right - windowRect.left;

			ClientToScreen(hWnd, &mousePos);

			//Set the window's new screen position
			MoveWindow(hWnd, mousePos.x - (windowWidth / 2), mousePos.y, windowWidth, windowHeight, TRUE);
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
		HDC hdc = BeginPaint(hWnd, &ps);			//BACKGROUND COLOUR
		FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(245,242,109)));
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
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
