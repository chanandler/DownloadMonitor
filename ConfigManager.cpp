#include "ConfigManager.h"
#include <iostream>
#include <fstream> 
#include <string>

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

const char* defaultConfig = "FOREGROUND_COLOUR=245,242,109\nCHILD_COLOUR=252,248,200\nLAST_POS=1200,0";

ConfigManager::ConfigManager()
{
    ReadData();
}

ConfigManager::~ConfigManager()
{

}

void ConfigManager::UpdateForegroundColour(COLORREF fg_col)
{
    foregroundColour = fg_col;
    WriteData();
}

void ConfigManager::UpdateChildColour(COLORREF ch_col)
{
    childColour = ch_col;
    WriteData();
}

void ConfigManager::UpdateWindowPos(int x, int y)
{
    lastX = x;
    lastY = y;
    WriteData();
}

void ConfigManager::WriteData()
{
    int fg_r = GetRValue(foregroundColour);
    int fg_g = GetGValue(foregroundColour);
    int fg_b = GetBValue(foregroundColour);

    IntRGB* fg_RGB = new IntRGB(fg_r, fg_g, fg_b);

    int ch_r = GetRValue(childColour);
    int ch_g = GetGValue(childColour);
    int ch_b = GetBValue(childColour);

    IntRGB* ch_RGB = new IntRGB(ch_r, ch_g, ch_b);

    char fg_Buf[200];
    sprintf_s(fg_Buf, "%s=%i,%i,%i", FOREGROUND_COLOUR, fg_RGB->r, fg_RGB->g, fg_RGB->b);

    char ch_Buf[200];
    sprintf_s(ch_Buf, "%s=%i,%i,%i", CHILD_COLOUR, ch_RGB->r, ch_RGB->g, ch_RGB->b);

    char lp_Buf[200];
    sprintf_s(lp_Buf, "%s=%i,%i", LAST_POS, lastX, lastY);

    std::ofstream configFile(CONFIG_NAME);

    // Write to the file
    configFile << fg_Buf << "\n" << ch_Buf << "\n" << lp_Buf << "\n";

    // Close the file
    configFile.close();

    delete fg_RGB, ch_RGB;
}

void ConfigManager::ReadData()
{
    std::ifstream configFile(CONFIG_NAME);
    std::string output;

    bool readData = false;
  
    while(getline(configFile, output))
    {
        if(!strncmp(output.c_str(),FOREGROUND_COLOUR, 14))
        {
            char* dataStart = strchr((char*)output.c_str(), '=');
            foregroundColour = ProcessRGB(dataStart);
            readData = true;
        }
        else if (!strncmp(output.c_str(), CHILD_COLOUR, 12))
        {
            char* dataStart = strchr((char*)output.c_str(), '=');
            childColour = ProcessRGB(dataStart);
            readData = true;
        }
        else if (!strncmp(output.c_str(), LAST_POS, 8))
        {
            char* dataStart = strchr((char*)output.c_str(), '=');
            std::tuple<int,int> coords = ProcessCoords(dataStart);
            lastX = std::get<0>(coords);
            lastY = std::get<1>(coords);
            readData = true;
        }
    }

    configFile.close();

    if(!readData)
    {
        InitDefaults();
        ReadData();
    }
}

std::tuple<int, int> ConfigManager::ProcessCoords(char* dataStart)
{
    ++dataStart;
    char* prevGood = dataStart;
    char* nxt = strchr(dataStart, ',');
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
        return RGB(0,0,0);
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

void ConfigManager::ResetColours()
{
    InitDefaults();
    ReadData();
}

void ConfigManager::InitDefaults()
{
    std::ofstream newConfigFile(CONFIG_NAME);

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

