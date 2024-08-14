#pragma once
#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "iostream"
#include "thread"

#include "chrono"
#include "shellapi.h"
#include <windowsx.h>
#include <tuple>

#define KILOBYTE 1024.0

class NetworkManager
{
public:
	std::tuple<double, double> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces);
	NetworkManager();
	~NetworkManager();
private:
	double lastDlCount = -1.0;
	double lastUlCount = -1.0;

	UCHAR currentPhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];
	//int cacheIndex = -1;
};

