#include "ActivationManager.h"
#include "Windows.h"
#include <time.h>
#include <stdio.h>
#include "Common.h"

//Key/Activation overview:
//We generate a key with KEY_SIZE digits
//All digits, when added together, create a multiple of value generated from the private key
//This is our UUID that we check for after decrypting a key

//We then encrypt the key using XOR against the user's email address
//Meaning they need to use their email address to decrypt the key and retrieve the valid UUID

//When the activation status is set, we save the modification time to an encrypted key elsewhere
//Then when loading the activation status later, we check the key's current time against the expected time

//Contact key manager server to make sure key exists and hasn't been revoked
SERVER_RESPONSE ActivationManager::ContactKeyServer(WCHAR* emailBuf, int* key)
{
	WSAData data;
	int err;
	WORD vReq = MAKEWORD(2, 2);
	err = WSAStartup(vReq, &data);

	if (err != 0)
	{
		OnConnectionEnd();
		return SERVER_RESPONSE::NO_RESPONSE;
	}

	clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (clientSock == INVALID_SOCKET)
	{
		OnConnectionEnd();
		return SERVER_RESPONSE::NO_RESPONSE;
	}

	sockaddr_in service;
	service.sin_family = AF_INET;
#pragma warning (push)
#pragma warning (disable: 4996)
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
#pragma warning (pop)
	service.sin_port = htons(80);

	if (connect(clientSock, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		OnConnectionEnd();
		return SERVER_RESPONSE::NO_RESPONSE;
	}

	int result = -1;

	char sendBuf[NET_BUF_SIZE];


	WCHAR wBuf[256];
	KeyToWStr(key, wBuf, 256);

	//GetKeyFromReg(wBuf, 256);
	char* aKey = Common::WideToAnsi(wBuf);
	char* aEmail = Common::WideToAnsi(emailBuf);

	sprintf_s(sendBuf, "CHECK,%s,%s,", aEmail, aKey);

	delete[] aKey, aEmail;

	result = send(clientSock, sendBuf, strnlen_s(sendBuf, NET_BUF_SIZE), NULL);

	if(result == SOCKET_ERROR)
	{
		OnConnectionEnd();
		return SERVER_RESPONSE::NO_RESPONSE;
	}

	//Shutdown as we just need to wait for response
	result = shutdown(clientSock, SD_SEND);
	if (result == SOCKET_ERROR) 
	{
		OnConnectionEnd();
		return SERVER_RESPONSE::NO_RESPONSE;
	}

	char recvBuf[NET_BUF_SIZE];

	result = 1;
	while(result > 0)
	{
		result = recv(clientSock, recvBuf, NET_BUF_SIZE, NULL);
	}

	OnConnectionEnd();

	//Process result
	if(!strncmp(recvBuf, "STATUS", 6))
	{
		char* statusStrt = strchr(&recvBuf[0], ',');

		if(!statusStrt || !(statusStrt + 1))
		{
			return SERVER_RESPONSE::NO_RESPONSE;
		}

		++statusStrt;
		char* statusEnd = strchr(statusStrt, ',');

		if(!statusEnd)
		{
			return SERVER_RESPONSE::NO_RESPONSE;
		}

		char status[256];

		Common::CopyRange(statusStrt, statusEnd, status, 256);

		if(!strncmp(status, "TRUE", 4))
		{
			return SERVER_RESPONSE::VALID_KEY;
		}
		else
		{
			return SERVER_RESPONSE::INVALID_KEY;
		}
	}

	return SERVER_RESPONSE::NO_RESPONSE;
}

void ActivationManager::OnConnectionEnd()
{
	closesocket(clientSock);
	WSACleanup();
}

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

			WCHAR buf[512];
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
	if (!CompareWriteTimes())
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

	if (ret != ACTIVATION_STATE::TRIAL_EXPIRED && ret != ACTIVATION_STATE::ACTIVATED && !TrialActive())
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

	WCHAR buf[512];
	ZeroMemory(buf, 512);

	DWORD* bSize = new DWORD();
	*bSize = 512;
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

	if (remSeconds < 0)
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


void ActivationManager::WriteKeyToReg(int* key)
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

	WCHAR keyStr[256];
	ZeroMemory(keyStr, 256);

	for (int i = 0; i < KEY_SIZE; i++)
	{
		WCHAR buf[256];
		ZeroMemory(buf, 256);

		_itow_s(key[i], buf, 10);
		buf[10] = 0;
		wcscat_s(keyStr, buf);

		if (i < KEY_SIZE - 1)
		{
			wcscat_s(keyStr, L"-");
		}
	}

	LRESULT vRes = RegSetValueEx(kHandle, K_DATA, NULL, REG_SZ, (LPBYTE)&keyStr, 256);

	RegCloseKey(kHandle);

	WriteLastModifiedTime();
}

void ActivationManager::KeyToWStr(int* key, WCHAR* buf, size_t bufSize)
{
	ZeroMemory(buf, 256);
	WCHAR tempBuf[256];

	for (int i = 0; i < KEY_SIZE; i++)
	{
		ZeroMemory(tempBuf, 256);
		_itow_s(key[i], tempBuf, 10);
		wcscat(buf, tempBuf);

		if (i < KEY_SIZE - 1)
		{
			wcscat(buf, L"-");
		}
	}
}

bool ActivationManager::GetKeyFromReg(WCHAR* buf, DWORD bSize)
{
	HKEY kHandle = { 0 };
	SYSTEMTIME ret = { 0 };

	LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_STATUS_PATH, NULL,
		KEY_READ, &kHandle);

	if (kRes != ERROR_SUCCESS || kHandle == NULL)
	{
		return false;
	}

	LSTATUS vRes = RegQueryValueEx(kHandle, K_DATA, NULL, NULL, (LPBYTE)&buf[0], &bSize);

	RegCloseKey(kHandle);

	return (vRes == ERROR_SUCCESS);
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

	while (strt != end)
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

	if (prevSt.wYear == st.wYear && prevSt.wMonth == st.wMonth && prevSt.wDay == st.wDay && prevSt.wHour == st.wHour
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
		for (int i = 0; i < (dCount - 2); i++)
		{
			ret /= 10;
		}
	}

	return ret;
}

////TODO move this to a seperate key/license program
//void ActivationManager::GenerateKey(wchar_t* prvKey)
//{
//	//Whole key should == a multiple of UUID
//
//	srand((unsigned)time(NULL));
//
//	int key[KEY_SIZE] = { 0 };
//	int uuid = GetUUIDFromStr(prvKey);
//	int total = 0;
//	while (total == 0 || total % uuid != 0)
//	{
//		for (int i = 0; i < KEY_SIZE; i++)
//		{
//			total = 0;
//
//			//Add all digits together
//			for (int j = 0; j < KEY_SIZE; j++)
//			{
//				if (j == i)
//				{
//					continue;
//				}
//				total += key[j];
//			}
//
//			//Add random value between 1>20
//			int rVal = rand() % 21;
//			if (rVal == 0)
//			{
//				++rVal;
//			}
//
//			key[i] = rVal;
//
//			total += rVal;
//		}
//	}
//
//	EncryptDecryptKey(&key[0], prvKey);
//	int v = key[0];
//}

bool ActivationManager::TryActivate(int* key, wchar_t* prvKey)
{
	int* baseKey = (int*)malloc(sizeof(int) * KEY_SIZE);

	if(baseKey)
	{
		memcpy(baseKey, key, sizeof(int) * KEY_SIZE);
	}

	SERVER_RESPONSE servResp = ContactKeyServer(prvKey, key);

	if (servResp != SERVER_RESPONSE::VALID_KEY)
	{
		return false;
	}

	EncryptDecryptKey(&key[0], prvKey);

	if (!ValidateKey(key, prvKey))
	{
		return false;
	}

	SetActivationState(ACTIVATION_STATE::ACTIVATED);
	if (baseKey)
	{
		WriteKeyToReg(baseKey);
		free(baseKey);
	}
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