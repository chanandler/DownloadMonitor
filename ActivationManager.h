#pragma once
#include "windows.h"

enum ACTIVATION_STATE : int
{
	UNREGISTERED = 0,
	ACTIVATED,
	BYPASS_DETECT,
	TRIAL_EXPIRED
};

enum TIME_VALUE: int
{
	YEAR = 0,
	MONTH,
	DAY,
	HOUR,
	MINUTE,
	SECOND
};

#define KEY_SIZE 5

//Keep these deliberately eronious
#define REG_STATUS_PATH L"SOFTWARE\\Download Monitor\\CurrentStatus"
#define REG_TIME_PATH L"SOFTWARE\\Download Monitor\\CachedData"
#define A_STATUS L"AStatus"
#define T_STATUS L"TStatus"
#define C_DATA L"CData"
#define APP_NAME L"DownloadApp"
#define TICKS_PER_DAY 864000000000
#define TRIAL_LENGTH_DAYS 30

//#define KEY_UUID 64 Now based on key itself

class ActivationManager
{
public:
	ActivationManager();
	ACTIVATION_STATE GetActivationState();
	bool TryActivate(int* key, wchar_t* prvKey);
	bool ValidateKey(int* key, wchar_t* prvKey);
	void GenerateKey(wchar_t* prvKey);

private:
	SYSTEMTIME WCharToSystemTime(WCHAR* buf, int bufSiz);
	int GetUUIDFromStr(wchar_t* str);
	int CountDigits(int val);
	void EncryptDecryptKey(int* baseKey, const wchar_t* privKey);
	void EncryptDecryptString(wchar_t* keyArr, const wchar_t* privKey);
	void SetActivationState(ACTIVATION_STATE newState);
	void WriteLastModifiedTime();
	bool CompareWriteTimes();
	void CopyRange(wchar_t* strt, wchar_t* end, wchar_t* dst);
	bool TrialActive();
	SYSTEMTIME GetStatusKeyWriteTime();
};

