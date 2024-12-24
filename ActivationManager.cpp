#include "ActivationManager.h"
#include "Windows.h"

ACTIVATION_STATE ActivationManager::GetActivationState()
{
    HKEY kHandle = { 0 };

    LRESULT kRes = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Download Monitor\\CurrentStatus", NULL,
        KEY_READ, &kHandle);

    if (kRes != ERROR_SUCCESS || kHandle == NULL)
    {
        return ACTIVATION_STATE::UNKNOWN;
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

    LRESULT vRes = RegSetValueEx(kHandle, L"AStatus", NULL,  REG_DWORD, (LPBYTE)ptr, sizeof(int));

    RegCloseKey(kHandle);
    delete ptr;


}