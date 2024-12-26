#pragma once

enum ACTIVATION_STATE : int
{
	UNREGISTERED = 0,
	ACTIVATED
};

#define KEY_SIZE 5
#define KEY_UUID 64

class ActivationManager
{
public:
	ACTIVATION_STATE GetActivationState();
	bool TryActivate(int* key, wchar_t* prvKey);
	bool ValidateKey(int* key);
	void GenerateKey();

private:
	int CountDigits(int val);
	void EncryptDecryptKey(int* baseKey, const wchar_t* privKey);
	void SetActivationState(ACTIVATION_STATE newState);
};

