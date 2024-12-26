#pragma once

enum ACTIVATION_STATE : int
{
	UNREGISTERED = 0,
	ACTIVATED
};

#define KEY_SIZE 5
#define REG_PATH L"SOFTWARE\\Download Monitor\\CurrentStatus"
#define A_STATUS L"AStatus"
//#define KEY_UUID 64 Now based on key itself

class ActivationManager
{
public:
	ACTIVATION_STATE GetActivationState();
	bool TryActivate(int* key, wchar_t* prvKey);
	bool ValidateKey(int* key, wchar_t* prvKey);
	void GenerateKey(wchar_t* prvKey);

private:
	int GetUUIDFromStr(wchar_t* str);
	int CountDigits(int val);
	void EncryptDecryptKey(int* baseKey, const wchar_t* privKey);
	void SetActivationState(ACTIVATION_STATE newState);
};

