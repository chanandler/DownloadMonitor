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

	void SetBmToColour(BITMAP bm, HBITMAP bmInst, HDC hdc, COLORREF col, std::vector<int> &cacheArr);
	static void UpdateInfo();
	ATOM RegisterWindowClass(HINSTANCE hInstance);
	ATOM RegisterChildWindowClass(HINSTANCE hInstance);
	HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
	void UpdateOpacity(HWND hWnd);
	void OnSelectItem(int sel);
	static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
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

	char* ansiArgs = nullptr;
};

