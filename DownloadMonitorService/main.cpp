
#pragma once

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <thread>
#include <iostream>
#include <vector>
#include <map>
#include "shellapi.h"
#include <windowsx.h>
#include <tuple>
#include <mutex>

#define KILOBYTE 1024.0
#define DIV_FACTOR (KILOBYTE * KILOBYTE)
#define MAX_TOP_CONSUMERS 6
#define ONE_SECOND 1000
#define PIPE_NAME L"\\\\.\\pipe\\dm_pipe"

//#define STANDALONE //Uncomment to run outside of service

struct PidData
{
public:
	double inBits;
	double outBits;

	double prvInBits;
	double prvOutBits;

	double GetInBits()
	{
		return inBits - prvInBits;
	}

	double GetOutBits()
	{
		return outBits - prvOutBits;
	}

	bool SkipThisTick()
	{
		return (prvInBits == 0 && prvOutBits == 0);
	}
};

SERVICE_STATUS g_ServiceStatus = {};
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE g_ServiceStopEvent = nullptr;
std::map<DWORD, PidData> pidMap;

#define SERVICE_NAME L"DownloadMonitorService"

std::thread processMonitorThread;
std::thread serviceThread;
std::mutex pidMutex;

struct ProcessData //Must mirror class in downloadapp client!
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

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
	// Handle the requested control code. 

	switch (dwCtrl)
	{
		case SERVICE_CONTROL_STOP:
			//ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

			// Signal the service to stop.
			g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
			SetEvent(g_ServiceStopEvent);
			//ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
			processMonitorThread.join();
			serviceThread.join();

			g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
			break;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}
}

PMIB_TCPTABLE2 GetAllocatedTcpTable()
{
	PMIB_TCPTABLE2 tcpTbl;
	ULONG ulSize = 0;

	tcpTbl = (MIB_TCPTABLE2*)malloc(sizeof(MIB_TCPTABLE2));
	if (!tcpTbl)
	{
		return nullptr;
	}

	ulSize = sizeof(MIB_TCPTABLE2);

	//Initial call to get tbl size, then allocate
	if (GetTcpTable2(tcpTbl, &ulSize, TRUE) == ERROR_INSUFFICIENT_BUFFER)
	{
		free(tcpTbl);
		tcpTbl = (MIB_TCPTABLE2*)malloc(ulSize);
		if (!tcpTbl)
		{
			return nullptr;
		}
	}

	//Get actual data
	GetTcpTable2(tcpTbl, &ulSize, TRUE);
	return tcpTbl;
}

INT GetProcessNetworkData(PMIB_TCPROW2 row, TCP_ESTATS_DATA_ROD_v0* data)
{
	TCP_ESTATS_DATA_RW_v0 settings;
	settings.EnableCollection = TcpBoolOptEnabled;
	return GetPerTcpConnectionEStats((PMIB_TCPROW)row, TcpConnectionEstatsData, (PUCHAR)&settings, 0, sizeof(TCP_ESTATS_DATA_RW_v0),
		nullptr, 0, 0, (PUCHAR)data, 0, sizeof(TCP_ESTATS_DATA_ROD_v0));
}

INT EnableNetworkTracing(PMIB_TCPROW2 row)
{
	TCP_ESTATS_DATA_RW_v0 settings;
	settings.EnableCollection = TcpBoolOptEnabled;
	return SetPerTcpConnectionEStats((PMIB_TCPROW)row, TcpConnectionEstatsData, (PUCHAR)&settings, 0, sizeof(TCP_ESTATS_DATA_RW_v0), 0);
}

void TrackPIDUsage()
{
	while (true) 
	{
		pidMutex.lock();
		std::map<int, bool> ignoredIndexes = std::map<int, bool>();

		PMIB_TCPTABLE2 tcpTbl = GetAllocatedTcpTable();
		std::map<DWORD, PidData>::iterator it;
		if (tcpTbl)
		{
			//Remove old processes that no longer exit
			for (it = pidMap.end(); it != pidMap.begin(); it--)
			{
				bool found = false;
				for (int j = 0; j < (int)tcpTbl->dwNumEntries; j++)
				{
					if (tcpTbl->table[j].dwState != MIB_TCP_STATE_ESTAB ||
						tcpTbl->table[j].dwState == MIB_TCP_STATE_CLOSED)
					{
						continue;
					}
					MIB_TCPROW2 row = tcpTbl->table[j];
					if(pidMap.find(row.dwOwningPid) != pidMap.end())
					{
						found = true;
						break;
					}
				}

				if(!found)
				{
					pidMap.erase(it);
				}
			}

			for (int i = 0; i < (int)tcpTbl->dwNumEntries; i++)
			{
				if (tcpTbl->table[i].dwState != MIB_TCP_STATE_ESTAB ||
					tcpTbl->table[i].dwState == MIB_TCP_STATE_CLOSED
					|| ignoredIndexes[i] == true)
				{
					continue;
				}

				//Get PID
				//Get TCP row
				//Pass into GetPerTcpConectioNEStats

				MIB_TCPROW2 row = tcpTbl->table[i];

				TCP_ESTATS_DATA_ROD_v0 pData = { 0 };

				INT status = EnableNetworkTracing(&row);

				if (status == NO_ERROR)
				{
					status = GetProcessNetworkData(&row, &pData);
					if (status != NO_ERROR)
					{
						continue;
					}
				}
				else
				{
					continue;
				}

				//These are octects, convert to bits
				double in = pData.DataBytesIn * 8.0;
				double out = pData.DataBytesOut * 8.0;

				//Check for any other occurances of this PID (Programs can have multiple processes under the same PID)
				for (int j = i + 1; j < (int)tcpTbl->dwNumEntries; j++)
				{
					if (tcpTbl->table[j].dwState != MIB_TCP_STATE_ESTAB ||
						tcpTbl->table[j].dwState == MIB_TCP_STATE_CLOSED)
					{
						continue;
					}
					MIB_TCPROW2 kRow = tcpTbl->table[j];

					if (kRow.dwOwningPid == row.dwOwningPid)
					{
						status = EnableNetworkTracing(&kRow);

						if (status == NO_ERROR)
						{
							TCP_ESTATS_DATA_ROD_v0 kPData = { 0 };

							status = GetProcessNetworkData(&kRow, &kPData);
							if (status == NO_ERROR)
							{
								in += (kPData.DataBytesIn * 8.0);
								out += (kPData.DataBytesOut * 8.0);
							}
						}

						ignoredIndexes[j] = true;
					}
				}

				double prvIn = 0.0;
				double prvOut = 0.0;

				//We have a prev for this PID
				if (pidMap.find(row.dwOwningPid) != pidMap.end())
				{
					if (in + out <= 0.0)
					{
						pidMap.erase(pidMap.find(row.dwOwningPid));
						continue;
					}

					prvIn = pidMap[row.dwOwningPid].inBits;
					prvOut = pidMap[row.dwOwningPid].outBits;

					if(in < prvIn)
					{
						prvIn = 0.0;
					}
					if(out < prvOut)
					{
						prvOut = 0.0f;
					}
				}

				if (in + out <= 0.0)
				{
					continue;
				}		

				PidData newData;
				newData.inBits = in;
				newData.outBits = out;
				newData.prvInBits = prvIn;
				newData.prvOutBits = prvOut;

				pidMap[row.dwOwningPid] = newData;
			}

			free(tcpTbl);
		}

		pidMutex.unlock();
#ifdef STANDALONE
		std::this_thread::sleep_for(std::chrono::milliseconds(ONE_SECOND));
#else
		if (WaitForSingleObject(g_ServiceStopEvent, ONE_SECOND) == WAIT_OBJECT_0)
		{
			break;
		}
#endif
	}
}

//PURPOSE: Get all active processes and return an ordered vector containing their current DL/UL speeds
std::vector<ProcessData*> GetTopConsumingProcesses()
{
	pidMutex.lock();
	std::vector<ProcessData*> allCnsmrs = std::vector<ProcessData*>();

	std::map<DWORD, PidData>::iterator it;

	for (it = pidMap.begin(); it != pidMap.end(); it++)
	{
		DWORD pid = it->first;
		PidData pData = it->second;

		if (pData.SkipThisTick()) 
		{
			continue;
		}
		double in = pData.GetInBits();
		double out = pData.GetOutBits();

		if (in + out <= 0.0)
		{
			continue;
		}

		HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (handle)
		{
			DWORD bufSize = 1024;
			WCHAR nameBuf[1024];
			INT queryResult = QueryFullProcessImageName(handle, 0, nameBuf, &bufSize);

			if (queryResult != TRUE) //QueryFullProcessImageName fails when being run on system
			{
				swprintf_s(nameBuf, L"System");
			}

			CloseHandle(handle);
			ProcessData* data = new ProcessData(pid, nameBuf, in, out);
			if (data)
			{
				bool added = false;
				for (int j = 0; j < allCnsmrs.size(); j++)
				{
					if (((in + out) < (allCnsmrs[j]->inBits + allCnsmrs[j]->outBits)) && j > 0)
					{
						allCnsmrs.insert(allCnsmrs.begin() + j - 1, data);
						added = true;
						break;
					}
				}
				if (!added)
				{
					allCnsmrs.push_back(data);
				}
			}
		}
	}
	
	std::vector<ProcessData*> ret = std::vector<ProcessData*>();
	int end = allCnsmrs.size() - MAX_TOP_CONSUMERS;
	if (end < 0)
	{
		end = 0;
	}

	//Create vec with correct amount of items
	for (int i = allCnsmrs.size() - 1; i >= 0; i--)
	{
		if (i >= end)
		{
			ret.push_back(allCnsmrs[i]);
		}
		else
		{
			delete allCnsmrs[i];
		}
	}

	pidMutex.unlock();
	return ret;
}

void RunService()
{
	//Create a pipe
	//Wait for connection
	while (true)
	{
		HANDLE pipe = CreateNamedPipe(
			PIPE_NAME,
			PIPE_ACCESS_OUTBOUND,
			PIPE_TYPE_BYTE,
			1,
			0,
			0,
			0,
			NULL
		);

		if (pipe == NULL || pipe == INVALID_HANDLE_VALUE)
		{
			return;
		}

		BOOL result = ConnectNamedPipe(pipe, NULL);

		if (!result)
		{
			CloseHandle(pipe);
			return;
		}

		//Get data to send
		std::vector<ProcessData*> data = GetTopConsumingProcesses();

		ProcessData* arrToSend = new ProcessData[MAX_TOP_CONSUMERS];

		for (int i = 0; i < MAX_TOP_CONSUMERS; i++)
		{
			if (i >= data.size()) 
			{
				break;
			}

			arrToSend[i] = *data[i];
		}

		DWORD bytesWritten = 0;

		result = WriteFile(
			pipe,
			arrToSend,
			sizeof(ProcessData) * MAX_TOP_CONSUMERS,
			&bytesWritten,
			NULL
		);

		CloseHandle(pipe);

		for(int i = 0; i < data.size(); i++)
		{
			delete data[i];
		}

		delete[] arrToSend;
#ifdef STANDALONE
		std::this_thread::sleep_for(std::chrono::milliseconds(ONE_SECOND));
#else
		if (WaitForSingleObject(g_ServiceStopEvent, ONE_SECOND) == WAIT_OBJECT_0)
		{
			break;
		}
#endif
	}
}

void WINAPI ServiceMain(DWORD, LPTSTR*) {
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, SvcCtrlHandler);

	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	processMonitorThread = std::thread(&TrackPIDUsage);
	processMonitorThread.detach();

	serviceThread = std::thread(&RunService);
	serviceThread.detach();

	while (true) 
	{
		if (WaitForSingleObject(g_ServiceStopEvent, 10) == WAIT_OBJECT_0)
		{
			break;
		}
	}
}


int wmain(int argc, wchar_t* argv[])
{
#ifdef STANDALONE	
	processMonitorThread = std::thread(&TrackPIDUsage);
	processMonitorThread.detach();
	while (true) 
	{
		RunService();
	}
#endif

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, ServiceMain },
		{ nullptr, nullptr }
	};

	StartServiceCtrlDispatcher(ServiceTable);
	return 0;
}
