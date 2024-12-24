#pragma once

enum ACTIVATION_STATE : int
{
	UNREGISTERED = 0,
	ACTIVATED
};

#define KEY_SIZE 16
#define KEY_UUID 64

class ActivationManager
{
public:
	ACTIVATION_STATE GetActivationState();
	bool TryActivate(int* key);
	bool ValidateKey(int* key);
	void GenerateKey();

private:
	void EncryptDecryptKey(int* baseKey, const wchar_t* privKey);
	void SetActivationState(ACTIVATION_STATE newState);
};

