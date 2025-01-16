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

			fontParams.lfHeight = -15;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 34;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Tahoma");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, true, 0, L"Slate Grey");
			break;
		}
		case AVAILABLE_THEME::ICE_COOL:
		{
			COLORREF fg = RGB(100, 165, 253);
			COLORREF bg = RGB(202, 222, 255);
			COLORREF ut = RGB(0, 183, 91);
			COLORREF dt = RGB(168, 0, 0);
			int opacity = 230;

			fontParams.lfHeight = -15;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 34;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Liberation Sans");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, true, 0, L"Ice Cool");
			break;
		}
		case AVAILABLE_THEME::NIGHT_RIDER:
		{
			COLORREF fg = RGB(4, 4, 0);
			COLORREF bg = RGB(0, 0, 0);
			COLORREF ut = RGB(57, 249, 63);
			COLORREF dt = RGB(255, 0, 0);
			int opacity = 230;

			fontParams.lfHeight = -12;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 49;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Lucida Console");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, true, 0, L"Night Rider");
			break;
		}
		case AVAILABLE_THEME::SUNNY:
		{
			COLORREF fg = RGB(245, 242, 109);
			COLORREF bg = RGB(252, 248, 200);
			COLORREF ut = RGB(0, 215, 54);
			COLORREF dt = RGB(255, 0, 0);
			int opacity = 230;

			fontParams.lfHeight = -13;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 49;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Cascadia Code");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, true, 0, L"Sunny");
			break;
		}
		case AVAILABLE_THEME::GHOST:
		{
			COLORREF fg = RGB(47, 47, 47);
			COLORREF bg = RGB(17, 17, 17);
			COLORREF ut = RGB(188, 188, 188);
			COLORREF dt = RGB(188, 188, 188);
			int opacity = 135;

			fontParams.lfHeight = -13;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 34;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Verdana");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, true, 0, L"Ghost");
			break;
		}
		case AVAILABLE_THEME::BORDERLESS:
		{
			COLORREF fg = RGB(44, 44, 44);
			COLORREF bg = RGB(44, 44, 44);
			COLORREF ut = RGB(57, 249, 63);
			COLORREF dt = RGB(255, 255, 255);
			int opacity = 230;

			fontParams.lfHeight = -13;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Verdana");
			fontParams.lfPitchAndFamily = 34;

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, false, 0, L"Borderless");
			break;
		}
		case AVAILABLE_THEME::GLASSES:
		{
			COLORREF fg = RGB(0, 0, 0);
			COLORREF bg = RGB(36, 29, 78);
			COLORREF ut = RGB(20, 248, 26);
			COLORREF dt = RGB(255, 255, 255);
			int opacity = 230;

			fontParams.lfHeight = -13;
			fontParams.lfWeight = 400;
			fontParams.lfOutPrecision = 3;
			fontParams.lfClipPrecision = 2;
			fontParams.lfQuality = 1;
			fontParams.lfPitchAndFamily = 34;
			wcscpy_s(fontParams.lfFaceName, LF_FACESIZE, L"Yu Gothic UI");

			ret = new Theme(fg, bg, ut, dt, fontParams, opacity, true, 20, L"Glasses");
			break;
		}
	}
	return ret;
}
