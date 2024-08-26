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

#define MAX_LOADSTRING 100
#define WM_TRAYMESSAGE (WM_USER + 1)
#define DIV_FACTOR (KILOBYTE * KILOBYTE)
#define ONE_SECOND 1000000

#define EXIT 1
#define ABOUT 2
#define SETTINGS 3

#define MIN_OPACITY 15
#define MAX_OPACITY 255

#define ARROW_X_OFFSET 4
#define ARROW_Y_OFFSET 4
#define ARROW_SIZE_DIV 5
#define MIN_ARROW_SCALE_DIV 2


//Base values before any DPI scaling
#define ROOT_INITIAL_WIDTH 220
#define ROOT_INITIAL_HEIGHT 28

#define DL_INITIAL_X 10
#define UL_INITIAL_X 110

#define CHILD_INITIAL_Y 4
#define CHILD_INITIAL_WIDTH 100
#define CHILD_INITIAL_HEIGHT 20

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

class UIManager
{
public:
	static UIManager* instance;
	static class NetworkManager* netManager;
	static class ConfigManager* configManager;

	UIManager(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);

	~UIManager();
private:
	HINSTANCE hInst;                                // current instance
	WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
	WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
	WCHAR szChildStaticWindowClass[MAX_LOADSTRING];

	void UpdateBitmapColours();
	COLORREF COLORREFToRGB(COLORREF Col);

	bool running = false;

	HWND roothWnd;

	WCHAR dlBuf[200];
	WCHAR ulBuf[200];

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

	void SetBmToColour(BITMAP bm, HBITMAP bmInst, HDC hdc, COLORREF col, std::vector<int> &cacheArr);
	static void UpdateInfo();
	ATOM RegisterWindowClass(HINSTANCE hInstance);
	ATOM RegisterChildWindowClass(HINSTANCE hInstance);
	HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
	void UpdateOpacity(HWND hWnd);
	void OnSelectItem(int sel);
	static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void UpdateForDPI(HWND hWnd, RECT* newRct);
	void InitForDPI(HWND hWnd, int initialWidth, int initialHeight, int initialX, int initialY, bool dontScalePos = false);
	void UpdateFontScaleForDPI();
	void UpdateBmScaleForDPI();
	void WriteWindowPos();
	static LRESULT ChildProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	COLORREF ShowColourDialog(HWND owner, COLORREF* initCol, DWORD flags);
	static INT_PTR OpacityProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR TextProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR FontWarningProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void ForceRepaint();
	static UINT_PTR ColourPickerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

