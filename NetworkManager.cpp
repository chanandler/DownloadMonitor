#include "NetworkManager.h"

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

	if (GetIfTable2(interfaces) == NO_ERROR)
	{
		MIB_IF_ROW2* row;

		if (cacheIndex == -1)
		{
			for (int i = 0; i < interfaces[0]->NumEntries; i++)
			{
				row = &interfaces[0]->Table[i];
				if (row)
				{
					//Find wireless interface
					//if (row->Type != IF_TYPE_IEEE80211)
					if (row->InOctets <= 0 || row->MediaConnectState != MediaConnectStateConnected /*|| row->Type != MIB_IF_TYPE_ETHERNET*/)
					{
						continue;
					}

					cacheIndex = i; //Save the index so we can grab it immediately each time 
					break;
				}
			}
		}

		if (cacheIndex != -1)
		{
			row = &interfaces[0]->Table[cacheIndex];
			if (row)
			{
				//Convert to bits
				double dlBits = (row->InOctets * 8.0);
				if (lastDlCount == -1.0) //Fix massive delta when first running program and lastDl/UlCount is not set
				{
					lastDlCount = dlBits;
				}

				dl = dlBits - lastDlCount;
				lastDlCount = dlBits;

				double ulBits = (row->OutOctets * 8.0);

				if (lastUlCount == -1.0)
				{
					lastUlCount = ulBits;
				}

				ul = ulBits - lastUlCount;
				lastUlCount = ulBits;
			}
		}
	}

	//GetIfTable2 allocates memory for the tables, which we must delete
	FreeMibTable(interfaces[0]);

	return std::make_tuple(dl, ul);
}
