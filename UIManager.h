#pragma 
#include "framework.h"
#include "Name.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "iostream"
#include "thread"

#include "commdlg.h"
#include "chrono"
#include "shellapi.h"
#include <windowsx.h>
#include <tuple>

#include "vector"
#include "Commctrl.h"
#include <map>

#define USE_ACTIVATION false //All activation related code is wrapped around this define

#define MAX_LOADSTRING 100
#define WM_TRAYMESSAGE (WM_USER + 1)
#define WM_SETCBSEL (WM_USER + 2)
#define ONE_SECOND 1000000

#define MOVE_TO 1
#define EXIT 2
#define ABOUT 3
#define SETTINGS 4

#define MIN_OPACITY 15
#define MAX_OPACITY 255

#define ARROW_X_OFFSET 4
#define ARROW_Y_OFFSET 4
#define ARROW_SIZE_DIV 5
#define MIN_ARROW_SCALE_DIV 2

#define VERSION_NUMBER L"Version 0.9"

//Base values before any DPI scaling
#define ROOT_INITIAL_WIDTH 220
#define ROOT_INITIAL_HEIGHT 28

#define DL_INITIAL_X 10
#define UL_INITIAL_X 110

#define CHILD_INITIAL_Y 4
#define CHILD_INITIAL_WIDTH 100
#define CHILD_INITIAL_HEIGHT 20

#define POPUP_BUF_SIZE 1000
#define POPUP_INITIAL_WIDTH 450
#define POPUP_INITIAL_HEIGHT 150

#define NO_PRIV_POPUP_INITIAL_WIDTH 425
#define NO_PRIV_POPUP_INITIAL_HEIGHT 25

class BitmapScaleInfo
{
public:
	int xPos;
	int yPos;
	int width;
	int height;

	BitmapScaleInfo(int Xpos, int YPos, int Width, int Height)
	{
		xPos = Xpos;
		yPos = YPos;
		width = Width;
		height = Height;
	}
};

class FontScaleInfo
{
public:
	int width;
	int height;

	FontScaleInfo(int Width, int Height)
	{
		width = Width;
		height = Height;
	}
};

class MonitorData
{
public:
	std::vector<HMONITOR> mList;


	static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
	{
		MonitorData* pThis = (MonitorData*)pData;
		pThis->mList.push_back(hMonitor);
		return TRUE;
	}

	void BuildMonitorList()
	{
		mList.clear();
		EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM(this)));
	}
};

class UIManager
{
public:
	static UIManager* instance;
	static class NetworkManager* netManager;
	static class ConfigManager* configManager;

	UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);

	~UIManager();
private:

	//For keeping the cursor in the same position when dragging the window
	LONG xDragOffset = -1;
	LONG yDragOffset = -1;

	HINSTANCE hInst;                                // current instance
	WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
	WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
	WCHAR szChildStaticWindowClass[MAX_LOADSTRING];
	WCHAR szPopupWindowClass[MAX_LOADSTRING];

	void UpdateBitmapColours();
	void GetMaxScreenRect();
	COLORREF COLORREFToRGB(COLORREF Col);

	bool running = false;

	HWND roothWnd;

	HWND settingsWnd;

	WCHAR dlBuf[200];
	WCHAR ulBuf[200];
	WCHAR puBuf[POPUP_BUF_SIZE];

	HWND dlChildWindow;
	HWND ulChildWindow;

	NOTIFYICONDATA trayIcon;

	BITMAP downloadIconBm;
	BITMAP uploadIconBm;

	HBITMAP downloadIconInst;
	HBITMAP uploadIconInst;

	HDC downloadIconHDC;
	HDC uploadIconHDC;

	std::vector<int> uploadBMIndexes;
	std::vector<int> downloadBMIndexes;

	BitmapScaleInfo* bmScaleInfo;
	FontScaleInfo* fontScaleInfo;

	static BOOL hasChildMouseEvent;
	static BOOL hasRootMouseEvent;

	HWND popup;

	MonitorData* monitorFinder;

	std::vector<MIB_IF_ROW2> foundAdapters;

	WCHAR* GetStringFromBits(double inBits);
	void ResetCursorDragOffset();
	void SetBmToColour(BITMAP bm, HBITMAP bmInst, HDC hdc, COLORREF col, std::vector<int> &cacheArr);
	static void UpdateInfo();
	ATOM RegisterWindowClass(HINSTANCE hInstance);
	ATOM RegisterChildWindowClass(HINSTANCE hInstance);
	HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
	void UpdateOpacity(HWND hWnd);
	void OnSelectItem(int sel);
	static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void ShowTopConsumersToolTip(POINT pos);
	void UpdatePosIfRequired();
	void UpdateForDPI(HWND hWnd, RECT* newRct);
	void InitForDPI(HWND hWnd, int initialWidth, int initialHeight, int initialX, int initialY, bool dontScalePos = false);
	bool IsOffScreen();
	void UpdateFontScaleForDPI();
	void UpdateBmScaleForDPI();
	void WriteWindowPos();
	void ShowNoPrivilegesTooptip(POINT pos);
	void UpdateSelectedAdapter(HWND dropDown);
	void TryElevate();
	void OnSelectMonitorItem(int sel);
	static LRESULT ChildProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT PopupProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	static INT_PTR AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	COLORREF ShowColourDialog(HWND owner, COLORREF* initCol, DWORD flags);
	static INT_PTR OpacityProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR ActivationProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR TextProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR FontWarningProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR PopupCompare(LPARAM val1, LPARAM val2, LPARAM lParamSort);
	void ForceRepaint();
	static UINT_PTR ColourPickerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

