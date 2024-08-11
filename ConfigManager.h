#pragma once
#include <windows.h>
#include <tuple>

#define CONFIG_NAME "downloadmonitor_config.cfg"

#define FOREGROUND_COLOUR "FOREGROUND_COLOUR"
#define CHILD_COLOUR "CHILD_COLOUR"
#define LAST_POS "LAST_POS"
#define OPACITY "OPACITY"

class ConfigManager
{
	//Needs to:
	//Write data to file
	//Read data from file

	//Start with:
	//Background colour
	//Foreground/child colour

	//Colour picker points to COLOREF structure in memory
	//Need to allocate then tell this to read/apply when Ok is clicked

private:
	void InitDefaults();
	void WriteData();
	void ReadData();
	int ProcessInt(char* dataStart);
	std::tuple<int, int> ProcessCoords(char* dataStart);
	COLORREF ProcessRGB(char* dataStart);
	void CopyRange(char* start, char* end, char* buf, int size);
public:

	int lastX, lastY;
	int opacity;
	void ResetConfig();

	ConfigManager();
	~ConfigManager();
	void UpdateForegroundColour(COLORREF fg_col);
	void UpdateChildColour(COLORREF ch_col);

	void UpdateWindowPos(int x, int y);

	void UpdateOpacity(int newopacity);

	void GetFullConfigPath(char* buf);

	COLORREF* foregroundColour;
	COLORREF* childColour;
	COLORREF* customColBuf;
};

