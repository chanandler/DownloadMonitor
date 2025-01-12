#include "ActivationManager.h"
#include "Windows.h"
#include <time.h>

//Key/Activation overview:
//We generate a key with KEY_SIZE digits
//All digits, when added together, create a multiple of value generated from the private key
//This is our UUID that we check for after decrypting a key

//We then encrypt the key using XOR against the user's email address
//Meaning they need to use their email address to decrypt the key and retrieve the valid UUID

//When the activation status is set, we save the modification time to an encrypted key elsewhere
//Then when loading the activation status later, we check the key's current time against the expected time

//TODO
//Contact key manager server to verify key


ActivationManager::ActivationManager()
{
	HKEY kHandle = { 0 };

	//Check activation status
	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
		KEY_READ, &kHandle);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		//Key not present, init to unregistered
		SetActivationState(ACTIVATION_STATE::UNREGISTERED);

		//Now key has been added, try again
		kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
			KEY_READ, &kHandle);
	}

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return;
	}

	LSTATUS trialRes = RegQueryValueEx(kHandle, T_STATUS, NULL, NULL, NULL, NULL);

	//Init the trial value, if required
	if (trialRes == ERROR_FILE_NOT_FOUND)
	{
		kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
			KEY_ALL_ACCESS, &kHandle);

		if (kRes == ERROR_SUCCESS && kHandle != NULL)
		{
			//Encode current time and save to value
			SYSTEMTIME st = { 0 };
			GetSystemTime(&st);

			WCHAR buf[250];
			wsprintf(buf, L"%02d, %02d, %02d, %02d, %02d, %02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

			EncryptDecryptString(&buf[0], APP_NAME);
			LRESULT vRes = RegSetValueEx(kHandle, T_STATUS, NULL, REG_SZ, (LPBYTE)&buf, 250);
			WriteLastModifiedTime();
		}
	}

	RegCloseKey(kHandle);
}
	
ACTIVATION_STATE ActivationManager::GetActivationState()
{
	HKEY kHandle = { 0 };

	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
		KEY_READ, &kHandle);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		//Key not present, init to unregistered
		SetActivationState(ACTIVATION_STATE::UNREGISTERED);
		return ACTIVATION_STATE::UNREGISTERED;
	}

	//The value for activation status has been modified at a time different to when we last modified it
	if(!CompareWriteTimes())
	{
		SetActivationState(ACTIVATION_STATE::BYPASS_DETECT);
		return ACTIVATION_STATE::BYPASS_DETECT;
	}

	int* aStatus = new int();
	DWORD* bSize = new DWORD();
	*bSize = sizeof(int);

	LSTATUS vRes = RegQueryValueEx(kHandle, A_STATUS, NULL, NULL, (LPBYTE)aStatus, bSize);

	ACTIVATION_STATE ret = (ACTIVATION_STATE)*aStatus;

	RegCloseKey(kHandle);
	delete aStatus, bSize;

	if(ret != ACTIVATION_STATE::TRIAL_EXPIRED && ret != ACTIVATION_STATE::ACTIVATED && !TrialActive())
	{
		SetActivationState(ACTIVATION_STATE::TRIAL_EXPIRED);
		return ACTIVATION_STATE::TRIAL_EXPIRED;
	}

	return ret;
}

int ActivationManager::GetRemainingTrialDays()
{
	HKEY kHandle = { 0 };

	//Check activation status
	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
		KEY_READ, &kHandle);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return false;
	}

	WCHAR buf[250];
	ZeroMemory(buf, 250);

	DWORD* bSize = new DWORD();
	*bSize = 250;
	LSTATUS trialRes = RegQueryValueEx(kHandle, T_STATUS, NULL, NULL, (LPBYTE)&buf[0], bSize);

	if (trialRes != ERROR_SUCCESS)
	{
		RegCloseKey(kHandle);
		return false;
	}

	//Decrypt
	EncryptDecryptString(&buf[0], APP_NAME);

	SYSTEMTIME prevSt = WCharToSystemTime(&buf[0], 250);
	SYSTEMTIME st = { 0 };
	GetSystemTime(&st);

	//Convert to FILETIME

	FILETIME prevFt = { 0 };
	FILETIME ft = { 0 };

	bool a = SystemTimeToFileTime(&prevSt, &prevFt);
	bool b = SystemTimeToFileTime(&st, &ft);

	if (!a || !b)
	{
		return false;
	}

	//Doing arithmetic with FILETIME directly is not recommened, so pass into a _ULARGE_INTEGER union
	_ULARGE_INTEGER prevTime = { 0 };
	prevTime.LowPart = prevFt.dwLowDateTime;
	prevTime.HighPart = prevFt.dwHighDateTime;

	_ULARGE_INTEGER time = { 0 };
	time.LowPart = ft.dwLowDateTime;
	time.HighPart = ft.dwHighDateTime;

	INT64 remSeconds = ((TICKS_PER_DAY * TRIAL_LENGTH_DAYS) - (time.QuadPart - prevTime.QuadPart));

	if(remSeconds < 0)
	{
		return 0;
	}

	return ((TICKS_PER_DAY * TRIAL_LENGTH_DAYS) - (time.QuadPart - prevTime.QuadPart)) / TICKS_PER_DAY;
}

bool ActivationManager::TrialActive()
{
	return GetRemainingTrialDays() > 0;
}

//Expects string formatted as: Year,Month,Day,Hour,Minute,Second
SYSTEMTIME ActivationManager::WCharToSystemTime(WCHAR* buf, int bufSiz)
{
	TIME_VALUE tVal = TIME_VALUE::YEAR;

	WCHAR* strt = &buf[0];
	WCHAR* end = wcschr(strt, L',');

	SYSTEMTIME st = { 0 };
	bool done = false;
	while (strt && end && !done)
	{
		WCHAR tempBuf[250];
		CopyRange(strt, end, tempBuf);

		strt = end + 1;

		switch (tVal)
		{
			case YEAR:
				st.wYear = _wtoi(tempBuf);
				tVal = TIME_VALUE::MONTH;
				break;
			case MONTH:
				st.wMonth = _wtoi(tempBuf);
				tVal = TIME_VALUE::DAY;
				break;
			case DAY:
				st.wDay = _wtoi(tempBuf);
				tVal = TIME_VALUE::HOUR;
				break;
			case HOUR:
				st.wHour = _wtoi(tempBuf);
				tVal = TIME_VALUE::MINUTE;
				break;
			case MINUTE:
				st.wMinute = _wtoi(tempBuf);
				tVal = TIME_VALUE::SECOND;
				break;
			case SECOND:
				st.wSecond = _wtoi(tempBuf);
				done = true;
				break;
		}

		if (tVal != TIME_VALUE::SECOND)
		{
			end = wcschr(strt, L',');
		}
		else
		{
			end = &buf[bufSiz - 1];
		}
	}

	return st;
}

void ActivationManager::SetActivationState(ACTIVATION_STATE newState)
{
	HKEY kHandle = { 0 };
	//Create/open key in reg

	LRESULT kRes = RegCreateKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
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

	WriteLastModifiedTime();

	delete ptr;
}

void ActivationManager::WriteLastModifiedTime()
{
	//Sneaky way to store the last write time on the user's PC

	HKEY kHandle = { 0 };

	LRESULT kRes = RegCreateKeyEx(HKEY_CURRENT_USER, REG_TIME_PATH, NULL,
		NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
		NULL, &kHandle, NULL);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return;
	}

	//Get last write time of key
	SYSTEMTIME st = GetStatusKeyWriteTime();

	//Place into a string
	WCHAR buf[250];
	wsprintf(buf, L"%02d, %02d, %02d, %02d, %02d, %02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	//Encode
	EncryptDecryptString(&buf[0], APP_NAME);
	
	//Write
	LRESULT vRes = RegSetValueEx(kHandle, C_DATA, NULL, REG_SZ, (LPBYTE)&buf, 250);

	RegCloseKey(kHandle);
}

void ActivationManager::CopyRange(wchar_t* strt, wchar_t* end, wchar_t* dst)
{
	if (!strt || !end || !dst)
	{
		return;
	}

	while(strt != end)
	{
		*dst = *strt;
		++strt;
		++dst;
	}

	*dst = 0;
}

bool ActivationManager::CompareWriteTimes()
{
	//Retrieve expected last write time, compare with actual last write time
	HKEY kHandle = { 0 };

	LRESULT kRes = RegCreateKeyEx(HKEY_CURRENT_USER, REG_TIME_PATH, NULL,
		NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
		NULL, &kHandle, NULL);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return false;
	}

	WCHAR buf[250]; //Contains our saved last write time
	ZeroMemory(buf, 250);
	DWORD* bSize = new DWORD();
	*bSize = 250;
	LSTATUS vRes = RegQueryValueEx(kHandle, C_DATA, NULL, NULL, (LPBYTE)&buf[0], bSize);

	if (vRes != ERROR_SUCCESS)
	{
		RegCloseKey(kHandle);
		return false; //No write data, if this is false but we have an activation key then something has been modified
	}

	EncryptDecryptString(&buf[0], APP_NAME);

	RegCloseKey(kHandle);

	//Construct into new SYSTEMTIME structure

	SYSTEMTIME prevSt = WCharToSystemTime(&buf[0], 250);

	//Query actual key
	
	SYSTEMTIME st = GetStatusKeyWriteTime(); //Get the key's modified value rather than the current time as we're comparing down to the second

	if(prevSt.wYear == st.wYear && prevSt.wMonth == st.wMonth && prevSt.wDay == st.wDay && prevSt.wHour == st.wHour
		&& prevSt.wMinute == st.wMinute && prevSt.wSecond == st.wSecond)
	{
		return true;
	}

	return false;
}

SYSTEMTIME ActivationManager::GetStatusKeyWriteTime()
{
	HKEY kHandle = { 0 };
	SYSTEMTIME ret = { 0 };

	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
		KEY_READ, &kHandle);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return ret;
	}

	FILETIME ft = { 0 };

	RegQueryInfoKey(kHandle, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, &ft);

	RegCloseKey(kHandle);

	FileTimeToSystemTime(&ft, &ret);

	return ret;
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

void ActivationManager::EncryptDecryptString(wchar_t* str, const wchar_t* privKey)
{
	int privKeyLen = wcslen(privKey);
	int strLen = wcslen(str);
	if (privKeyLen == 0 || strLen == 0)
	{
		return;
	}
	for (int i = 0; i < strLen; i++)
	{
		//Perform XOR encyrption using the private key
		str[i] ^= privKey[i % privKeyLen];
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