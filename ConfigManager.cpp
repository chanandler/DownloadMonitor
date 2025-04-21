#include "ConfigManager.h"
#include "ThemeManager.h"
#include <iostream>
#include <fstream> 
#include <string>
#include <tchar.h>

ConfigManager::ConfigManager(LPWSTR configDirOverride, ThemeManager* themeManager)
{
	themeManagerRef = themeManager;
	customColBuf = (COLORREF*)malloc(16 * sizeof(COLORREF));
	if (customColBuf)
	{
		memset(customColBuf, 0, 16 * sizeof(COLORREF));
	}
	foregroundColour = (COLORREF*)malloc(sizeof(COLORREF));
	childColour = (COLORREF*)malloc(sizeof(COLORREF));
	uploadTxtColour = (COLORREF*)malloc(sizeof(COLORREF));
	downloadTxtColour = (COLORREF*)malloc(sizeof(COLORREF));
	currentFont = (LOGFONT*)malloc(sizeof(LOGFONT));
	uniqueAddr = (char*)malloc(sizeof(char) * 32);

	hoverSetting = HOVER_ENUM::SHOW_ALL;

	if (configDirOverride)
	{
		char* ansiArgs = WideToAnsi(configDirOverride);

		if (ansiArgs)
		{
			if (!strncmp(ansiArgs, "-configdir=", 11)) //If we passed in an argument
			{
				configDir = &ansiArgs[11]; //Get the path
			}
		}
	}
}

ConfigManager::~ConfigManager()
{
	free(customColBuf);
	free(foregroundColour);
	free(childColour);
	free(currentFont);
	free(uploadTxtColour);
	free(downloadTxtColour);
	free(uniqueAddr);
	if (configDir)
	{
		free(configDir);
	}
}

void ConfigManager::UpdateSelectedAdapter(char* selAdapterPhysAddr)
{
	int end = 32;
	memcpy(uniqueAddr, selAdapterPhysAddr, end);
	uniqueAddr[end - 1] = 0;

	WriteData();
}

void ConfigManager::UpdateForegroundColour(COLORREF fg_col)
{
	*foregroundColour = fg_col;
	WriteData();
}

void ConfigManager::UpdateChildColour(COLORREF ch_col)
{
	*childColour = ch_col;
	WriteData();
}

void ConfigManager::UpdateUploadTextColour(COLORREF fg_col)
{
	*uploadTxtColour = fg_col;
	WriteData();
}

void ConfigManager::UpdateDownloadTextColour(COLORREF ch_col)
{
	*downloadTxtColour = ch_col;
	WriteData();
}

void ConfigManager::UpdateWindowPos(int x, int y)
{
	lastX = x;
	lastY = y;
	WriteData();
}

void ConfigManager::UpdateFont(LOGFONT f)
{
	*currentFont = f;
	WriteData();
}

void ConfigManager::UpdateOpacity(int newopacity)
{
	opacity = newopacity;
	WriteData();
}

void ConfigManager::UpdateHoverSetting(HOVER_ENUM newSetting)
{
	hoverSetting = newSetting;
	WriteData();
}

void ConfigManager::UpdateDragToExposeGraph(bool newGraphSetting)
{
	dragToExposeGraph = newGraphSetting;
	WriteData();
}

void ConfigManager::UpdateBorderEnabled(bool newBorder)
{
	drawBorder = newBorder;
	WriteData();
}

void ConfigManager::UpdateBorderWH(int newWH)
{
	borderWH = newWH;
	WriteData();
}

void ConfigManager::GetFullConfigPath(char* buf)
{
	//Either pass in override dir or use appdata
	char* appdata = getenv("APPDATA");
	sprintf_s(buf, MAX_PATH, "%s\\%s", configDir ? configDir : appdata, CONFIG_NAME);
}

void ConfigManager::ApplyTheme(Theme* newTheme)
{
	*foregroundColour = newTheme->fgColour;
	*childColour = newTheme->bgColour;
	*downloadTxtColour = newTheme->downloadTxtColour;
	*uploadTxtColour = newTheme->uploadTxtColour;
	opacity = newTheme->opacity;
	*currentFont = newTheme->font;
	drawBorder = newTheme->border;
	borderWH = newTheme->borderWH;

	WriteData();
}

void ConfigManager::WriteData()
{
	//So we don't have to worry about the order/line we write to, we just write every config setting at once
	//(Not very efficient)

	char fg_Buf[200];
	sprintf_s(fg_Buf, "%s=%i,%i,%i", FOREGROUND_COLOUR, GetRValue(*foregroundColour), GetGValue(*foregroundColour),
		GetBValue(*foregroundColour));

	char ch_Buf[200];
	sprintf_s(ch_Buf, "%s=%i,%i,%i", CHILD_COLOUR, GetRValue(*childColour), GetGValue(*childColour), GetBValue(*childColour));

	char lp_Buf[200];
	sprintf_s(lp_Buf, "%s=%i,%i", LAST_POS, lastX, lastY);

	char op_Buf[200];
	sprintf_s(op_Buf, "%s=%i", OPACITY, opacity);

	char fo_Buf[500];

	char* fontName = WideToAnsi(currentFont->lfFaceName);
	if (fontName)
	{
		sprintf_s(fo_Buf, "%s=%i,%i,%i,%i,%i,%u,%u,%u,%u,%u,%u,%u,%u,%s", FONT, currentFont->lfHeight, currentFont->lfWidth, currentFont->lfEscapement,
			currentFont->lfOrientation, currentFont->lfWeight, currentFont->lfItalic, currentFont->lfUnderline, currentFont->lfStrikeOut, currentFont->lfCharSet,
			currentFont->lfOutPrecision, currentFont->lfClipPrecision, currentFont->lfQuality, currentFont->lfPitchAndFamily, fontName);
	}

	char ul_txt_Buf[200];
	sprintf_s(ul_txt_Buf, "%s=%i,%i,%i", UPLOAD_TXT_COLOUR, GetRValue(*uploadTxtColour), GetGValue(*uploadTxtColour),
		GetBValue(*uploadTxtColour));

	char dl_txt_Buf[200];
	sprintf_s(dl_txt_Buf, "%s=%i,%i,%i", DOWNLOAD_TXT_COLOUR, GetRValue(*downloadTxtColour), GetGValue(*downloadTxtColour),
		GetBValue(*downloadTxtColour));

	char adapter_buf[200];
	sprintf_s(adapter_buf, "%s=%s", SELECTED_ADAPTER, uniqueAddr);


	char hSettingBuf[100];
	switch (hoverSetting)
	{
		case HOVER_ENUM::DO_NOTHING:
		{
			sprintf_s(hSettingBuf, "%s", CFG_DO_NOTHING);
			break;
		}
		case HOVER_ENUM::SHOW_ALL:
		{
			sprintf_s(hSettingBuf, "%s", CFG_SHOW_ALL);
			break;
		}
		case HOVER_ENUM::SHOW_WHEN_AVAILABLE:
		{
			sprintf_s(hSettingBuf, "%s", CFG_SHOW_WHEN_AVAILABLE);
			break;
		}
	}

	char hover_buf[200];
	sprintf_s(hover_buf, "%s=%s", HOVER_MODE, hSettingBuf);

	char draw_border_buf[200];
	sprintf_s(draw_border_buf, "%s=%s", DRAW_BORDER, drawBorder ? "TRUE" : "FALSE");

	char border_w_h_buf[200];
	sprintf_s(border_w_h_buf, "%s=%i", BORDER_W_H, borderWH);

	char drag_graph_buf[200];
	sprintf_s(drag_graph_buf, "%s=%s", DRAG_TO_EXPOSE_GRAPH, dragToExposeGraph ? "TRUE" : "FALSE");

	char pathBuf[MAX_PATH];
	GetFullConfigPath(pathBuf);
	std::ofstream configFile(pathBuf);

	// Write to the file
	configFile << fg_Buf << "\n" << ch_Buf << "\n" << lp_Buf << "\n" << op_Buf << "\n" << fo_Buf << "\n" << ul_txt_Buf << "\n" << dl_txt_Buf << "\n"
		<< adapter_buf << "\n" << hover_buf << "\n" << draw_border_buf << "\n" << border_w_h_buf << "\n" << drag_graph_buf << "\n";

	configFile.close();
}

//PURPOSE: Convert from Unicode(Wide), used by UIManager to Ansi
char* ConfigManager::WideToAnsi(LPWSTR inStr)
{
	//First get required buffer size
	int reqBufSize = WideCharToMultiByte(CP_UTF8, 0, inStr, -1, NULL, 0, NULL, NULL);

	//Alocate
	char* ret = (char*)malloc(reqBufSize);
	if (ret)
	{
		//Actually perform the conversion
		WideCharToMultiByte(CP_UTF8, 0, inStr, _tcslen(inStr), &ret[0], reqBufSize, NULL, NULL);
		ret[reqBufSize - 1] = 0;
	}

	return ret;
}

//PURPOSE: Read through all entries in config file and write to relevant variables, which are read by UIManager
bool ConfigManager::ReadData()
{
	char pathBuf[MAX_PATH];
	GetFullConfigPath(pathBuf);
	std::ifstream configFile(pathBuf);
	std::string output;

	int readCount = 0;
	bool invalidCfg = configFile.fail() ? true : false;

	while (!invalidCfg && getline(configFile, output))
	{
		char* dataStart = strchr((char*)output.c_str(), '=');

		if (!strncmp(output.c_str(), FOREGROUND_COLOUR, 14))
		{
			COLORREF val = ProcessRGB(dataStart);
			int r = GetRValue(val);

			if (r == -1)
			{
				invalidCfg = true;
				break;
			}

			++readCount;
			*foregroundColour = val;
		}
		else if (!strncmp(output.c_str(), CHILD_COLOUR, 12))
		{
			COLORREF val = ProcessRGB(dataStart);
			int r = GetRValue(val);

			if (r == -1)
			{
				invalidCfg = true;
				break;
			}

			*childColour = val;
			++readCount;
		}
		else if (!strncmp(output.c_str(), LAST_POS, 8))
		{
			std::tuple<int, int> coords = ProcessCoords(dataStart);

			if (std::get<0>(coords) == -1)
			{
				invalidCfg = true;
				break;
			}

			lastX = std::get<0>(coords);
			lastY = std::get<1>(coords);
			++readCount;
		}
		else if (!strncmp(output.c_str(), OPACITY, 7))
		{
			int val = ProcessInt(dataStart);
			if (val == -1)
			{
				invalidCfg = true;
				break;
			}
			opacity = val;
			++readCount;
		}
		else if (!strncmp(output.c_str(), FONT, 4))
		{
			LOGFONT font = ProcessFont(dataStart);
			if (font.lfHeight == 0)
			{
				invalidCfg = true;
				break;
			}
			*currentFont = font;
			++readCount;
		}
		else if (!strncmp(output.c_str(), UPLOAD_TXT_COLOUR, 17))
		{
			COLORREF val = ProcessRGB(dataStart);
			int r = GetRValue(val);

			if (r == -1)
			{
				invalidCfg = true;
				break;
			}

			*uploadTxtColour = val;
			++readCount;
		}
		else if (!strncmp(output.c_str(), DOWNLOAD_TXT_COLOUR, 17))
		{
			COLORREF val = ProcessRGB(dataStart);
			int r = GetRValue(val);

			if (r == -1)
			{
				invalidCfg = true;
				break;
			}

			*downloadTxtColour = val;
			++readCount;
		}
		else if (!strncmp(output.c_str(), SELECTED_ADAPTER, 16))
		{
			char* uID = ProcessChar(dataStart);
			if (!uID)
			{
				invalidCfg = true;
				break;
			}

			int end = strlen(uID);
			memcpy(uniqueAddr, uID, end);
			uniqueAddr[end] = 0;
			//*uniqueAddr = (UCHAR)*uID;

			free(uID);
			++readCount;
		}
		else if (!strncmp(output.c_str(), HOVER_MODE, 10))
		{
			char* hMode = ProcessChar(dataStart);
			if (!hMode)
			{
				invalidCfg = true;
				break;
			}

			if (!strcmp(hMode, CFG_SHOW_ALL))
			{
				hoverSetting = HOVER_ENUM::SHOW_ALL;
			}
			else if (!strcmp(hMode, CFG_SHOW_WHEN_AVAILABLE))
			{
				hoverSetting = HOVER_ENUM::SHOW_WHEN_AVAILABLE;
			}
			else if (!strcmp(hMode, CFG_DO_NOTHING))
			{
				hoverSetting = HOVER_ENUM::DO_NOTHING;
			}

			free(hMode);
			++readCount;
		}
		else if (!strncmp(output.c_str(), DRAW_BORDER, 11))
		{
			char* borderEnabled = ProcessChar(dataStart);
			if (!borderEnabled)
			{
				invalidCfg = true;
				break;
			}

			drawBorder = !strcmp(borderEnabled, "TRUE");

			free(borderEnabled);
			++readCount;
		}
		else if (!strncmp(output.c_str(), BORDER_W_H, 10))
		{
			int val = ProcessInt(dataStart);
			if (val == -1)
			{
				invalidCfg = true;
				break;
			}

			borderWH = val;

			++readCount;
		}
		else if (!strncmp(output.c_str(), DRAG_TO_EXPOSE_GRAPH, 20))
		{
			char* allowGraph = ProcessChar(dataStart);
			if (!allowGraph)
			{
				invalidCfg = true;
				break;
			}

			dragToExposeGraph = !strcmp(allowGraph, "TRUE");

			free(allowGraph);
			++readCount;
		}
	}

	configFile.close();


	if (invalidCfg || readCount < PARAM_COUNT)
	{
		if (retriedOnce)
		{
			//We've tried to read and write to a cfg file, but we've still failed.
			//Passed in path is most likely not valid
			return false;
		}

		retriedOnce = true;
		InitDefaults();
		return ReadData();
	}

	return true;
}

//PURPOSE: Read int value from config file
int ConfigManager::ProcessInt(char* dataStart)
{
	++dataStart;
	char* dataEnd = &dataStart[strlen(dataStart)];

	if (!dataEnd)
	{
		return -1;
	}

	char buf[20];
	CopyRange(dataStart, dataEnd, buf, 20);

	return atoi(buf);
}

//PURPOSE: Read char value from config file
char* ConfigManager::ProcessChar(char* dataStart)
{
	++dataStart;
	char* dataEnd = &dataStart[strlen(dataStart)];

	if (!dataEnd)
	{
		return nullptr;
	}

	char* buf = (char*)malloc(sizeof(char) * 32);
	if (!buf)
	{
		return nullptr;
	}

	CopyRange(dataStart, dataEnd, buf, 32);

	return buf;
}

//PURPOSE: Read comma seperated font parameters and construct into LOGFONT
LOGFONT ConfigManager::ProcessFont(char* dataStart)
{
	++dataStart;

	char* prevGood = dataStart;
	char* nxt = strchr(dataStart, ',');

	int pos = 0;
	LOGFONT ret = { 0 };

	if (!nxt)
	{
		return ret;
	}

	bool atEnd = false;

	//Fonts have so many parameters, so step through one by one with a state-based approach
	while (nxt)
	{
		char buf[200];
		CopyRange(prevGood, nxt, buf, 200);


		switch ((FONT_ENUM)pos)
		{
			case HEIGHT:
			{
				ret.lfHeight = atoi(buf);
				break;
			}
			case WIDTH:
			{
				ret.lfWidth = atoi(buf);
				break;
			}
			case ESCAPEMENT:
			{
				ret.lfEscapement = atoi(buf);
				break;
			}
			case ORIENTATION:
			{
				ret.lfOrientation = atoi(buf);
				break;
			}
			case WEIGHT:
			{
				ret.lfWeight = atoi(buf);
				break;
			}
			case ITALIC:
			{
				ret.lfItalic = strtoul(buf, nullptr, 10);
				break;
			}
			case UNDERLINE:
			{
				ret.lfUnderline = strtoul(buf, nullptr, 10);
				break;
			}
			case STRIKE_OUT:
			{
				ret.lfStrikeOut = strtoul(buf, nullptr, 10);
				break;
			}
			case CHAR_SET:
			{
				ret.lfCharSet = strtoul(buf, nullptr, 10);
				break;
			}
			case OUT_PRECISION:
			{
				ret.lfOutPrecision = strtoul(buf, nullptr, 10);
				break;
			}
			case CLIP_PRECISION:
			{
				ret.lfClipPrecision = strtoul(buf, nullptr, 10);
				break;
			}
			case QUALITY:
			{
				ret.lfQuality = strtoul(buf, nullptr, 10);
				break;
			}
			case PITCH_AND_FAMILY:
			{
				ret.lfPitchAndFamily = strtoul(buf, nullptr, 10);
				break;
			}
			case FACE_NAME:
			{
				swprintf(ret.lfFaceName, LF_FACESIZE, L"%hs", buf);
				break;
			}
		}

		nxt++;
		prevGood = nxt;
		nxt = strchr(nxt, ',');
		if (!nxt && !atEnd)
		{
			atEnd = true;
			nxt = &dataStart[strlen(dataStart)];
		}

		pos++;
	}

	return ret;
}

//PURPOSE: Read comma seperated coordinate value and return tuple of ints
std::tuple<int, int> ConfigManager::ProcessCoords(char* dataStart)
{
	++dataStart;
	char* prevGood = dataStart;
	char* nxt = strchr(dataStart, ',');
	if (!nxt)
	{
		return std::make_tuple(-1, -1);
	}
	int colIndex = 0;

	char xBuf[100];
	xBuf[99] = 0;

	char yBuf[100];
	yBuf[99] = 0;

	bool atEnd = false;

	while (nxt)
	{
		char* writeDest;

		if (colIndex == 0)
		{
			writeDest = &xBuf[0];
		}
		else if (colIndex == 1)
		{
			writeDest = &yBuf[0];
		}
		else
		{
			break;
		}

		CopyRange(prevGood, nxt, writeDest, 100);
		nxt++;
		prevGood = nxt;
		nxt = strchr(nxt, ',');
		if (!nxt && !atEnd)
		{
			atEnd = true;
			nxt = &dataStart[strlen(dataStart)];
		}
		++colIndex;
	}

	int x = atoi(xBuf);
	int y = atoi(yBuf);

	return std::make_tuple(x, y);
}

//PURPOSE: Read comma seperated RGB value and construct into COLOREF
COLORREF ConfigManager::ProcessRGB(char* dataStart)
{
	if (!dataStart)
	{
		return RGB(-1, -1, -1);
	}
	++dataStart;
	char* prevGood = dataStart;
	char* nxt = strchr(dataStart, ',');

	//Iterate along the relevant buffers
	int colIndex = 0;

	char redBuf[100];
	redBuf[99] = 0;

	char greenBuf[100];
	greenBuf[99] = 0;

	char blueBuf[100];
	blueBuf[99] = 0;

	bool atEnd = false;

	while (nxt)
	{
		char* writeDest;

		if (colIndex == 0)
		{
			writeDest = &redBuf[0];
		}
		else if (colIndex == 1)
		{
			writeDest = &greenBuf[0];
		}
		else if (colIndex == 2)
		{
			writeDest = &blueBuf[0];
		}
		else
		{
			break;
		}

		CopyRange(prevGood, nxt, writeDest, 100);
		nxt++;
		prevGood = nxt;
		nxt = strchr(nxt, ',');
		if (!nxt && !atEnd)
		{
			atEnd = true;
			nxt = &dataStart[strlen(dataStart)];
		}
		++colIndex;
	}

	int r = atoi(redBuf);
	int g = atoi(greenBuf);
	int b = atoi(blueBuf);

	return RGB(r, g, b);
}

void ConfigManager::ResetConfig()
{
	InitDefaults();
	ReadData();
}

void ConfigManager::InitDefaults()
{
	//Instead of writing a default config, we now
	//just set important variables then apply a default theme, as this will
	//init everything we need for a new cfg

	/*
	char pathBuf[MAX_PATH];
	GetFullConfigPath(pathBuf);

	std::ofstream newConfigFile(pathBuf);

	newConfigFile << defaultConfig;

	newConfigFile.close();*/

	lastX = 1200;
	strcpy_s(uniqueAddr, 32, "AUTO");
	hoverSetting = HOVER_ENUM::SHOW_ALL;
	dragToExposeGraph = true;

	if (themeManagerRef)
	{
		Theme* newTheme = themeManagerRef->GetTheme(AVAILABLE_THEME::SLATE_GREY);
		if (!newTheme)
		{
			return;
		}

		ApplyTheme(newTheme);
		delete newTheme;
	}
}

void ConfigManager::CopyRange(char* start, char* end, char* buf, int size)
{
	memset(buf, 0, size);
	int written = 0;
	while (start != end && written < size)
	{
		*buf = *start;
		++buf; ++start;

		written++;
	}
}