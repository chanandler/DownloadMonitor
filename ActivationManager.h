#pragma once

enum ACTIVATION_STATE : int
{
	UNKNOWN = -1,
	UNREGISTERED = 0,
	ACTIVATED,
	EXPIRED
};

class ActivationManager
{
public:
	ACTIVATION_STATE GetActivationState();
	void SetActivationState(ACTIVATION_STATE newState);
};

