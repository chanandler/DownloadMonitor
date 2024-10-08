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
#include <map>

#define KILOBYTE 1024.0
#define DIV_FACTOR (KILOBYTE * KILOBYTE)
#define MAX_TOP_CONSUMERS 7

class ProcessData
{
public:
	DWORD pid;
	WCHAR* name;
	double inBits;
	double outBits;

	bool skip = false;

	ProcessData(DWORD Pid, WCHAR* Name, double InBits, double OutBits)
	{
		pid = Pid;
		name = _wcsdup(Name);
		inBits = InBits;
		outBits = OutBits;
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
	std::tuple<double, double> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces, UCHAR* override);
	std::vector<MIB_IF_ROW2> GetAllAdapters();
	INT GetProcessNetworkData(PMIB_TCPROW2 row, TCP_ESTATS_DATA_ROD_v0* data);
	INT EnableNetworkTracing(PMIB_TCPROW2 row);
	bool HasElevatedPrivileges();
	std::vector<ProcessData*> GetTopConsumingProcesses();
	std::map<DWORD, PidData> pidMap;
	UCHAR currentPhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];
	NetworkManager();
	~NetworkManager();
private:
	double lastDlCount = -1.0;
	double lastUlCount = -1.0;

	PMIB_TCPTABLE2 GetAllocatedTcpTable();

	//int cacheIndex = -1;
};

