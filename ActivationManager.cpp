#include "ActivationManager.h"
#include "Windows.h"
#include <time.h>

//Key/Activation overview:
//We generate a key with KEY_SIZE digits
//All digits, when added together, create a multiple of value generated from the private key
//This is our UUID that we check for after decrypting a key

//We then encrypt the key using XOR against the user's email address
//Meaning they need to use their email address to decrypt the key and retrieve the valid UUID

ACTIVATION_STATE ActivationManager::GetActivationState()
{
	HKEY kHandle = { 0 };

	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_PATH, NULL,
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

	LSTATUS vRes = RegQueryValueEx(kHandle, A_STATUS, NULL, NULL, (LPBYTE)aStatus, bSize);

	ACTIVATION_STATE ret = (ACTIVATION_STATE)*aStatus;

	RegCloseKey(kHandle);
	delete aStatus, bSize;

	return ret;
}

void ActivationManager::SetActivationState(ACTIVATION_STATE newState)
{
	HKEY kHandle = { 0 };
	//Create/open key in reg

	LRESULT kRes = RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, NULL,
		NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
		NULL, &kHandle, NULL);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return;
	}

	//RegSetValueEx needs a ptr
	int* ptr = new int();
	*ptr = newState;

	LRESULT vRes = RegSetValueEx(kHandle, A_STATUS, NULL, REG_DWORD, (LPBYTE)ptr, sizeof(int));

	RegCloseKey(kHandle);
	delete ptr;
}

int ActivationManager::GetUUIDFromStr(wchar_t* str)
{
	int ret = 0;
	int len = wcslen(str);

	for (int i = 0; i < len; i++)
	{
		int val = str[i];
		ret += val;
	}

	//Only get first two digits
	int dCount = CountDigits(ret);

	if (dCount >= 2) 
	{
		for(int i = 0; i < (dCount - 2); i++)
		{
			ret /= 10;
		}
	}

	return ret;
}

//TODO move this to a seperate key/license program
void ActivationManager::GenerateKey(wchar_t* prvKey)
{
	//Whole key should == a multiple of UUID

	srand((unsigned)time(NULL));

	int key[KEY_SIZE] = { 0 };
	int uuid = GetUUIDFromStr(prvKey);
	int total = 0;
	while (total == 0 || total % uuid != 0)
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

			//Add random value between 1>20
			int rVal = rand() % 21;
			if (rVal == 0)
			{
				++rVal;
			}

			key[i] = rVal;

			total += rVal;
		}
	}

	EncryptDecryptKey(&key[0], prvKey);
	int v = key[0];
}

bool ActivationManager::TryActivate(int* key, wchar_t* prvKey)
{
	EncryptDecryptKey(&key[0], prvKey);

	if (!ValidateKey(key, prvKey))
	{
		return false;
	}

	SetActivationState(ACTIVATION_STATE::ACTIVATED);
	return true;
}

bool ActivationManager::ValidateKey(int* key, wchar_t* prvKey)
{
	int uuid = GetUUIDFromStr(prvKey);
	if (uuid == 0) 
	{
		return false;
	}
	int total = 0;
	for (int i = 0; i < KEY_SIZE; i++)
	{
		total += key[i];
	}

	if (total == 0) 
	{
		return false;
	}

	return (total % uuid == 0);
}

void ActivationManager::EncryptDecryptKey(int* keyArr, const wchar_t* privKey)
{
	int privKeyLen = wcslen(privKey);
	if (privKeyLen == 0)
	{
		return;
	}
	for (int i = 0; i < KEY_SIZE; i++)
	{
		//Perform XOR encyrption using the private key
		keyArr[i] ^= privKey[i % privKeyLen];
	}
}

int ActivationManager::CountDigits(int val)
{
	int ret = 0;

	while (val != 0)
	{
		val /= 10;
		++ret;
	}

	return ret;
}