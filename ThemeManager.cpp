#include "ThemeManager.h"
#include <fstream> 

Theme* ThemeManager::GetTheme(AVAILABLE_THEME reqTheme)
{
	Theme* ret = nullptr;
	LOGFONT fontParams = { 0 };
	fontParams.lfCharSet = ANSI_CHARSET;

	switch (reqTheme)
	{
		case AVAILABLE_THEME::SLATE_GREY:
		{
			COLORREF fg = RGB(4, 4, 0);
			COLORREF bg = RGB(35, 35, 35);
			COLORREF ut = RGB(57, 249, 63);
			COLORREF dt = RGB(255, 255, 255);
			int opacity = 230;

			//Tahoma font
			fontParams.lfHeight = -15;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 34;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Tahoma");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, L"Slate Grey");
			break;
		}
		//TODO add more themes
		case AVAILABLE_THEME::ICE_COOL:
		{
			break;
		}
		case AVAILABLE_THEME::NIGHT_RIDER:
		{
			break;
		}
		case AVAILABLE_THEME::SUNNY:
		{
			COLORREF fg = RGB(245, 242, 109);
			COLORREF bg = RGB(252, 248, 200);
			COLORREF ut = RGB(0, 215, 54);
			COLORREF dt = RGB(255, 0, 0);
			int opacity = 230;

			//Tahoma font
			fontParams.lfHeight = -13;
			fontParams.lfWeight = 700;
			fontParams.lfOutPrecision = 1;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 34;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"System");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, L"Sunny");
			break;
		}
	}
	return ret;
}
