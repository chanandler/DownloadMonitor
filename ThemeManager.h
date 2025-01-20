#pragma once
#include <wtypes.h>

class Theme
{
public:
	COLORREF fgColour;
	COLORREF bgColour;
	COLORREF uploadTxtColour;
	COLORREF downloadTxtColour;
	LOGFONT font;
	int opacity;
	bool border;
	int borderWH;

	WCHAR name[250];

	Theme(COLORREF FG, COLORREF BG, COLORREF UT, COLORREF DT, LOGFONT Font, int Opacity, bool Border, int BorderWH, const wchar_t Name[])
	{
		fgColour = FG;
		bgColour = BG;
		uploadTxtColour = UT;
		downloadTxtColour = DT;
		font = Font;
		opacity = Opacity;
		border = Border;
		borderWH = BorderWH;
		wcscpy_s(name, Name);
	}
};

enum AVAILABLE_THEME
{
	SLATE_GREY,
	ICE_COOL,
	NIGHT_RIDER,
	SUNNY,
	GHOST,
	BORDERLESS,
	GLASSES,
	STEAM_PUNK,
	CLEAN,
	DIGITAL,
	ORANGE
};

class ThemeManager
{
public:
	Theme* GetTheme(AVAILABLE_THEME reqTheme);
};

