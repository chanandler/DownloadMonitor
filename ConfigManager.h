#pragma once
#include <windows.h>
#include <tuple>

#define CONFIG_NAME "config.cfg"

#define FOREGROUND_COLOUR "FOREGROUND_COLOUR"
#define CHILD_COLOUR "CHILD_COLOUR"
#define LAST_POS "LAST_POS"

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
	std::tuple<int, int> ProcessCoords(char* dataStart);
	COLORREF ProcessRGB(char* dataStart);
	void CopyRange(char* start, char* end, char* buf, int size);
public:

	int lastX, lastY;
	void ResetColours();

	ConfigManager();
	~ConfigManager();
	void UpdateForegroundColour(COLORREF fg_col);
	void UpdateChildColour(COLORREF ch_col);

	void UpdateWindowPos(int x, int y);

	COLORREF foregroundColour;
	COLORREF childColour;
};

