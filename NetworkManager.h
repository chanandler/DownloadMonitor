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

#include "vector"

#define KILOBYTE 1024.0
#define MAX_TOP_CONSUMERS 10

class ProcessData
{
public:
	DWORD pid;
	WCHAR* name;
	double inBw;
	double outBw;

	bool skip = false;

	ProcessData(DWORD Pid, WCHAR* Name, double InBw, double OutBw)
	{
		pid = Pid;
		name = _wcsdup(Name);
		inBw = InBw;
		outBw = OutBw;
	}

	~ProcessData()
	{
		if(!name)
		{
			return;
		}

		free(name);
	}
};

struct PidData
{
public:
	double inBits;
	double outBits;
};

class NetworkManager
{
public:
	std::tuple<double, double> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces);
	INT GetProcessNetworkData(PMIB_TCPROW2 row, TCP_ESTATS_DATA_ROD_v0* data);
	INT EnableNetworkTracing(PMIB_TCPROW2 row);
	std::vector<ProcessData*> GetTopConsumingProcesses();
	NetworkManager();
	~NetworkManager();
private:
	double lastDlCount = -1.0;
	double lastUlCount = -1.0;

	UCHAR currentPhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];
	//int cacheIndex = -1;
};

