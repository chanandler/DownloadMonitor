#pragma once
#include <WinSock2.h>

#define SERVER_MAJOR 1
#define SERVER_MINOR 2

#define KEY_SIZE 5

class Common 
{
public:
	static int CountDigits(int val);
	static int GetUUIDFromStr(wchar_t* str);
	static void EncryptDecryptKey(int* keyArr, const wchar_t* privKey);
	static int* GenerateKey(wchar_t* prvKey);

	static wchar_t* AnsiToWide(char* ansi);
	static char* WideToAnsi(wchar_t* wide);
	
	static bool Readable(SOCKET* socket);
	static bool Writable(SOCKET* socket);

	static void CopyRange(char* start, char* end, char* buf, int size);
};
