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
			if(memcmp(row->PermanentPhysicalAddress, currentPhysicalAddress, IF_MAX_PHYS_ADDRESS_LENGTH))
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
