#pragma once
#include <windows.h>
#include <tuple>

#define CONFIG_NAME "downloadmonitor_config.cfg"

#define PARAM_COUNT 7

#define FOREGROUND_COLOUR "FOREGROUND_COLOUR"
#define CHILD_COLOUR "CHILD_COLOUR"
#define LAST_POS "LAST_POS"
#define OPACITY "OPACITY"
#define FONT "FONT"
#define UPLOAD_TXT_COLOUR "UPLOAD_TXT_COLOUR"
#define DOWNLOAD_TXT_COLOUR "DOWNLOAD_TXT_COLOUR"
#define SELECTED_ADAPTER "SELECTED_ADAPTER"
#define HOVER_MODE "HOVER_MODE"
#define DRAW_BORDER "DRAW_BORDER"
#define BORDER_W_H "BORDER_W_H"

#define CFG_SHOW_ALL "SHOW_ALL"
#define CFG_SHOW_WHEN_AVAILABLE "SHOW_WHEN_AVAILABLE"
#define CFG_DO_NOTHING "DO_NOTHING"

#define DRAG_TO_EXPOSE_GRAPH "DRAG_TO_EXPOSE_GRAPH"

enum FONT_ENUM
{
	HEIGHT = 0,
	WIDTH,
	ESCAPEMENT,
	ORIENTATION,
	WEIGHT,
	ITALIC,
	UNDERLINE,
	STRIKE_OUT,
	CHAR_SET,
	OUT_PRECISION,
	CLIP_PRECISION,
	QUALITY,
	PITCH_AND_FAMILY,
	FACE_NAME
};

enum HOVER_ENUM
{
	SHOW_ALL = 0,
	SHOW_WHEN_AVAILABLE,
	DO_NOTHING
};

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
	char* WideToAnsi(LPWSTR inStr);
	int ProcessInt(char* dataStart);
	char* ProcessChar(char* dataStart);
	LOGFONT ProcessFont(char* dataStart);
	std::tuple<int, int> ProcessCoords(char* dataStart);
	COLORREF ProcessRGB(char* dataStart);
	void CopyRange(char* start, char* end, char* buf, int size);
	char* configDir = nullptr;
	bool retriedOnce = false;

public:
	int lastX, lastY;
	int opacity;

	int borderWH;
	bool drawBorder;
	
	char* uniqueAddr;

	void ResetConfig();
	bool ReadData();

	ConfigManager(LPWSTR configDirOverride, class ThemeManager* themeManager);
	~ConfigManager();
	void UpdateSelectedAdapter(char* selAdapterPhysAddr);
	void UpdateForegroundColour(COLORREF fg_col);
	void UpdateChildColour(COLORREF ch_col);

	void UpdateUploadTextColour(COLORREF fg_col);

	void UpdateDownloadTextColour(COLORREF ch_col);

	void UpdateWindowPos(int x, int y);

	void UpdateFont(LOGFONT f);

	void UpdateOpacity(int newopacity);

	void UpdateHoverSetting(HOVER_ENUM newSetting);

	void UpdateDragToExposeGraph(bool newGraphSetting);

	void UpdateBorderEnabled(bool newBorder);

	void UpdateBorderWH(int newWH);

	void GetFullConfigPath(char* buf);

	void ApplyTheme(class Theme* newTheme);

	COLORREF* foregroundColour;
	COLORREF* childColour;

	COLORREF* uploadTxtColour;
	COLORREF* downloadTxtColour;

	COLORREF* customColBuf;

	LOGFONT* currentFont;

	HOVER_ENUM hoverSetting;

	bool dragToExposeGraph;

	class ThemeManager* themeManagerRef;
};

