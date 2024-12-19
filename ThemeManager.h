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

	WCHAR name[250];

	Theme(COLORREF FG, COLORREF BG, COLORREF UT, COLORREF DT, LOGFONT Font, int Opacity, const wchar_t Name[])
	{
		fgColour = FG;
		bgColour = BG;
		uploadTxtColour = UT;
		downloadTxtColour = DT;
		font = Font;
		opacity = Opacity;
		wcscpy_s(name, Name);
	}
};

enum AVAILABLE_THEME
{
	SLATE_GREY,
	ICE_COOL,
	NIGHT_RIDER,
	SUNNY
};

class ThemeManager
{
public:
	Theme* GetTheme(AVAILABLE_THEME reqTheme);
};

