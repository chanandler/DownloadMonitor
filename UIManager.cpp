#include "UIManager.h"
#include "NetworkManager.h"
#include "ConfigManager.h"
#include "ActivationManager.h"
#include "ThemeManager.h"
#include "shlwapi.h"

//TODO move this into manifest file along with DPI awareness setting
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

UIManager* UIManager::instance = NULL;
NetworkManager* UIManager::netManager = NULL;
ConfigManager* UIManager::configManager = NULL;
ActivationManager* UIManager::activationManager = NULL;
ThemeManager* UIManager::themeManager = NULL;
BOOL UIManager::hasChildMouseEvent = FALSE;
BOOL UIManager::hasRootMouseEvent = FALSE;

UIManager::UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	instance = this;
	netManager = new NetworkManager();
	configManager = new ConfigManager(lpCmdLine);
	activationManager = new ActivationManager();
	themeManager = new ThemeManager();

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
	LoadStringW(hInstance, IDC_POPUP_NAME, szPopupWindowClass, MAX_LOADSTRING);
	RegisterWindowClass(hInstance);
	RegisterChildWindowClass(hInstance);
	//RegisterPopupWindowClass(hInstance);
	roothWnd = InitInstance(hInstance, nCmdShow);

	// Perform application initialization:
	if (roothWnd == NULL)
	{
		MessageBox(NULL, L"Failed to create window, shutting down!", L"NetworkManager", MB_OK);
		exit(-1);
		return;
	}

	monitorFinder = new MonitorData();
	monitorFinder->BuildMonitorList();

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

	//netManager->GetProcessUsageTable();
	//ShowTopConsumersToolTip();

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

void UIManager::GetMaxScreenRect()
{
	int currDPI = GetDpiForWindow(roothWnd);

	int l = GetSystemMetricsForDpi(SM_XVIRTUALSCREEN, currDPI);
	int r = GetSystemMetricsForDpi(SM_YVIRTUALSCREEN, currDPI);

	int w = GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, currDPI);
	int h = GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, currDPI);
}

void UIManager::SetBmToColour(BITMAP bm, HBITMAP bmInst, HDC hdc, COLORREF col, std::vector<int>& cacheArr)
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
	delete activationManager;
	delete themeManager;
	delete monitorFinder;
}

void UIManager::UpdateInfo()
{
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));
	if (!interfaces)
	{
		SendMessage(instance->roothWnd, WM_CLOSE, NULL, NULL);
		return;
	}

	while (instance->running)
	{
		std::tuple<double, double> speedInfo = netManager->GetAdaptorInfo(instance->roothWnd, interfaces, (UCHAR*)instance->configManager->uniqueAddr);

		double dl = std::get<0>(speedInfo);
		double ul = std::get<1>(speedInfo);

		WCHAR* dlBuf = instance->GetStringFromBits(dl);
		WCHAR* ulBuf = instance->GetStringFromBits(ul);

		if (dlBuf)
		{
			SetWindowText(instance->dlChildWindow, dlBuf);
			free(dlBuf);
		}
		if (ulBuf)
		{
			SetWindowText(instance->ulChildWindow, ulBuf);
			free(ulBuf);
		}

		if (instance->popup != NULL) //Tick the popup if it exists
		{
			WCHAR pNames[POPUP_BUF_SIZE];
			memset(pNames, 0, POPUP_BUF_SIZE);

			std::vector<ProcessData*> topCnsmrs = netManager->GetTopConsumingProcesses();

			//Go through 0 -> max and re-use items that exist, or delete items that beyond the current vector size
			for (int i = 0; i < MAX_TOP_CONSUMERS; i++)
			{
				LVITEM lvI = { 0 };
				lvI.iItem = i;
				lvI.iSubItem = 0;

				//See if we have an item at this index
				BOOL existingItem = ListView_GetItem(instance->popup, &lvI);

				if (existingItem == TRUE && i > topCnsmrs.size() - 1) //Delete ones beyond the size of the process list
				{
					ListView_DeleteItem(instance->popup, lvI.iItem);
				}
				else if (topCnsmrs.size() > 0 && i < topCnsmrs.size())
				{
					ProcessData* pData = topCnsmrs[i];
					LPWSTR exeName = PathFindFileName(topCnsmrs[i]->name);

					if (exeName != NULL)
					{
						lvI.mask = LVIF_TEXT | LVIF_PARAM;
						lvI.cchTextMax = MAX_PATH;
						lvI.iItem = i;
						lvI.iSubItem = 0;
						lvI.pszText = exeName;
						lvI.lParam = (LPARAM)(double)(topCnsmrs[i]->inBits + topCnsmrs[i]->outBits);
					}

					if (existingItem == FALSE)
					{
						SendMessage(instance->popup, LVM_INSERTITEM, 0, (LPARAM)&lvI);
					}
					else
					{
						SendMessage(instance->popup, LVM_SETITEM, 0, (LPARAM)&lvI);
					}

					lvI.mask = LVIF_TEXT;

					WCHAR* dlStr = instance->GetStringFromBits(topCnsmrs[i]->inBits);
					if (dlStr)
					{
						//Download column
						lvI.iSubItem = 1;
						lvI.pszText = dlStr;
						SendMessage(instance->popup, LVM_SETITEM, 0, (LPARAM)&lvI);
						free(dlStr);
					}

					WCHAR* ulStr = instance->GetStringFromBits(topCnsmrs[i]->outBits);
					if (ulStr)
					{
						//Upload column
						lvI.iSubItem = 2;
						lvI.pszText = ulStr;
						SendMessage(instance->popup, LVM_SETITEM, 0, (LPARAM)&lvI);
						free(ulStr);
					}
				}
			}

			//Use our comparison func to sort in descending order
			ListView_SortItems(instance->popup, instance->PopupCompare, 0);

			for (int i = 0; i < topCnsmrs.size(); i++)
			{
				delete topCnsmrs[i];
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(ONE_SECOND));
	}

	free(interfaces);
}

WCHAR* UIManager::GetStringFromBits(double inBits)
{
	double bits = inBits;
	WCHAR* retBuf = (WCHAR*)malloc(sizeof(WCHAR) * 100);

	double bMbps = bits / DIV_FACTOR;
	bool kbps = false;

	//If <1 Mbps, display as Kbps
	if (bMbps < 1.0)
	{
		bits /= KILOBYTE;
		kbps = true;
	}
	else
	{
		bits = bMbps;
	}

	if (retBuf)
	{
		swprintf_s(retBuf, 99, L"%.1lf %s", bits, kbps ? L"kbps" : L"Mbps");
	}

	return retBuf;
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
	hInst = hInstance;

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
	if (sel == MOVE_TO)
	{
		//Get all monitors
		//Populate into dropdown
		//Whichever is selected, move there

		POINT point;
		GetCursorPos(&point);

		HMENU menu = CreatePopupMenu();

		MONITORINFOEX monitorInfo;
		monitorInfo.cbSize = sizeof(MONITORINFOEX);

		HMONITOR currentMonitor = MonitorFromWindow(roothWnd, MONITOR_DEFAULTTONEAREST);

		for (int i = 0; i < monitorFinder->mList.size(); i++)
		{
			HMONITOR monitor = monitorFinder->mList[i];

			GetMonitorInfo(monitor, &monitorInfo);
			AppendMenu(menu, currentMonitor == monitor ? MF_STRING | MF_GRAYED : MF_STRING,
				i + 1, monitorInfo.szDevice);
		}

		SetForegroundWindow(instance->roothWnd);
		instance->OnSelectMonitorItem(TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, instance->roothWnd, NULL));
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

void UIManager::OnSelectMonitorItem(int sel)
{
	if (sel == 0)
	{
		return;
	}

	int vecPos = sel - 1;

	if (vecPos >= monitorFinder->mList.size())
	{
		return;
	}

	HMONITOR newMonitor = monitorFinder->mList[vecPos];
	MONITORINFOEX newMonitorInfo;
	newMonitorInfo.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(newMonitor, &newMonitorInfo);

	int mWidth = newMonitorInfo.rcWork.right - newMonitorInfo.rcWork.left;
	int mHeight = newMonitorInfo.rcWork.bottom - newMonitorInfo.rcWork.top;

	//Get current window position
	//Get normalised value between 0 and 1
	//Work out that same pos for new monitor's w/h

	HMONITOR currentMonitor = MonitorFromWindow(roothWnd, MONITOR_DEFAULTTONEAREST);

	MONITORINFOEX currentMonitorInfo;
	currentMonitorInfo.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(currentMonitor, &currentMonitorInfo);

	int cmWidth = currentMonitorInfo.rcWork.right - currentMonitorInfo.rcWork.left;
	int cmHeight = currentMonitorInfo.rcWork.bottom - currentMonitorInfo.rcWork.top;

	RECT rootPos;
	GetWindowRect(roothWnd, &rootPos);

	//Work out proportial value and map to new monitor

	float nrmWindowX = (rootPos.left * 1.0 - currentMonitorInfo.rcWork.left * 1.0) / cmWidth * 1.0;
	float nrmWindowY = (rootPos.top * 1.0 - currentMonitorInfo.rcWork.top * 1.0) / cmHeight * 1.0;

	int finalX = newMonitorInfo.rcWork.left + (mWidth * nrmWindowX);
	int finalY = newMonitorInfo.rcWork.top + (mHeight * nrmWindowY);

	SetWindowPos(roothWnd, HWND_TOPMOST, finalX, finalY, 0, 0, SWP_NOSIZE);
	WriteWindowPos();
}

float UIManager::DivLower(float a, float b)
{
	if (a <= b)
	{
		return a / b;
	}

	return b / a;
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
		case WM_LBUTTONUP:
		{
			WndProc(instance->roothWnd, message, wParam, lParam);
			break;
		}
		case WM_MOUSEMOVE:
		{
			if (hasChildMouseEvent == FALSE)
			{
				TRACKMOUSEEVENT tme{ 0 };
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_HOVER | TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				hasChildMouseEvent = TrackMouseEvent(&tme);
			}
			break;
		}
		case WM_MOUSEHOVER:
		{
			if (GetCapture() == instance->roothWnd)  //Check if this window has mouse input
			{
				break;
			}

			POINT p;
			p.x = GET_X_LPARAM(lParam);
			p.y = GET_Y_LPARAM(lParam);
			ClientToScreen(hWnd, &p);

			if (instance->netManager->HasElevatedPrivileges())
			{
				instance->ShowTopConsumersToolTip(p);
			}
			else
			{
				instance->ShowNoPrivilegesTooptip(p);
			}

			break;
		}
		case WM_MOUSELEAVE:
		{
			hasChildMouseEvent = FALSE;
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);				//CHILD WINDOW BACKGROUND COLOUR

			HBRUSH brush = (HBRUSH)::GetStockObject(DC_BRUSH);
			SetDCBrushColor(hdc, *instance->configManager->childColour);

			FillRect(hdc, &ps.rcPaint, brush);
			SetBkMode(hdc, TRANSPARENT);

			LOGFONT* modFont = (LOGFONT*)malloc(sizeof(LOGFONT));
			if (modFont && instance->fontScaleInfo)
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
			if (modFont)
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

LRESULT UIManager::PopupProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (message)
	{
		case WM_COMMAND:
		{
			break;
		}
		case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT tme{ 0 };
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_HOVER | TME_LEAVE;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			TrackMouseEvent(&tme);
			break;
		}
		case WM_MOUSELEAVE:
		{
			if (uIdSubclass == 1) //For the elevated priv popup
			{
				DestroyWindow(hWnd);
				break;
			}

			for (int i = 0; i < MAX_TOP_CONSUMERS; i++) //Delete all old items
			{
				LVITEM lvI = { 0 };
				lvI.iItem = i;
				lvI.iSubItem = 0;

				BOOL existingItem = ListView_GetItem(instance->popup, &lvI);

				if (existingItem == TRUE) //Delete ones beyond the size of the process list
				{
					ListView_DeleteItem(instance->popup, lvI.iItem);
				}
			}

			netManager->pidMap.clear();

			//Destroy this popup when we leave it
			DestroyWindow(instance->popup);
			instance->popup = NULL;
			break;
		}
		case WM_LBUTTONDOWN:	//Allow user to quickly elevate when in standard mode
		{
			if (uIdSubclass != 1)
			{
				break;
			}
			instance->TryElevate();
		}
	}
	return DefSubclassProc(hWnd, message, wParam, lParam);
}

LRESULT UIManager::ThemeListProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (message)
	{
		case WM_COMMAND:
		{
			break;
		}
		case WM_NOTIFY:
		{
		case NM_DBLCLK: 
		{
			LPNMITEMACTIVATE selData = (LPNMITEMACTIVATE)lParam;
			int sel = ListView_GetNextItem(hWnd, -1, LVNI_FOCUSED);
			if(sel == -1)
			{
				break;
			}

			LVITEM lvI = { 0 };
			lvI.mask = LVIF_TEXT | LVIF_PARAM;
			lvI.iItem = sel;

			if (ListView_GetItem(hWnd, &lvI) != -1)
			{
				Theme* theme = (Theme*)lvI.lParam;
				if (theme)
				{
					configManager->ApplyTheme(theme);
					instance->UpdateOpacity(instance->roothWnd);
					instance->ForceRepaint();
					instance->UpdateBitmapColours();
					instance->UpdateFontScaleForDPI();
				}
			}
			break;
		}
		break;
		}
	}
	return DefSubclassProc(hWnd, message, wParam, lParam);
}

void UIManager::ResetCursorDragOffset()
{
	xDragOffset = -1;
	yDragOffset = -1;
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

			instance->monitorFinder->BuildMonitorList();
			if (instance->monitorFinder->mList.size() > 1)
			{
				AppendMenu(menu, MF_STRING, MOVE_TO, L"Move to...");
			}
			AppendMenu(menu, MF_STRING, ABOUT, L"About");
			AppendMenu(menu, MF_STRING, SETTINGS, L"Settings");
			AppendMenu(menu, MF_STRING, EXIT, L"Exit");

			SetForegroundWindow(instance->roothWnd); //TrackPopupMenu requires the parent window to be in foreground, otherwise the popupmenu won't be destroyed when clicking off it
			instance->OnSelectItem(TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, instance->roothWnd, NULL));
			break;
		}
		case WM_MOUSEHOVER:
		{
			//Pass the hover message to the childproc which handles it
			ChildProc(hWnd, message, wParam, lParam);
			break;
		}
		case WM_MOUSELEAVE:
		{
			hasRootMouseEvent = FALSE;
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
			instance->ResetCursorDragOffset();
			break;

		case WM_MOUSEMOVE:
		{
			if (hasRootMouseEvent == FALSE)
			{
				TRACKMOUSEEVENT tme{ 0 };
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_HOVER | TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				hasRootMouseEvent = TrackMouseEvent(&tme);
			}

			if (GetCapture() == hWnd)  //Check if this window has mouse input
			{
				RECT windowRect;
				POINT mousePos;
				GetWindowRect(hWnd, &windowRect);

				//Get the current mouse coordinates
				mousePos.x = GET_X_LPARAM(lParam);
				mousePos.y = GET_Y_LPARAM(lParam);

				int windowHeight = windowRect.bottom - windowRect.top;
				int windowWidth = windowRect.right - windowRect.left;

				//Make the cursor stay in the same place relative to the window

				if (instance->xDragOffset == -1)
				{
					instance->xDragOffset = windowWidth - mousePos.x;
				}

				if (instance->yDragOffset == -1)
				{
					instance->yDragOffset = windowHeight - mousePos.y;
				}

				ClientToScreen(hWnd, &mousePos);

				//Set the window's new screen position
				// 
				//We could use this line and keep the cursor's Y position the same as we drag the window, like with the X, but if we keep the top of the window at the cursor, then it provides
				//A "free" way of preventing the window from being dragged above the top of the screen, as the cursor blocks it
				// 
				//MoveWindow(hWnd, mousePos.x - (windowWidth - instance->xDragOffset), mousePos.y - (windowHeight - instance->yDragOffset), windowWidth, windowHeight, TRUE);

				MoveWindow(hWnd, mousePos.x - (windowWidth - instance->xDragOffset), mousePos.y, windowWidth, windowHeight, TRUE);
			}
			break;
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
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);			//OUTER/PRIMARY COLOUR

			HBRUSH brush = (HBRUSH)::GetStockObject(DC_BRUSH);
			SetDCBrushColor(hdc, *instance->configManager->foregroundColour);

			FillRect(hdc, &ps.rcPaint, brush);
			EndPaint(hWnd, &ps);
			break;
		}
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

INT_PTR UIManager::PopupCompare(LPARAM val1, LPARAM val2, LPARAM lParamSort)
{
	if ((double)val1 > (double)val2)
	{
		return -1;
	}
	else if ((double)val1 == (double)val2)
	{
		return 0;
	}

	return 1;
}

void UIManager::ShowTopConsumersToolTip(POINT pos)
{
	if (popup != NULL)
	{
		return;
		/*DestroyWindow(popup);
		popup = NULL;*/
	}

	popup = CreateWindow(WC_LISTVIEW, L"", WS_VISIBLE | WS_POPUP | LVS_REPORT,
		pos.x, pos.y, POPUP_INITIAL_WIDTH, POPUP_INITIAL_HEIGHT, roothWnd, NULL, hInst, NULL);

	SetWindowSubclass(popup, PopupProc, 0, 0);

	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	int currDPI = GetDpiForWindow(popup);

	SendMessage(roothWnd, CCM_DPISCALE, (WPARAM)TRUE, NULL);

	//Init collumns
	//NAME	 DOWNLOAD	UPLOAD
	int totalWidth = 0;
	WCHAR cText[256];
	for (int i = 0; i < 3; i++)
	{
		lvc.iSubItem = i;
		lvc.pszText = cText;
		lvc.cx = MulDiv(POPUP_INITIAL_WIDTH / 3, currDPI, USER_DEFAULT_SCREEN_DPI);
		lvc.fmt = LVCFMT_FIXED_WIDTH;

		totalWidth += lvc.cx;
		LoadString(hInst,
			IDS_FIRST_COLUMN + i,
			cText,
			sizeof(cText) / sizeof(cText[0]));

		ListView_InsertColumn(popup, i, &lvc);
	}

	++totalWidth;

	HWND lvHeader = ListView_GetHeader(popup);

	DWORD currentStyle = GetWindowLong(lvHeader, GWL_STYLE);
	SetWindowLong(lvHeader, GWL_STYLE, (currentStyle & ~HDS_BUTTONS));

	SetWindowSubclass(lvHeader, PopupProc, 0, 0);

	InitForDPI(popup, POPUP_INITIAL_WIDTH, POPUP_INITIAL_HEIGHT, pos.x, pos.y, true, totalWidth);
}

void UIManager::ShowNoPrivilegesTooptip(POINT pos)
{
	HWND errPopup = CreateWindow(WC_EDIT, L"", WS_VISIBLE | WS_POPUP | ES_CENTER | ES_READONLY | WS_BORDER,
		pos.x, pos.y, NO_PRIV_POPUP_INITIAL_WIDTH, NO_PRIV_POPUP_INITIAL_HEIGHT, roothWnd, NULL, hInst, NULL);

	if (errPopup == NULL)
	{
		return;
	}

	LOGFONT* modFont = (LOGFONT*)malloc(sizeof(LOGFONT));
	if (modFont && fontScaleInfo)
	{
		memcpy(modFont, configManager->currentFont, sizeof(LOGFONT));

		modFont->lfWidth = fontScaleInfo->width;
		modFont->lfHeight = fontScaleInfo->height;
	}
	else
	{
		modFont = configManager->currentFont;
	}

	HFONT txtFont = CreateFontIndirect(modFont);

	SendMessage(errPopup, WM_SETFONT, (WPARAM)txtFont, NULL);

	free(modFont);
	SetWindowText(errPopup, L"Process monitoring requires elevation (Left-click to attempt)"); //We should move things like this to the string table
	SetWindowSubclass(errPopup, PopupProc, 1, 0);

	//For some reason MOUSEMOVE messages aren't being recieved by a static control,
	//so we use an edit and disable all the edit-y bits here
	HideCaret(errPopup);
	SetClassLongPtr(errPopup, GCLP_HCURSOR, (LONG_PTR)LoadCursor(nullptr, IDC_ARROW));

	InitForDPI(errPopup, NO_PRIV_POPUP_INITIAL_WIDTH, NO_PRIV_POPUP_INITIAL_HEIGHT, pos.x, pos.y, true);
}

void UIManager::UpdatePosIfRequired()
{
	if (!IsOffScreen())
	{
		return;
	}

	//If off screen, place 3/4 along the top right of the users screen
	int xPos = GetSystemMetrics(SM_CXSCREEN); //SM_CXSCREEN == primary monitor
	xPos -= (xPos / 4);

	SetWindowPos(roothWnd, HWND_TOPMOST, xPos, 0, 0, 0, SWP_NOSIZE);
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

void UIManager::InitForDPI(HWND hWnd, int initialWidth, int initialHeight, int initialX, int initialY, bool dontScalePos, int minWidth)
{
	int currDPI = GetDpiForWindow(hWnd);

	int scaledX = dontScalePos ? initialX : MulDiv(initialX, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledY = dontScalePos ? initialY : MulDiv(initialY, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledWidth = MulDiv(initialWidth, currDPI, USER_DEFAULT_SCREEN_DPI);
	int scaledHeight = MulDiv(initialHeight, currDPI, USER_DEFAULT_SCREEN_DPI);

	//Added for the popup as if width is < total collum width, unwanted scrollbars are shown
	if (minWidth != -1 && scaledWidth < minWidth)
	{
		scaledWidth = minWidth;
	}

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

	if (fontScaleInfo)
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

	if (bmScaleInfo)
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

void UIManager::TryElevate()
{
	WCHAR path[MAX_PATH];

	if (!GetModuleFileName(NULL, path, MAX_PATH))
	{
		return;
	}

	SHELLEXECUTEINFO sEI = { sizeof(sEI) };
	sEI.lpVerb = L"runas";
	sEI.lpFile = path;
	sEI.hwnd = NULL;
	sEI.nShow = SW_NORMAL;

	if (!ShellExecuteEx(&sEI))
	{
		DWORD exErr = GetLastError();
		if (exErr == ERROR_CANCELLED)
		{
			//User denied the UAC prompt (or something else went wrong)
		}

		return;
	}

	SendMessage(roothWnd, WM_CLOSE, NULL, NULL); //Shut down this instance
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
				SendMessage(vTxt, WM_SETTEXT, 0, (LPARAM)VERSION_NUMBER);
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
		{
			instance->settingsWnd = hDlg;
#ifdef USE_ACTIVATION
			HWND activateBtn = GetDlgItem(hDlg, IDC_ACTIVATION_SETTINGS);
			ShowWindow(activateBtn, FALSE);
#endif

			HWND chkBtn = GetDlgItem(hDlg, IDC_ADAPTER_AUTO_CHECK);

			if (!strcmp((char*)instance->configManager->uniqueAddr, "AUTO"))
			{
				Button_SetCheck(chkBtn, TRUE);
			}
			else
			{
				Button_SetCheck(chkBtn, FALSE);
			}

			SendMessage(hDlg, WM_SETCBSEL, (WPARAM)TRUE, NULL);

			return (INT_PTR)TRUE;

		}
		case WM_SETCBSEL:
		{
			//Init the adapter selection dropdown
			BOOL autoMode = IsDlgButtonChecked(hDlg, IDC_ADAPTER_AUTO_CHECK);
			HWND dropDown = GetDlgItem(hDlg, IDC_ADAPTER_DD);

			BOOL addToDD = (BOOL)wParam;

			if (dropDown != NULL)
			{
				ComboBox_Enable(dropDown, !autoMode);

				instance->foundAdapters = netManager->GetAllAdapters();

				int sel = -1;
				for (int i = 0; i < instance->foundAdapters.size(); i++)
				{
					if (addToDD)
					{
						SendMessage(dropDown, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)instance->foundAdapters[i].Description);
					}

					//If in auto mode, set to the currently bound adapater...
					if (autoMode && !strcmp((char*)&instance->netManager->currentPhysicalAddress, (char*)&instance->foundAdapters[i].PermanentPhysicalAddress))
					{
						sel = i;
					}
					else if (!strcmp((char*)instance->configManager->uniqueAddr, (char*)&instance->foundAdapters[i].PermanentPhysicalAddress))
					{
						sel = i;
					}
				}

				if (sel == -1 && !autoMode) //Adapter no longer exists, revert to auto
				{
					char buf[IF_MAX_PHYS_ADDRESS_LENGTH];
					strcpy(buf, "AUTO");
					instance->configManager->UpdateSelectedAdapter(buf);

					SendMessage(hDlg, message, wParam, lParam);
					break;
				}

				if (sel == -1)
				{
					sel = 0;
				}

				SendMessage(dropDown, CB_SETCURSEL, (WPARAM)sel, (LPARAM)0);
			}
		}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCLOSE || LOWORD(wParam) == IDCANCEL)
			{
				instance->foundAdapters.clear();
				EndDialog(hDlg, LOWORD(wParam));
				instance->settingsWnd = NULL;
				return (INT_PTR)TRUE;
			}
			else if (LOWORD(wParam) == IDC_ACTIVATION_SETTINGS)
			{
				DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_ACTIVATE), hDlg, ActivationProc);
			}
			else if (LOWORD(wParam) == IDC_THEMES)
			{
				DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_THEMES), hDlg, ThemesProc);
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
			else if (LOWORD(wParam) == IDC_RESET_CONFIG)
			{
				instance->configManager->ResetConfig();
				instance->UpdateOpacity(instance->roothWnd);
				instance->ForceRepaint();
				instance->UpdateBitmapColours();
				instance->WriteWindowPos(); //We reset the whole cfg so re-write this

				HWND chkBtn = GetDlgItem(hDlg, IDC_ADAPTER_AUTO_CHECK);
				HWND dropDown = GetDlgItem(hDlg, IDC_ADAPTER_DD);

				if (!strcmp((char*)instance->configManager->uniqueAddr, "AUTO"))
				{
					Button_SetCheck(chkBtn, TRUE);
					ComboBox_Enable(dropDown, FALSE);
				}
				else
				{
					Button_SetCheck(chkBtn, FALSE);
					ComboBox_Enable(dropDown, TRUE);
				}
			}
			else if (LOWORD(wParam) == IDC_OPACITY)
			{
				DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_OPACITY), hDlg, OpacityProc);
			}
			else if (LOWORD(wParam) == IDC_TEXT)
			{
				DialogBox(instance->hInst, MAKEINTRESOURCE(IDD_TEXT), hDlg, TextProc);
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_ADAPTER_AUTO_CHECK)
			{
				BOOL enabled = IsDlgButtonChecked(hDlg, IDC_ADAPTER_AUTO_CHECK);
				HWND dropDown = GetDlgItem(hDlg, IDC_ADAPTER_DD);

				if (dropDown != NULL)
				{
					ComboBox_Enable(dropDown, !enabled);

					if (enabled)
					{
						char buf[IF_MAX_PHYS_ADDRESS_LENGTH];
						strcpy(buf, "AUTO");
						instance->configManager->UpdateSelectedAdapter(buf);
						if (instance->settingsWnd != NULL)
						{
							//This is really lazy, but basically we must wait for NetworkManager to update it's currentPhysicalAddress variable
							//so the WM_SETCBSEL message is handled correctly and shows the correct selection
							//The NetworkManager updates this every second, causing this race condition
							//We could update currentPhysicalAddress from here but then that would break the newAdapater check in NetworkManager

							std::this_thread::sleep_for(std::chrono::microseconds(ONE_SECOND));
							SendMessage(instance->settingsWnd, WM_SETCBSEL, (WPARAM)FALSE, NULL);
						}
					}
					else
					{
						instance->UpdateSelectedAdapter(dropDown);
					}
				}
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				instance->UpdateSelectedAdapter(HWND(lParam));

			}
			break;
	}
	return (INT_PTR)FALSE;
}

void UIManager::UpdateSelectedAdapter(HWND dropDown)
{
	int selIndex = SendMessage(dropDown, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
	WCHAR selItemName[IF_MAX_STRING_SIZE + 1];
	selItemName[IF_MAX_STRING_SIZE] = 0;
	SendMessage(dropDown, CB_GETLBTEXT, (WPARAM)selIndex, (LPARAM)selItemName);

	//Match back up with address
	for (int i = 0; i < instance->foundAdapters.size(); i++)
	{
		if (!wcscmp(selItemName, instance->foundAdapters[i].Description))
		{
			instance->configManager->UpdateSelectedAdapter((char*)instance->foundAdapters[i].PermanentPhysicalAddress);
			break;
		}
	}
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
	ColorRGBToHLS(colorStruct.rgbResult, &h, &l, &s);
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
						instance->UpdateFontScaleForDPI();
					}
				}

				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK UIManager::ActivationProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	//Basic license key alogorithm:
	//Start with users email address
	//Get ascii value for each character
	//Shift along, do some other operations

	//Input key
	//Read in
	//Need to either do offline activation or implement server-based activation

	//Once key is verified as valid, need a way to check if the program is activated

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
			if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK UIManager::ThemesProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	//Populate with pre-made themes, apply on selection...

	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
		case WM_INITDIALOG:
		{
			HWND listCtrl = GetDlgItem(hDlg, IDC_THEMES_LIST);
			if (listCtrl != NULL)
			{
				SetWindowSubclass(listCtrl, ThemeListProc, 0, 0);

				std::vector<Theme*>allThemes;
				//Get available themes
				Theme* slateGrey = themeManager->GetTheme(AVAILABLE_THEME::SLATE_GREY);
				Theme* sunny = themeManager->GetTheme(AVAILABLE_THEME::SUNNY);

				allThemes.push_back(slateGrey);
				allThemes.push_back(sunny);

				for (int i = 0; i < allThemes.size(); i++)
				{
					LVITEM lvI = { 0 };
					lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
					lvI.iItem = i;
					lvI.iImage = i;
					lvI.lParam = (LPARAM)allThemes[i];
					lvI.pszText = allThemes[i]->name;

					// Insert items into the list.
					if (ListView_InsertItem(listCtrl, &lvI) == -1)
					{
						delete allThemes[i];
					}
				}
			}

			return (INT_PTR)TRUE;
		}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				HWND listCtrl = GetDlgItem(hDlg, IDC_THEMES_LIST);
				if (listCtrl != NULL)
				{
					LVITEM lvI = { 0 };
					lvI.mask = LVIF_PARAM;

					//Clean up items/theme instances

					while (ListView_GetItem(listCtrl, &lvI))
					{
						if (Theme* theme = (Theme*)lvI.lParam)
						{
							delete theme;
						}
						ListView_DeleteItem(listCtrl, lvI.iItem);
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

void UIManager::ForceRepaintOnRect(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	InvalidateRect(hWnd, &rc, FALSE);
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
