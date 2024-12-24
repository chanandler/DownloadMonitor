#include "ActivationManager.h"
#include "Windows.h"
#include <time.h>

ACTIVATION_STATE ActivationManager::GetActivationState()
{
	HKEY kHandle = { 0 };

	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Download Monitor\\CurrentStatus", NULL,
		KEY_READ, &kHandle);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		//Key not present, init to unregistered
		SetActivationState(ACTIVATION_STATE::UNREGISTERED);
		return ACTIVATION_STATE::UNREGISTERED;
	}

	int* aStatus = new int();
	DWORD* bSize = new DWORD();
	*bSize = sizeof(int);

	LSTATUS vRes = RegQueryValueEx(kHandle, L"AStatus", NULL, NULL, (LPBYTE)aStatus, bSize);

	ACTIVATION_STATE ret = (ACTIVATION_STATE)*aStatus;

	RegCloseKey(kHandle);
	delete aStatus, bSize;

	return ret;
}

void ActivationManager::SetActivationState(ACTIVATION_STATE newState)
{
	HKEY kHandle = { 0 };
	//Create/open key in reg

	LRESULT kRes = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Download Monitor\\CurrentStatus", NULL,
		NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
		NULL, &kHandle, NULL);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return;
	}

	//RegSetValueEx needs a ptr
	int* ptr = new int();
	*ptr = newState;

	LRESULT vRes = RegSetValueEx(kHandle, L"AStatus", NULL, REG_DWORD, (LPBYTE)ptr, sizeof(int));

	RegCloseKey(kHandle);
	delete ptr;
}

void ActivationManager::GenerateKey()
{
	//Whole key should == a multiple of UUID
	
	srand((unsigned)time(NULL));

	int key[KEY_SIZE] = { 0 };

	int total = 0;
	while (total == 0 || total % KEY_UUID != 0)
	{
		for (int i = 0; i < KEY_SIZE; i++)
		{
			total = 0;

			//Add all digits together
			for (int j = 0; j < KEY_SIZE; j++)
			{
				if (j == i) 
				{
					continue;
				}
				total += key[j];
			}

			//Add random value between 1>9
			int rVal = rand() % 10;
			if (rVal == 0)
			{
				++rVal;
			}

			key[i] = rVal;

			total += rVal;
		}
	}



	EncryptDecryptKey(&key[0], L"MyPrivateKey");

	//EncryptDecryptKey(&key[0], L"MyPrivateKey");
	int v = key[0];

	//bool isValid = ValidateKey(&key[0]);
}

bool ActivationManager::TryActivate(int* key)
{
	EncryptDecryptKey(&key[0], L"MyPrivateKey");

	if (!ValidateKey(key))
	{
		return false;
	}

	SetActivationState(ACTIVATION_STATE::ACTIVATED);
	return true;
}

bool ActivationManager::ValidateKey(int* key)
{
	int total = 0;
	for (int i = 0; i < KEY_SIZE; i++)
	{
		total += key[i];
	}

	return (total % KEY_UUID);
}

void ActivationManager::EncryptDecryptKey(int* keyArr, const wchar_t* privKey)
{
	int privKeyLen = wcslen(privKey);
	for (int i = 0; i < KEY_SIZE; i++)
	{
		//Perform XOR encyrption using the private key
		keyArr[i] ^= privKey[i % privKeyLen];
	}
}