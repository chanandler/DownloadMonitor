#include "NetworkManager.h"

NetworkManager::NetworkManager()
{

}

NetworkManager::~NetworkManager()
{

}

//PURPOSE: Find currently active adaptor, cache it's position, then track the different between the previous In/Out octets vs the current
std::tuple<double, double> NetworkManager::GetAdaptorInfo(HWND hWnd, PMIB_IF_TABLE2* interfaces, UCHAR* override)
{
	countMutex.lock();
	double dl = 0.0;
	double ul = 0.0;
	int adapterIndex = -1;
	DWORD res = GetIfTable2(interfaces);
	if (res == NO_ERROR)
	{
		MIB_IF_ROW2* row;

		for (int i = 0; i < interfaces[0]->NumEntries; i++)
		{
			row = &interfaces[0]->Table[i];
			if (row)
			{
				//In auto, this is our selected adapte
				if (!strcmp((char*)override, "AUTO") && !strcmp((char*)currentPhysicalAddress, (char*)&row->PermanentPhysicalAddress))
				{
					adapterIndex = i;
					break;
				}
				else if (!strcmp((char*)override, (char*)&row->PermanentPhysicalAddress)) //We've set this as our adapter
				{
					adapterIndex = i;
					break;
				}
			}
		}

		if (adapterIndex != -1)
		{
			row = &interfaces[0]->Table[adapterIndex];

			if (row)
			{
				//Convert to bits
				double dlBits = (row->InOctets * 8.0);

				if(lastDlCount != -1)
				{
					dl = dlBits - lastDlCount;
				}

				lastDlCount = dlBits;

				double ulBits = (row->OutOctets * 8.0);

				if(lastUlCount != -1)
				{
					ul = ulBits - lastUlCount;
				}
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
	if (res == NO_ERROR)
	{
		FreeMibTable(interfaces[0]);
	}

	countMutex.unlock();

	return std::make_tuple(dl, ul);
}

void NetworkManager::ResetPrev(bool lock)
{
	if (lock)
	{
		countMutex.lock();
	}

	lastDlCount = -1;
	lastUlCount = -1;

	if (lock)
	{
		countMutex.unlock();
	}
}

//Set currentPhysicalAddress to the auto-selected adapter
void NetworkManager::SetAutoAdaptor()
{
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));

	if (!interfaces)
	{
		return;
	}

	DWORD res = GetIfTable2(interfaces);

	if (res == NO_ERROR)
	{
		MIB_IF_ROW2* row;

		for (int i = 0; i < interfaces[0]->NumEntries; i++)
		{
			row = &interfaces[0]->Table[i];
			if (row)
			{
				//Find any interface that has incoming data, is marked as connected and not set as Unspecified medium type
				if (row->InOctets <= 0 || row->MediaConnectState != MediaConnectStateConnected || row->PhysicalMediumType == NdisPhysicalMediumUnspecified
					|| row->PhysicalAddressLength == 0)
				{
					continue;
				}

				if (memcmp(row->PermanentPhysicalAddress, currentPhysicalAddress, IF_MAX_PHYS_ADDRESS_LENGTH))
				{
					//We have found a different adapter from the current one, reset prev
					ResetPrev(false);
				}

				memcpy(currentPhysicalAddress, row->PermanentPhysicalAddress, IF_MAX_PHYS_ADDRESS_LENGTH);
				break;
			}
		}

	}

	//GetIfTable2 allocates memory for the tables, which we must delete
	if (res == NO_ERROR)
	{
		FreeMibTable(interfaces[0]);
	}

	free(interfaces);
}

std::vector<MIB_IF_ROW2> NetworkManager::GetAllAdapters()
{
	PMIB_IF_TABLE2* interfaces = (PMIB_IF_TABLE2*)malloc(sizeof(PMIB_IF_TABLE2));
	std::vector<MIB_IF_ROW2> ret = std::vector<MIB_IF_ROW2>();

	if (!interfaces)
	{
		goto Exit;
	}

	if (GetIfTable2(interfaces) == NO_ERROR)
	{
		MIB_IF_ROW2* row;

		std::map<UCHAR, bool> foundAdapters = std::map<UCHAR, bool>();;

		for (int i = 0; i < interfaces[0]->NumEntries; i++)
		{
			row = &interfaces[0]->Table[i];
			if (row && !foundAdapters[*row->PermanentPhysicalAddress])
			{
				if ((row->PhysicalMediumType == NdisPhysicalMediumUnspecified || row->PhysicalMediumType == NdisPhysicalMediumBluetooth)
					|| *row->PermanentPhysicalAddress == (UCHAR)'\n' || row->PhysicalAddressLength == 0)
				{
					continue;
				}
				ret.push_back(*row);
				foundAdapters[*row->PermanentPhysicalAddress] = true;
			}
		}

		FreeMibTable(interfaces[0]);
		free(interfaces);
	}

Exit:
	return ret;
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

//Attempt setting connection EStat to see if we have the required privileges
bool NetworkManager::HasElevatedPrivileges()
{
	TCP_ESTATS_DATA_RW_v0 settings;
	settings.EnableCollection = TcpBoolOptEnabled;

	PMIB_TCPTABLE2 tcpTbl = GetAllocatedTcpTable();
	for (int i = 0; i < (int)tcpTbl->dwNumEntries; i++)
	{
		if (tcpTbl->table[i].dwState != MIB_TCP_STATE_ESTAB)
		{
			continue;
		}

		return SetPerTcpConnectionEStats((PMIB_TCPROW)&tcpTbl->table[i], TcpConnectionEstatsData, (PUCHAR)&settings, 0, sizeof(TCP_ESTATS_DATA_RW_v0), 0)
			!= ERROR_ACCESS_DENIED;
	}

	return false;
}

PMIB_TCPTABLE2 NetworkManager::GetAllocatedTcpTable()
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

std::vector<ProcessData*> NetworkManager::GetTopConsumingProcesses()
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