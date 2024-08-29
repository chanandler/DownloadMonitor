#include "NetworkManager.h"
#include <map>

NetworkManager::NetworkManager()
{

}

NetworkManager::~NetworkManager()
{

}

//PURPOSE: Find currently active adaptor, cache it's position, then track the different between the previous In/Out octets vs the current
std::tuple<double, double> NetworkManager::GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces)
{
	double dl = -1.0;
	double ul = -1.0;
	int adapterIndex = -1;
	if (GetIfTable2(interfaces) == NO_ERROR)
	{
		MIB_IF_ROW2* row;

		for (int i = 0; i < interfaces[0]->NumEntries; i++)
		{
			row = &interfaces[0]->Table[i];
			if (row)
			{
				//Find any interface that has incoming data, is marked as connected and not set as Unspecified medium type
				if (row->InOctets <= 0 || row->MediaConnectState != MediaConnectStateConnected || row->PhysicalMediumType == NdisPhysicalMediumUnspecified)
				{
					continue;
				}

				adapterIndex = i;
				break;
			}
		}

		if (adapterIndex != -1)
		{
			row = &interfaces[0]->Table[adapterIndex];

			bool newAdapter = false;
			if (memcmp(row->PermanentPhysicalAddress, currentPhysicalAddress, IF_MAX_PHYS_ADDRESS_LENGTH))
			{
				newAdapter = true;
				memcpy(currentPhysicalAddress, row->PermanentPhysicalAddress, IF_MAX_PHYS_ADDRESS_LENGTH);
			}

			if (row)
			{
				//Convert to bits
				double dlBits = (row->InOctets * 8.0);
				if (newAdapter) //Fix massive delta when first running program/switching adapters
				{
					lastDlCount = dlBits;
				}

				dl = dlBits - lastDlCount;
				lastDlCount = dlBits;

				double ulBits = (row->OutOctets * 8.0);

				if (newAdapter)
				{
					lastUlCount = ulBits;
				}

				ul = ulBits - lastUlCount;
				lastUlCount = ulBits;
			}
		}
		else
		{
			//No adapters available, just return 0
			dl = 0;
			ul = 0;
		}
	}

	//GetIfTable2 allocates memory for the tables, which we must delete
	FreeMibTable(interfaces[0]);

	return std::make_tuple(dl, ul);
}

INT NetworkManager::GetProcessNetworkData(PMIB_TCPROW2 row, TCP_ESTATS_DATA_ROD_v0* data)
{
	TCP_ESTATS_DATA_RW_v0 settings;
	settings.EnableCollection = TcpBoolOptEnabled;
	return GetPerTcpConnectionEStats((PMIB_TCPROW)row, TcpConnectionEstatsData, (PUCHAR)&settings, 0, sizeof(TCP_ESTATS_DATA_RW_v0),
		nullptr, 0, 0, (PUCHAR)data, 0, sizeof(TCP_ESTATS_DATA_ROD_v0));
}

INT NetworkManager::EnableNetworkTracing(PMIB_TCPROW2 row)
{
	TCP_ESTATS_DATA_RW_v0 settings;
	settings.EnableCollection = TcpBoolOptEnabled;
	return SetPerTcpConnectionEStats((PMIB_TCPROW)row, TcpConnectionEstatsData, (PUCHAR)&settings, 0, sizeof(TCP_ESTATS_DATA_RW_v0), 0);
}

std::map<DWORD, PidData> pidMap;

std::vector<ProcessData*> NetworkManager::GetTopConsumingProcesses()
{
	std::vector<ProcessData*> ret = std::vector<ProcessData*>();

	PMIB_TCPTABLE2 tcpTbl;
	ULONG ulSize = 0;

	char localAddr[128];
	char remoteAddr[128];

	struct in_addr IpAddr;

	tcpTbl = (MIB_TCPTABLE2*)malloc(sizeof(MIB_TCPTABLE2));
	if (!tcpTbl)
	{
		return ret;
	}

	ulSize = sizeof(MIB_TCPTABLE2);

	//Initial call to get tbl size, then allocate
	if (GetTcpTable2(tcpTbl, &ulSize, TRUE) == ERROR_INSUFFICIENT_BUFFER)
	{
		free(tcpTbl);
		tcpTbl = (MIB_TCPTABLE2*)malloc(ulSize);
		if (!tcpTbl)
		{
			return ret;
		}
	}

	//Get actual data
	if (GetTcpTable2(tcpTbl, &ulSize, TRUE) == NO_ERROR)
	{
		for (int i = 0; i < (int)tcpTbl->dwNumEntries; i++)
		{
			if (tcpTbl->table[i].dwState != MIB_TCP_STATE_ESTAB ||
				tcpTbl->table[i].dwState == MIB_TCP_STATE_CLOSED)
			{
				continue;
			}

			/*TCP_ESTATS_BANDWIDTH_RW_v0 bwSettings;
			bwSettings.EnableCollectionInbound = TcpBoolOptEnabled;
			bwSettings.EnableCollectionOutbound = TcpBoolOptEnabled;
			TCP_ESTATS_BANDWIDTH_ROD_v0 bwData;
			*/
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
					return ret;
				}
			}
			else
			{
				return ret;
			}
			
			//These are octects, convert to bits
			double in = pData.DataBytesIn * 8.0;
			double out = pData.DataBytesOut * 8.0;

			double finalIn = 0.0;
			double finalOut = 0.0;

			bool foundInPrev = false;

			if (pidMap.find(row.dwOwningPid) != pidMap.end())
			{
				double prevIn = pidMap[row.dwOwningPid].inBits;
				double prevOut = pidMap[row.dwOwningPid].outBits;

				if(in >= prevIn && out >= prevOut)
				{	
					finalIn = in - prevIn;
					finalOut = out - prevOut;

					foundInPrev = true;
				}
			}

			PidData newData;
			newData.inBits = in;
			newData.outBits = out;

			pidMap[row.dwOwningPid] = newData;

			if (foundInPrev && (finalIn != 0.0 || finalOut != 0.0) )
			{
				HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, row.dwOwningPid);
				if (handle)
				{
					DWORD bufSize = 1024;
					WCHAR nameBuf[1024];
					QueryFullProcessImageName(handle, 0, nameBuf, &bufSize);
					CloseHandle(handle);

					ProcessData* data = new ProcessData(row.dwOwningPid, nameBuf, finalIn, finalOut);
					if (data)
					{
						bool added = false;
						for (int j = 0; j < ret.size(); j++)
						{
							if (in < ret[j]->inBw && j > 0)
							{
								ret.insert(ret.begin() + j - 1, data);
								added = true;
								break;
							}
						}
						if (!added)
						{
							ret.push_back(data);
						}
					}
				}
			}
		}
	}

	free(tcpTbl);

	return ret;
}