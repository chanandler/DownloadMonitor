#include "UIManager.h"
#include "NetworkManager.h"
#include "ConfigManager.h"
#include "Commctrl.h"
#include "shlwapi.h"

UIManager* UIManager::instance = NULL;
NetworkManager* UIManager::netManager = NULL;
ConfigManager* UIManager::configManager = NULL;

UIManager::UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	instance = this;
	netManager = new NetworkManager();

	configManager = new ConfigManager(lpCmdLine);

	if (!configManager->ReadData()) //If init failed, kill program
	{
		MessageBox(NULL, L"Failed to initialise config, is the supplied path valid?", L"NetworkManager", MB_OK);
		exit(-1);
		return;
	}

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_DOWNLOAD_APP, szWindowClass, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_CHILD_STATIC_NAME, szChildStaticWindowClass, MAX_LOADSTRING);
	RegisterWindowClass(hInstance);
	RegisterChildWindowClass(hInstance);
	roothWnd = InitInstance(hInstance, nCmdShow);

	// Perform application initialization:
	if (roothWnd == NULL)
	{
		MessageBox(NULL, L"Failed to create window, shutting down!", L"NetworkManager", MB_OK);
		exit(-1); 
		return;
	}
	
	dlChildWindow = CreateWindow(szChildStaticWindowClass, L"DL_SPEED", WS_VISIBLE | WS_CHILDWINDOW | WS_BORDER | WS_EX_CLIENTEDGE, DL_INITIAL_X,
		CHILD_INITIAL_Y, CHILD_INITIAL_WIDTH, CHILD_INITIAL_HEIGHT, roothWnd, NULL, hInstance, NULL);
	ulChildWindow = CreateWindow(szChildStaticWindowClass, L"UL_SPEED", WS_VISIBLE | WS_CHILDWINDOW | WS_BORDER | WS_EX_CLIENTEDGE, UL_INITIAL_X,
		CHILD_INITIAL_Y, CHILD_INITIAL_WIDTH, CHILD_INITIAL_HEIGHT, roothWnd, NULL, hInstance, NULL);

	//Load upload/download bitmaps into HDC memory
	uploadIconInst = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(IDB_UP_ARROW), IMAGE_BITMAP, 0, 0, NULL);
	uploadIconHDC = CreateCompatibleDC(NULL);
	SelectObject(uploadIconHDC, uploadIconInst);
	GetObject((HGDIOBJ)uploadIconInst, sizeof(uploadIconBm), &uploadIconBm);

	downloadIconInst = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(IDB_DOWN_ARROW), IMAGE_BITMAP, 0, 0, NULL);
	downloadIconHDC = CreateCompatibleDC(NULL);
	SelectObject(downloadIconHDC, downloadIconInst);
	GetObject((HGDIOBJ)downloadIconInst, sizeof(downloadIconBm), &downloadIconBm);

	UpdateBitmapColours();

	//Do initial DPI update
	instance->UpdateBmScaleForDPI();
	instance->UpdateFontScaleForDPI();

	InitForDPI(roothWnd, ROOT_INITIAL_WIDTH, ROOT_INITIAL_HEIGHT, configManager->lastX, configManager->lastY, true);
	InitForDPI(dlChildWindow, CHILD_INITIAL_WIDTH, CHILD_INITIAL_HEIGHT, DL_INITIAL_X, CHILD_INITIAL_Y);
	InitForDPI(ulChildWindow, CHILD_INITIAL_WIDTH, CHILD_INITIAL_HEIGHT, UL_INITIAL_X, CHILD_INITIAL_Y);

	UpdatePosIfRequired();

	running = true;
	std::thread mainThread = std::thread(UpdateInfo);
	mainThread.detach();

	//Move to last known pos
	//SetWindowPos(roothWnd, HWND_TOPMOST, configManager->lastX, configManager->lastY, 0, 0, SWP_NOSIZE);
}

void UIManager::UpdateBitmapColours()
{
	//To achieve download/upload icons that match the selected text colour, we must:
	//Load in a bitmap with transparent areas set to black, and fillable areas set to white
	//Retrieve each pixel/bit in the map, using GetDIBits
	//Loop through each pixel and if it is equal to white, save the index as that check will fail subsequent times due to the colour being changed to the user's preference
	//Set all saved indexes to the required colour, and write this new data to the bitmap
	//
	//In ChildProc(), we draw the bitmaps via TransparentBlt, which allows us to pass in the colour used for transparency (in our case black)
	//Whenever a colour change is required, we run through again and apply the new colour to the cached indexes

	SetBmToColour(uploadIconBm, uploadIconInst, uploadIconHDC, *configManager->uploadTxtColour, uploadBMIndexes);
	SetBmToColour(downloadIconBm, downloadIconInst, downloadIconHDC, *configManager->downloadTxtColour, downloadBMIndexes);
}

//TODO use this or something similar to work out if our requested coords are off screen
void UIManager::GetMaxScreenRect()
{
	int currDPI = GetDpiForWindow(roothWnd);

	int l = GetSystemMetricsForDpi(SM_XVIRTUALSCREEN, currDPI);
	int r = GetSystemMetricsForDpi(SM_YVIRTUALSCREEN, currDPI);

	int w = GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, currDPI);
	int h = GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, currDPI);
}

void UIManager::SetBmToColour(BITMAP bm, HBITMAP bmInst, HDC hdc, COLORREF col, std::vector<int> &cacheArr)
{
	BITMAPINFO bmInfo = { 0 };

	bmInfo.bmiHeader.biWidth = bm.bmWidth;
	bmInfo.bmiHeader.biHeight = bm.bmHeight;
	bmInfo.bmiHeader.biPlanes = 1;
	bmInfo.bmiHeader.biBitCount = 32;
	bmInfo.bmiHeader.biSize = sizeof(bmInfo.bmiHeader);

	GetDIBits(hdc, bmInst, 0, 0, NULL, &bmInfo, DIB_RGB_COLORS);

	std::vector<COLORREF> pixels(bm.bmWidth * bm.bmHeight);

	GetDIBits(hdc, bmInst, 0, bmInfo.bmiHeader.biHeight, &pixels[0], &bmInfo, DIB_RGB_COLORS);

	if (cacheArr.size() == 0)
	{
		for (int i = 0; i < pixels.size(); i++)
		{
			if (pixels[i] == RGB(255, 255, 255)) //If this pixel is white
			{
				cacheArr.push_back(i); //Save the index
			}
		}
	}

	for (int i = 0; i < cacheArr.size(); i++)
	{
		pixels[cacheArr[i]] = COLORREFToRGB(col); //Change the colour
	}

	SetDIBits(hdc, bmInst, 0, bm.bmHeight, &pixels[0], &bmInfo, DIB_RGB_COLORS);
}

COLORREF UIManager::COLORREFToRGB(COLORREF Col)
{
	//COLORREF is so old it stores in BGR instead of RGB
	return RGB(GetBValue(Col), GetGValue(Col), GetRValue(Col));
}

UIManager::~UIManager()
{
	Shell_NotifyIcon(NIM_DELETE, &trayIcon);
	running = false;

	//Clean up bitmaps
	DeleteDC(downloadIconHDC);
	DeleteDC(uploadIconHDC);
	DeleteObject(downloadIconInst);
	DeleteObject(uploadIconInst);

	if (bmScaleInfo)
	{
		delete bmScaleInfo;
	}
	
	if (fontScaleInfo)
	{
		delete fontScaleInfo;
	}

	delete netManager;
	delete configManager;
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

		swprintf_s(dlBuf, L"%.1lf %s", dl, dlKbps ? L"kbps" : L"Mbps");
		swprintf_s(ulBuf, L"%.1lf %s", ul, ulKbps ? L"kbps" : L"Mbps");

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
		CW_USEDEFAULT, 0, ROOT_INITIAL_WIDTH, ROOT_INITIAL_HEIGHT, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return NULL;
	}

	UpdateOpacity(hWnd);

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

void UIManager::UpdateOpacity(HWND hWnd)
{
	SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), configManager->opacity, LWA_ALPHA);
}

void UIManager::OnSelectItem(int sel)
{
	//0 == No selection
	if (sel == EXIT)
	{
		SendMessage(roothWnd, WM_CLOSE, NULL, NULL);
	}
	if (sel == ABOUT)
	{
		DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), roothWnd, AboutProc);
	}
	if (sel == SETTINGS)
	{
		DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), roothWnd, SettingsProc);
	}
}

//For child windows which hold DL/UL texts
LRESULT CALLBACK UIManager::ChildProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

		HBRUSH brush = (HBRUSH)::GetStockObject(DC_BRUSH);
		SetDCBrushColor(hdc, *instance->configManager->childColour);

		FillRect(hdc, &ps.rcPaint, brush);
		SetBkMode(hdc, TRANSPARENT);


		LOGFONT* modFont = (LOGFONT*)malloc(sizeof(LOGFONT));
		if(modFont && instance->fontScaleInfo)
		{
			memcpy(modFont, instance->configManager->currentFont, sizeof(LOGFONT));

			modFont->lfWidth = instance->fontScaleInfo->width;
			modFont->lfHeight = instance->fontScaleInfo->height;
		}
		else
		{
			modFont = instance->configManager->currentFont;
		}

		HFONT txtFont = CreateFontIndirect(modFont);
		SelectObject(hdc, txtFont);

		BitmapScaleInfo* info = instance->bmScaleInfo;
		if (hWnd == instance->dlChildWindow)
		{
			TransparentBlt(hdc, info->xPos, info->yPos, info->width / ARROW_SIZE_DIV, info->height / ARROW_SIZE_DIV,
				instance->downloadIconHDC, 0, 0, instance->downloadIconBm.bmWidth, instance->downloadIconBm.bmHeight, RGB(0, 0, 0));

			SetTextColor(hdc, *instance->configManager->downloadTxtColour);

			ps.rcPaint.left += 15;
			DrawText(hdc, instance->dlBuf, lstrlenW(instance->dlBuf), &ps.rcPaint, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
		}
		else if (hWnd == instance->ulChildWindow)
		{
			TransparentBlt(hdc, info->xPos, info->yPos, info->width / ARROW_SIZE_DIV, info->height / ARROW_SIZE_DIV,
				instance->uploadIconHDC, 0, 0, instance->uploadIconBm.bmWidth, instance->uploadIconBm.bmHeight, RGB(0, 0, 0));

			SetTextColor(hdc, *instance->configManager->uploadTxtColour);
			ps.rcPaint.right += 15;
			DrawText(hdc, instance->ulBuf, lstrlenW(instance->ulBuf), &ps.rcPaint, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
		}
		EndPaint(hWnd, &ps);
		DeleteObject(txtFont);
		if(modFont)
		{
			free(modFont);
		}
		break;
	}
	case WM_SETTEXT:
	{
		//Intercept this message and handle it ourselves
		LPWSTR msg = (LPWSTR)lParam;

		if (hWnd == instance->dlChildWindow)
		{
			ZeroMemory(instance->dlBuf, 200);
			wcscpy(instance->dlBuf, msg);
		}
		else if (hWnd == instance->ulChildWindow)
		{
			ZeroMemory(instance->ulBuf, 200);
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
			//Forward onto WM_CONTEXTMENU, as it does the same thing
			WndProc(hWnd, WM_CONTEXTMENU, wParam, lParam);
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
		AppendMenu(menu, MF_STRING, EXIT, L"Exit");
		AppendMenu(menu, MF_STRING, ABOUT, L"About");
		AppendMenu(menu, MF_STRING, SETTINGS, L"Settings");
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
		instance->WriteWindowPos();
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
			DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), instance->roothWnd, AboutProc);
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
		HDC hdc = BeginPaint(hWnd, &ps);			//OUTER/PRIMARY COLOUR

		HBRUSH brush = (HBRUSH)::GetStockObject(DC_BRUSH);
		SetDCBrushColor(hdc, *instance->configManager->foregroundColour);

		FillRect(hdc, &ps.rcPaint, brush);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DPICHANGED:
	{
		//Update font and bitmap scale values
		instance->UpdateBmScaleForDPI();
		instance->UpdateFontScaleForDPI();

		//The message provides suggested scale/position via the lParam, so apply that
		instance->UpdateForDPI(hWnd, (RECT*)lParam);

		//Update the child windows scale to match our new DPI
		instance->InitForDPI(instance->dlChildWindow, CHILD_INITIAL_WIDTH, CHILD_INITIAL_HEIGHT, DL_INITIAL_X, CHILD_INITIAL_Y);
		instance->InitForDPI(instance->ulChildWindow, CHILD_INITIAL_WIDTH, CHILD_INITIAL_HEIGHT, UL_INITIAL_X, CHILD_INITIAL_Y);

		instance->UpdatePosIfRequired();

		break;
	}
	case WM_DISPLAYCHANGE:
		instance->UpdatePosIfRequired();
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void UIManager::UpdatePosIfRequired()
{
	if(!IsOffScreen())
	{
		return;
	}


	//TODO get the actual max width instead of guessing
	SetWindowPos(roothWnd, HWND_TOPMOST, 1200, 0, 0, 0, SWP_NOSIZE);
	WriteWindowPos();
}

void UIManager::UpdateForDPI(HWND hWnd, RECT* newRct)
{
	int xPos = newRct->left;
	int yPos = newRct->top;	
	
	int	scaledWidth = newRct->right - newRct->left;
	int	scaledHeight = newRct->bottom - newRct->top;
	
	SetWindowPos(hWnd, hWnd, xPos, yPos, scaledWidth, scaledHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	WriteWindowPos();
}

void UIManager::InitForDPI(HWND hWnd, int initialWidth, int initialHeight, int initialX, int initialY, bool dontScalePos)
{
	int currDPI = GetDpiForWindow(hWnd);

	int scaledX = dontScalePos ? initialX : MulDiv(initialX, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledY = dontScalePos ? initialY : MulDiv(initialY, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledWidth = MulDiv(initialWidth, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledHeight = MulDiv(initialHeight, currDPI, USER_DEFAULT_SCREEN_DPI);


	SetWindowPos(hWnd, hWnd, scaledX, scaledY, scaledWidth, scaledHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

bool UIManager::IsOffScreen()
{
	RECT rc; 
	GetWindowRect(instance->roothWnd, &rc);

	POINT p;
	p.x = rc.left;
	p.y = rc.top;
	HMONITOR hMon = MonitorFromPoint(p, MONITOR_DEFAULTTONULL);
	if (hMon == NULL)
	{
		//Point is off screen
		return true;
	}

	return false;
}

void UIManager::UpdateFontScaleForDPI()
{
	int currDPI = GetDpiForWindow(roothWnd);

	int scaledWidth = MulDiv(configManager->currentFont->lfWidth, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledHeight = MulDiv(configManager->currentFont->lfHeight, currDPI, USER_DEFAULT_SCREEN_DPI);

	if(fontScaleInfo)
	{
		delete fontScaleInfo;
	}

	fontScaleInfo = new FontScaleInfo(scaledWidth, scaledHeight);
}

void UIManager::UpdateBmScaleForDPI()
{
	int currDPI = GetDpiForWindow(roothWnd);

	int scaledX = MulDiv(ARROW_X_OFFSET, currDPI, USER_DEFAULT_SCREEN_DPI);

	int scaledY = MulDiv(ARROW_Y_OFFSET, currDPI, USER_DEFAULT_SCREEN_DPI);

	//Download and upload have the same dimensions so we can do this
	int width = MulDiv(instance->downloadIconBm.bmWidth, currDPI, USER_DEFAULT_SCREEN_DPI);
	int height = MulDiv(instance->downloadIconBm.bmHeight, currDPI, USER_DEFAULT_SCREEN_DPI);

	if(bmScaleInfo)
	{
		delete bmScaleInfo;
	}

	bmScaleInfo = new BitmapScaleInfo(scaledX, scaledY, width, height);
}

void UIManager::WriteWindowPos()
{
	RECT rect = { NULL };
	GetWindowRect(roothWnd, &rect);
	int x = rect.left;
	int y = rect.top;

	configManager->UpdateWindowPos(x, y);
}

INT_PTR CALLBACK UIManager::AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		HWND vTxt = GetDlgItem(hDlg, IDC_VER_TXT);
		if (vTxt != NULL)
		{
			SendMessage(vTxt, WM_SETTEXT, 0, (LPARAM) VERSION_NUMBER);
		}

		return (INT_PTR)TRUE;
	}
		
	

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

INT_PTR CALLBACK UIManager::SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCLOSE || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDC_PRIMARY_COLOUR || LOWORD(wParam) == IDC_SECONDARY_COLOUR)
		{
			bool isPrimary = LOWORD(wParam) == IDC_PRIMARY_COLOUR;
			COLORREF rgbResult = instance->ShowColourDialog(hDlg,
				isPrimary ? instance->configManager->foregroundColour : instance->configManager->childColour, CC_ANYCOLOR | CC_RGBINIT);

			if (isPrimary)
			{
				instance->configManager->UpdateForegroundColour(rgbResult);
			}
			else
			{
				instance->configManager->UpdateChildColour(rgbResult);
			}

			//Force repaint to apply colours
			instance->ForceRepaint();
		}
		else if (LOWORD(wParam) == IDC_RESET_COLOURS)
		{
			instance->configManager->ResetConfig();
			instance->UpdateOpacity(instance->roothWnd);
			instance->ForceRepaint();
			instance->UpdateBitmapColours();
			instance->WriteWindowPos(); //We reset the whole cfg so re-write this
		}
		else if (LOWORD(wParam) == IDC_OPACITY)
		{
			DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_OPACITY), hDlg, OpacityProc);
		}
		else if (LOWORD(wParam) == IDC_TEXT)
		{
			DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_TEXT), hDlg, TextProc);
		}
		break;
	}
	return (INT_PTR)FALSE;
}

COLORREF UIManager::ShowColourDialog(HWND owner, COLORREF* initCol, DWORD flags)
{
	CHOOSECOLOR colorStruct = { 0 };
	colorStruct.hwndOwner = owner;
	colorStruct.lStructSize = sizeof(CHOOSECOLOR);
	colorStruct.rgbResult = *initCol;
	colorStruct.lpCustColors = instance->configManager->customColBuf;
	colorStruct.Flags = flags;
	ChooseColor(&colorStruct);

	WORD h, l, s;
	ColorRGBToHLS(colorStruct.rgbResult , &h, &l, &s);
	COLORREF r = ColorHLSToRGB(h, l, s);
	//return colorStruct.rgbResult;
	return r;
}

INT_PTR CALLBACK UIManager::OpacityProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		HWND sliderCtrl = GetDlgItem(hDlg, IDC_OPACITY_SLIDER);
		SendMessage(sliderCtrl, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELPARAM(MIN_OPACITY, MAX_OPACITY));
		SendMessage(sliderCtrl, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)instance->configManager->opacity);
		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			HWND sliderCtrl = GetDlgItem(hDlg, IDC_OPACITY_SLIDER);
			int newVal = SendMessage(sliderCtrl, TBM_GETPOS, NULL, NULL);

			if (newVal <= MAX_OPACITY && newVal >= MIN_OPACITY)
			{
				instance->configManager->UpdateOpacity(newVal);
				instance->UpdateOpacity(instance->roothWnd);
				instance->ForceRepaint();
			}
			else
			{
				DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_INVALID_INPUT), hDlg, AboutProc); //Just re-use this proc as it does the same thing
			}

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK UIManager::TextProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDC_FONT)
		{
			DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_FONT_WARNING), hDlg, FontWarningProc);
		}
		else if (LOWORD(wParam) == IDC_UPLOAD_COL || LOWORD(wParam) == IDC_DOWNLOAD_COL)
		{
			bool isUploadCol = LOWORD(wParam) == IDC_UPLOAD_COL;

			COLORREF rgbResult = instance->ShowColourDialog(hDlg,
				isUploadCol ? instance->configManager->uploadTxtColour : instance->configManager->downloadTxtColour, CC_ANYCOLOR | CC_RGBINIT);

			if (isUploadCol)
			{
				instance->configManager->UpdateUploadTextColour(rgbResult);
			}
			else
			{
				instance->configManager->UpdateDownloadTextColour(rgbResult);
			}

			instance->UpdateBitmapColours();
		}
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK UIManager::FontWarningProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK) 
			{
				ShowWindow(hDlg, SW_HIDE);

				CHOOSEFONT fontStruct = { 0 };
				fontStruct.lStructSize = sizeof(CHOOSEFONT);
				fontStruct.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOVERTFONTS | CF_LIMITSIZE | CF_SCALABLEONLY;
				fontStruct.nSizeMin = 4;
				fontStruct.nSizeMax = 12;
				fontStruct.hwndOwner = hDlg;
				fontStruct.lpLogFont = instance->configManager->currentFont;

				if (ChooseFont(&fontStruct))
				{
					instance->configManager->UpdateFont(*fontStruct.lpLogFont);
				}
			}
			
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


void UIManager::ForceRepaint()
{
	RECT rc;
	GetClientRect(roothWnd, &rc);
	InvalidateRect(roothWnd, &rc, FALSE);
}

UINT_PTR CALLBACK UIManager::ColourPickerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY: //Intercept this message from the colour picker so we know when to apply the selection
		//Don't need to do it this way
		//instance->configManager->OnColourPickerComplete();
		break;
	}
	return (INT_PTR)FALSE;
}
