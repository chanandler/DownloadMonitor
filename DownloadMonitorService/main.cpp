
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

#define KILOBYTE 1024.0
#define DIV_FACTOR (KILOBYTE * KILOBYTE)
#define MAX_TOP_CONSUMERS 6

struct PidData
{
public:
	double inBits;
	double outBits;
};

SERVICE_STATUS g_ServiceStatus = {};
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE g_ServiceStopEvent = nullptr;
std::map<DWORD, PidData> pidMap;

#define SERVICE_NAME L"DownloadMonitorService"

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

			SetEvent(g_ServiceStopEvent);
			//ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

			return;

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

//PURPOSE: Get all active processes and return an ordered vector containing their current DL/UL speeds
std::vector<ProcessData*> GetTopConsumingProcesses()
{
	std::vector<ProcessData*> allCnsmrs = std::vector<ProcessData*>();

	std::map<int, bool> ignoredIndexes = std::map<int, bool>();

	PMIB_TCPTABLE2 tcpTbl = GetAllocatedTcpTable();

	if (tcpTbl)
	{
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
			//Eiter get bandwidth from that or track recv/sent as with main network tracking
			//Return top consumers bandwidth + process name

			MIB_TCPROW2 row = tcpTbl->table[i];

			TCP_ESTATS_DATA_ROD_v0 pData = { 0 };

			INT status = EnableNetworkTracing(&row);

			if (status == NO_ERROR)
			{
				status = GetProcessNetworkData(&row, &pData);
				if (status != NO_ERROR)
				{
					return allCnsmrs;
				}
			}
			else
			{
				return allCnsmrs;
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
							ignoredIndexes[j] = true;
						}
					}
				}
			}

			double finalIn = 0.0;
			double finalOut = 0.0;

			bool foundInPrev = false;

			if (pidMap.find(row.dwOwningPid) != pidMap.end())
			{
				double prevIn = pidMap[row.dwOwningPid].inBits;
				double prevOut = pidMap[row.dwOwningPid].outBits;

				if (in >= prevIn && out >= prevOut)
				{
					finalIn = in - prevIn;
					finalOut = out - prevOut;

					foundInPrev = true;
				}
				else
				{
					pidMap.erase(pidMap.find(row.dwOwningPid));
				}
			}

			PidData newData;
			newData.inBits = in;
			newData.outBits = out;

			pidMap[row.dwOwningPid] = newData;

			if (foundInPrev && (finalIn != 0.0 || finalOut != 0.0))
			{
				HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, row.dwOwningPid);
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
					ProcessData* data = new ProcessData(row.dwOwningPid, nameBuf, finalIn, finalOut);
					if (data)
					{
						bool added = false;
						for (int j = 0; j < allCnsmrs.size(); j++)
						{
							if (((finalIn + finalOut) < (allCnsmrs[j]->inBits + allCnsmrs[j]->outBits)) && j > 0)
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
		}

		free(tcpTbl);
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

	return ret;
}

void RunService()
{
	//Create a pipe

	

	//Wait for connection
	while (true)
	{
		HANDLE pipe = CreateNamedPipe(
			L"\\\\.\\pipe\\dm_pipe",
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
			continue;
		}

		BOOL result = ConnectNamedPipe(pipe, NULL);

		if (!result)
		{
			CloseHandle(pipe);
			continue;
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


	while (true) 
	{
		GetTopConsumingProcesses();
		RunService();

		if (WaitForSingleObject(g_ServiceStopEvent, 1000)) 
		{
			break;
		}
	}

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}


int wmain(int argc, wchar_t* argv[])
{
	GetTopConsumingProcesses(); //hack
	RunService();
	return 0;
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, ServiceMain },
		{ nullptr, nullptr }
	};

	StartServiceCtrlDispatcher(ServiceTable);
	return 0;
}
