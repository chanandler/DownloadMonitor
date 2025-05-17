#pragma once
#include "framework.h"
#include "windows.h"
#include "shellapi.h"
#include "resource.h"

#define MAX_LOADSTRING 100

#define BTN_STD_HEIGHT 25
#define BTN_STD_WIDTH 100
#define LOWER_BTN_Y_POS 390

enum UIState
{
	FAILED = -1,
	WELCOME,
	OPTIONS,
	INSTALLING,
	COMPLETE,
};

class Installer
{

public:
	Installer(HINSTANCE hInstance, HINSTANCE hPrevInstance);
	bool Init(LPWSTR lpCmdLine, int nCmdShow);
	int MainLoop();


private:
	//Common controls
	HWND hWnd;
	HWND nextBtn;
	HWND backBtn;
	HWND cancelBtn;
	HWND closeBtn;
	HWND title;
	HWND desc;
	HWND installerVersion;
	HWND sideImage;
	HWND progressBar;

	HFONT titleFont;
	HFONT descFont;

	static Installer* instance;

	HWND chckbox;

	int installPct = 0;

	HINSTANCE hInst;                                // current instance
	HINSTANCE hPrevInst;                                // current instance
	WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
	WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
	WCHAR appVer[MAX_LOADSTRING];            
	WCHAR installerVer[MAX_LOADSTRING];   

	UIState currentState;
	void BuildUI(UIState state);
	void CloseServiceIfExists();
	bool InstallService(LPCWSTR path);
	HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
	ATOM MyRegisterClass(HINSTANCE hInstance);
	void OnInstallSuccess();
	void LaunchDownloadMonitor();
	bool LaunchUnelevated(LPCWSTR appPath, LPCWSTR args = nullptr);
	HFONT CreateNewFont(int width, int height, LPWSTR name);
	void BeginInstall();
	static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	bool ExtractResourceToFile(LPCWSTR resName, LPCWSTR outPath);
	static INT_PTR About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	wchar_t username[512];
	void SetCurrStatus(const WCHAR* status, int pct);

	bool defferedSvcInstall = false;
};

