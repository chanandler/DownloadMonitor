#include "ConfigManager.h"
#include <iostream>
#include <fstream> 
#include <string>
#include <tchar.h>

class IntRGB
{
public:
    int r;
    int g;
    int b;

    IntRGB(int R, int G, int B)
    {
        r = R;
        g = G;
        b = B;
    }
};

const char* defaultConfig = "FOREGROUND_COLOUR=245,242,109\nCHILD_COLOUR=252,248,200\nLAST_POS=1200,0\nOPACITY=230\nFONT=-13,0,0,0,700,0,0,0,0,1,2,1,34,System\n";

ConfigManager::ConfigManager(char* configDirOverride)
{
    customColBuf = (COLORREF*)malloc(sizeof(COLORREF));
    foregroundColour = (COLORREF*)malloc(sizeof(COLORREF));
    childColour = (COLORREF*)malloc(sizeof(COLORREF));
    configDir = configDirOverride;
    ReadData();
}

ConfigManager::~ConfigManager()
{
    free(customColBuf);
    free(foregroundColour);
    free(childColour);
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

void ConfigManager::UpdateWindowPos(int x, int y)
{
    lastX = x;
    lastY = y;
    WriteData();
}

void ConfigManager::UpdateFont(LOGFONT f)
{
    if(currentFont)
    {
        free(currentFont);
    }

    currentFont = (LOGFONT*)malloc(sizeof(LOGFONT));
    memcpy(currentFont, &f, sizeof(LOGFONT));
    WriteData();
}

void ConfigManager::UpdateOpacity(int newopacity)
{
    opacity = newopacity;
    WriteData();
}

void ConfigManager::GetFullConfigPath(char* buf)
{
    //Either pass in override dir or use appdata
    char* appdata = getenv("APPDATA");
    sprintf_s(buf, MAX_PATH, "%s\\%s", configDir ? configDir : appdata, CONFIG_NAME);
}

void ConfigManager::WriteData()
{
    //So we don't have to worry about the order/line we write to, we just write every config setting at once
    //(Not very efficient)
    int fg_r = GetRValue(*foregroundColour);
    int fg_g = GetGValue(*foregroundColour);
    int fg_b = GetBValue(*foregroundColour);

    IntRGB* fg_RGB = new IntRGB(fg_r, fg_g, fg_b);

    int ch_r = GetRValue(*childColour);
    int ch_g = GetGValue(*childColour);
    int ch_b = GetBValue(*childColour);

    IntRGB* ch_RGB = new IntRGB(ch_r, ch_g, ch_b);

    char fg_Buf[200];
    sprintf_s(fg_Buf, "%s=%i,%i,%i", FOREGROUND_COLOUR, fg_RGB->r, fg_RGB->g, fg_RGB->b);

    char ch_Buf[200];
    sprintf_s(ch_Buf, "%s=%i,%i,%i", CHILD_COLOUR, ch_RGB->r, ch_RGB->g, ch_RGB->b);

    char lp_Buf[200];
    sprintf_s(lp_Buf, "%s=%i,%i", LAST_POS, lastX, lastY);

    char op_Buf[200];
    sprintf_s(op_Buf, "%s=%i", OPACITY, opacity);

    char fo_Buf[500];

    char* fontName = WideToAnsi(currentFont->lfFaceName);
    if(fontName)
    {
        sprintf_s(fo_Buf, "%s=%i,%i,%i,%i,%i,%u,%u,%u,%u,%u,%u,%u,%u,%s", FONT, currentFont->lfHeight, currentFont->lfWidth, currentFont->lfEscapement,
            currentFont->lfOrientation, currentFont->lfWeight, currentFont->lfItalic, currentFont->lfUnderline, currentFont->lfStrikeOut, currentFont->lfCharSet,
            currentFont->lfOutPrecision, currentFont->lfClipPrecision, currentFont->lfQuality, currentFont->lfPitchAndFamily, fontName);
    }

    char pathBuf[MAX_PATH];
    GetFullConfigPath(pathBuf);
    std::ofstream configFile(pathBuf);

    // Write to the file
    configFile << fg_Buf << "\n" << ch_Buf << "\n" << lp_Buf << "\n" << op_Buf << "\n" << fo_Buf << "\n";

    configFile.close();

    delete fg_RGB, ch_RGB;
}

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

void ConfigManager::ReadData()
{
    char pathBuf[MAX_PATH];
    GetFullConfigPath(pathBuf);
    std::ifstream configFile(pathBuf);
    std::string output;

    bool readFgCol = false;
    bool readChCol = false;
    bool readLastPos = false;
    bool readOpacity = false;
    bool readFont = false;

    bool invalidCfg = false;
  
    while(getline(configFile, output))
    {
        char* dataStart = strchr((char*)output.c_str(), '=');

        if(!strncmp(output.c_str(),FOREGROUND_COLOUR, 14))
        {
            COLORREF val = ProcessRGB(dataStart);
            int r = GetRValue(val);

            if(r == -1)
            {
                invalidCfg = true;
                break;
            }

            *foregroundColour = val;
            readFgCol = true;
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
            readChCol = true;
        }
        else if (!strncmp(output.c_str(), LAST_POS, 8))
        {
            std::tuple<int,int> coords = ProcessCoords(dataStart);

            if(std::get<0>(coords) == -1)
            {
                invalidCfg = true;
                break;
            }

            lastX = std::get<0>(coords);
            lastY = std::get<1>(coords);
            readLastPos = true;
        }
        else if (!strncmp(output.c_str(), OPACITY, 7))
        {
            int val = ProcessInt(dataStart);
            if(val == -1)
            {
                invalidCfg = true;
                break;
            }
            opacity = val;
            readOpacity = true;
        }
        else if (!strncmp(output.c_str(), FONT, 4))
        {
            LOGFONT font = ProcessFont(dataStart);
            currentFont = (LOGFONT*)malloc(sizeof(LOGFONT));
            if(currentFont)
            {
                memcpy(currentFont, &font, sizeof(LOGFONT));
            }

            readFont = true;
        }
    }

    configFile.close();

 
    if(!readFgCol || !readChCol || !readLastPos || !readOpacity || !readFont || invalidCfg)
    {
        if (retriedOnce)
        {
            //We've tried to read and write to a cfg file, but we've still failed.
            //Passed in path is most likely not valid
            failedToInit = true;
            return;
        }

        retriedOnce = true;
        InitDefaults();
        ReadData();
    }
}

int ConfigManager::ProcessInt(char* dataStart)
{
    ++dataStart;
    char* dataEnd = &dataStart[strlen(dataStart)];

    if(!dataEnd)
    {
        return -1;
    }

    char buf[20];
    CopyRange(dataStart, dataEnd, buf, 20);

    return atoi(buf);
}

LOGFONT ConfigManager::ProcessFont(char* dataStart)
{
    ++dataStart;

    char* prevGood = dataStart;
    char* nxt = strchr(dataStart, ',');

    int pos = 0;
    LOGFONT ret = { 0 };

    if(!nxt)
    {
        return ret;
    }

    bool atEnd = false;

    while(nxt)
    {
        char buf[200];
        CopyRange(prevGood, nxt, buf, 200);


        switch((FONT_ENUM)pos)
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

std::tuple<int, int> ConfigManager::ProcessCoords(char* dataStart)
{
    ++dataStart;
    char* prevGood = dataStart;
    char* nxt = strchr(dataStart, ',');
    if(!nxt)
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
        return RGB(-1,-1,-1);
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

        if(colIndex == 0)
        {
            writeDest = &redBuf[0];
        }
        else if(colIndex == 1)
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
        if(!nxt && !atEnd)
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
    char pathBuf[MAX_PATH];
    GetFullConfigPath(pathBuf);

    std::ofstream newConfigFile(pathBuf);

    newConfigFile << defaultConfig;

    newConfigFile.close();
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