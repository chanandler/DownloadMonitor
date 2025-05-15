#pragma once
#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "iostream"
#include "thread"
#include <mutex>

#include "chrono"
#include "shellapi.h"
#include <windowsx.h>
#include <tuple>

#include "vector"
#include <map>

#define KILOBYTE 1024.0
#define DIV_FACTOR (KILOBYTE * KILOBYTE)
#define MAX_TOP_CONSUMERS 6

enum PipeResult 
{
	OK,
	CONNECTION_FAILED
};

struct PidData
{
public:
	double inBits;
	double outBits;
};

struct ProcessData //Must mirror class in downloadapp service
{
public:
	DWORD pid;
	WCHAR name[256];
	double inBits;
	double outBits;

	ProcessData()
	{
		inBits = -1;
		outBits = -1;
		pid = -1;
		memset(&name[0], 0, 255);
	};

	ProcessData(DWORD Pid, WCHAR* Name, double InBits, double OutBits)
	{
		pid = Pid;
		wcscpy_s(&name[0], 256, Name);
		inBits = InBits;
		outBits = OutBits;
	}

	bool IsBlank() 
	{
		return inBits + outBits <= 0.0;
	}
};

class NetworkManager
{
public:
	std::mutex countMutex;
	std::tuple<double, double> GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces, UCHAR* override);
	void ResetPrev(bool lock = true);
	void SetAutoAdaptor();
	std::vector<MIB_IF_ROW2> GetAllAdapters();
	INT GetProcessNetworkData(PMIB_TCPROW2 row, TCP_ESTATS_DATA_ROD_v0* data);
	INT EnableNetworkTracing(PMIB_TCPROW2 row);
	bool HasElevatedPrivileges();
	std::tuple<PipeResult, ProcessData*> GetTopConsumingProcesses();
	bool CanCommunicateWithPipe();
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

