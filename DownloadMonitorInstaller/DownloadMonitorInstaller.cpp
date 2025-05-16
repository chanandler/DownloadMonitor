// DownloadMonitorInstaller.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "DownloadMonitorInstaller.h"
#include "shellapi.h"
#include "windowsx.h"
#include "Installer.h"


int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	Installer* installer = new Installer(hInstance, hPrevInstance);

	if(!installer->Init(lpCmdLine, nCmdShow))
	{
		return -1;
	}

	int exitCode = installer->MainLoop();

	delete installer;

	return exitCode;
}