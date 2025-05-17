#pragma once
#include "framework.h"
#include "windows.h"
#include "shellapi.h"
#include "windowsx.h"
#include "resource.h"

#define MAX_LOADSTRING 100

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

	HFONT titleFont;
	HFONT descFont;

	static Installer* instance;

	HWND chckbox;

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
	HFONT CreateNewFont(int width, int height, LPWSTR name);
	void BeginInstall();
	static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	bool ExtractResourceToFile(LPCWSTR resName, LPCWSTR outPath);
	static INT_PTR About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	wchar_t username[512];

	bool defferedSvcInstall = false;
};

