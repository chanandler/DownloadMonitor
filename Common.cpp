#include "Common.h"
#include <corecrt_wstring.h>
#include <time.h>
#include <stdlib.h>
#include "Windows.h"

//Static util functions

int Common::CountDigits(int val)
{
	int ret = 0;

	while (val != 0)
	{
		val /= 10;
		++ret;
	}

	return ret;
}

int Common::GetUUIDFromStr(wchar_t* str)
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

void Common::EncryptDecryptKey(int* keyArr, const wchar_t* privKey)
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

int* Common::GenerateKey(wchar_t* prvKey)
{
	//Whole key should == a multiple of UUID

	srand((unsigned)time(NULL));

	int* key = new int[KEY_SIZE];
	for (int i = 0; i < KEY_SIZE; i++)
	{
		key[i] = 0;
	}
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
	return key;
}

wchar_t* Common::AnsiToWide(char* ansi)
{
	size_t bufSize = MultiByteToWideChar(CP_UTF8, 0, ansi, -1, NULL, 0);
	wchar_t* buf = new wchar_t[bufSize];
	MultiByteToWideChar(CP_UTF8, 0, ansi, -1, buf, bufSize);
	return buf;
}

char* Common::WideToAnsi(wchar_t* wide)
{
	int wideLen = wcslen(wide);
	size_t buf_size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
	char* buf = new char[buf_size];
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf, buf_size, NULL, NULL);
	return buf;
}

bool Common::Readable(SOCKET* socket)
{
	if (socket == nullptr)
	{
		return false;
	}
	static timeval dontBlock = { 0,0 }; //We poll this, don't make us block
	fd_set rfd;
	FD_ZERO(&rfd);
	FD_SET(*socket, &rfd);
	int ret = select(0, &rfd, NULL, NULL, &dontBlock);
	return ret != SOCKET_ERROR && ret > 0;
}

bool Common::Writable(SOCKET* socket)
{
	if (socket == nullptr)
	{
		return false;
	}
	static timeval dontBlock = { 0,0 };
	fd_set wfd;
	FD_ZERO(&wfd);
	FD_SET(*socket, &wfd);
	int ret = select(0, NULL, &wfd, NULL, &dontBlock);
	return ret != SOCKET_ERROR && ret > 0;
}

void Common::CopyRange(char* start, char* end, char* buf, int size)
{
	memset(buf, 0, size);
	int written = 0;
	while (start != end && written < size)
	{
		*buf = *start;
		++buf; ++start;

		written++;
	}

	//buf[size] = 0;
}
