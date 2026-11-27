// System: resolution/aspect-ratio types and platform helpers.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <list>
#include <algorithm>
#include <cctype>
#include <vector>
#include <SDL3_image/SDL_image.h>
#include "netlib/udpsocket.h"
#include "../fmtlib/printf.h"

#include "system.h"
#include "fsmgr.h"
#include "inivals.h"
#include "common/utils.h"


namespace System
{
static std::vector<std::string> CommandLine;


struct FontNode
{
    std::string Path;
    std::string Name;
    std::string Style;

    FontNode() {};
    FontNode(const std::string &p, const std::string &n, const std::string &s)
    : Path(p)
    , Name(n)
    , Style(s)
    {};

    FontNode(const FontNode &&fnt)
    {
        Path = std::move(fnt.Path);
        Name = std::move(fnt.Name);
        Style = std::move(fnt.Style);
    };
};

struct TFontLookup
{
    const std::string Name; // Font name or part
    const std::string Alt1;
    const std::string Alt2;
};


ResRatio::ResRatio()
{}

ResRatio::ResRatio(const Common::Point &p)
: W(p.x), H(p.y)
{
    R = (float)W / (float)H;
}

ResRatio::ResRatio(int32_t w, int32_t h)
: W(w), H(h)
{
    R = (float)W / (float)H;
}

bool ResRatio::operator==(const ResRatio &b) const
{
    if (W != b.W || H != b.H)
        return false;
    return true;
}

bool ResRatio::operator!=(const ResRatio &b) const
{
    if (W == b.W || H == b.H)
        return false;
    return true;
}

ResRatio::operator Common::Point() const
{
    return Common::Point(W, H);
}

void ResRatio::Set(int32_t w, int32_t h)
{
    W = w;
    H = h;
    R = (float)W / (float)H;
}


static std::list< SDL_EventFilter > EventHandlers;
static std::list< FontNode > FontsList;

static TFontLookup FontsLookup[]
{
    {"arial", "liberation sans", ""},
    {"dungeon", "xolonium bold", "xolonium regular"},
    {"ms sans serif", "microsoft sans serif", "liberation serif"},
    {"courier", "liberation mono", ""},
    {"small font", "press start 2p", ""},
    {"ＭＳ Ｐゴシック", "MS PGothic", "Textar"},
    {"MS PGothic", "Textar", ""}
};


static SDL_Window *window = nullptr;
static ResRatio winRes(640, 480);
static bool curRelMouse = false;

static std::string FontDisplayName(const FontNode &fnt)
{
    std::string name = StrTrim(fnt.Name);
    std::string style = StrTrim(fnt.Style);

    if (name.empty())
        return std::string();

    if (!style.empty())
        name += " " + style;

    return name;
}

static std::string NormalizeMenuFontName(std::string fontName)
{
    fontName = StrTrim(fontName);

    if (fontName.empty() || !StriCmp(fontName, "Default"))
        return std::string("Default");

    return fontName;
}

std::string MenuFontStorageValue(const std::string &fontName)
{
    std::string value = NormalizeMenuFontName(fontName);
    if (!StriCmp(value, "Default"))
        return std::string("Default");

    for (char &ch : value)
    {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = '_';
    }

    return value;
}

std::string MenuFontDisplayValue(const std::string &storedValue)
{
    std::string value = NormalizeMenuFontName(storedValue);
    if (!StriCmp(value, "Default"))
        return std::string("Default");

    // Backward compatibility: older test builds saved the display name directly
    // with spaces. If that value matches a scanned font, keep it.
    if (IsFontAvailable(value))
        return value;

    std::string decoded = value;
    for (char &ch : decoded)
    {
        if (ch == '_')
            ch = ' ';
    }

    decoded = NormalizeMenuFontName(decoded);
    if (!StriCmp(decoded, "Default") || !IsFontAvailable(decoded))
        return std::string("Default");

    return decoded;
}

static void ScanFontDir(const std::string &path)
{
    FSMgr::DirIter dir = FSMgr::iDir::readDir(path);
    if (dir)
    {
        FSMgr::iNode *nod = nullptr;
        while(dir.getNext(&nod))
        {
            if (nod && nod->getType() == FSMgr::iNode::NTYPE_FILE)
            {
                std::string tmp = nod->getName();
                for (size_t i = 0; i < tmp.length(); i++)
                    tmp[i] = std::tolower(tmp[i]);

                if( tmp.find(".ttf") != std::string::npos || tmp.find(".otf") != std::string::npos || tmp.find(".fon") != std::string::npos )
                {
                    TTF_Font *fnt = TTF_OpenFont(nod->getPath().c_str(), 12);
                    if (fnt)
                    {
                        const char *familyName = TTF_GetFontFamilyName(fnt);
                        const char *styleName = TTF_GetFontStyleName(fnt);

                        if (familyName && familyName[0])
                            FontsList.push_back( FontNode(nod->getPath(), familyName, styleName ? styleName : "") );

                        TTF_CloseFont(fnt);
                    }
                }
            }
        }
    }
}

void ScanFonts()
{
    FontsList.clear();

    for (const std::string &path : uaDataFirstRootDirCandidates("Fonts"))
        ScanFontDir(path);
}

std::string FindFont(const std::string &fontName)
{
    for(const FontNode &fnt : FontsList )
    {
        if (StriCmp(fontName, fnt.Name) == 0)
            return fnt.Path;
        else
        {
            std::string ttmp = fnt.Name;
            ttmp += " ";
            ttmp += fnt.Style;

            if (StriCmp(fontName, ttmp) == 0)
                return fnt.Path;
        }
    }

    return std::string();
}

void RescanFonts()
{
    ScanFonts();
}

std::vector<std::string> GetScannedFontNames()
{
    std::vector<std::string> names;
    std::vector<std::string> namesLower;

    for (const FontNode &fnt : FontsList)
    {
        std::string displayName = FontDisplayName(fnt);
        if (displayName.empty())
            continue;

        std::string lowerName = displayName;
        for (char &ch : lowerName)
            ch = std::tolower((unsigned char)ch);

        bool found = false;
        for (const std::string &existing : namesLower)
        {
            if (existing == lowerName)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            names.push_back(displayName);
            namesLower.push_back(lowerName);
        }
    }

    std::sort(names.begin(), names.end(),
        [](const std::string &a, const std::string &b) { return StriCmp(a, b) < 0; });

    return names;
}

bool IsFontAvailable(const std::string &fontname)
{
    std::string selected = StrTrim(fontname);
    if (selected.empty())
        return false;

    return !FindFont(selected).empty();
}

std::string GetConfiguredMenuFontName()
{
    // OpenNeoUA: ui.menu_font is stored in nucleus.ini as a single safe token,
    // e.g. Liberation_Mono_Regular. Decode it back to the display name used by
    // the dynamic Fonts/ scanner. This avoids whitespace/parser edge cases while
    // keeping the menu label human-readable.
    return MenuFontDisplayValue(IniConf::UiMenuFont.Get<std::string>());
}

std::string ResolveMenuFontDescr(const std::string &defaultDescr)
{
    return ResolveMenuFontDescr(defaultDescr, GetConfiguredMenuFontName());
}

std::string ResolveMenuFontDescr(const std::string &defaultDescr, const std::string &menuFont)
{
    std::string selected = NormalizeMenuFontName(menuFont);

    if (!StriCmp(selected, "Default"))
        return defaultDescr;

    if (!IsFontAvailable(selected))
        return defaultDescr;

    std::vector<std::string> parts = Stok::Split(defaultDescr, ",");
    if (parts.size() < 2)
        return defaultDescr;

    std::string resolved = selected;
    for (size_t i = 1; i < parts.size(); i++)
        resolved += "," + StrTrim(parts[i]);

    return resolved;
}


TTF_Font *LoadFont(const std::string &fontname, int height)
{
    std::string tmp2 = fontname;
    for (size_t i = 0; i < tmp2.length(); i++)
        tmp2[i] = std::tolower(tmp2[i]);

    TTF_Font *fnt = nullptr;

    std::string filename = FindFont(tmp2);

    if (filename.empty())
    {
        for( const TFontLookup &lookup : FontsLookup )
        {
            if ( !StriCmp(tmp2, lookup.Name) )
            {
                filename = FindFont(lookup.Alt1);

                if (!filename.empty())
                    break;

                filename = FindFont(lookup.Alt2);

                if (!filename.empty())
                    break;
            }
        }
    }

    if (filename.empty())
    {
        for( const TFontLookup &lookup : FontsLookup )
        {
            if ( tmp2.find(lookup.Name) != std::string::npos )
            {
                filename = FindFont(lookup.Alt1);

                if (!filename.empty())
                    break;

                filename = FindFont(lookup.Alt2);

                if (!filename.empty())
                    break;
            }
        }
    }

    if (!filename.empty())
        fnt = TTF_OpenFont(filename.c_str(), height);

    if (!fnt)
        fmt::printf("Can't load font %s %d\n", fontname, height);

    return fnt;
}



// Resizable game window centred on the primary display.
static SDL_Window *CreateGameWindow()
{
    // No API flag: the GPU device claims the window later (GPU::Init).
    SDL_Window *wnd = SDL_CreateWindow("OpenNeoUA (Urban Assault)", winRes.W, winRes.H,
                                       SDL_WINDOW_RESIZABLE);
    if (wnd)
        SDL_SetWindowPosition(wnd, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return wnd;
}

void Init()
{
    EventHandlers.clear();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC))
    {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return;
    }

    window = CreateGameWindow();

    if (!window)
    {
        printf("Couldn't create window and renderer: %s", SDL_GetError());
        return;
    }

    // SDL3 sends text events only after SDL_StartTextInput (SDL2 had them on by
    // default). No on-screen keyboard: names and chat are typed on real keys.
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    SDL_StartTextInput(window);

    TTF_Init();

    ScanFonts();

    Net::Init();
}

void Deinit()
{
    FontsList.clear();

    if (window)
        SDL_DestroyWindow(window);
    window = nullptr;

    TTF_Quit();
    SDL_Quit();
}


bool ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            winRes = ResRatio(w, h);
        }
        break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_QUIT:
            return true;

        default:
            break;
        }

        // A handler returning false consumes the event.
        for(SDL_EventFilter &e : EventHandlers)
        {
            if ( !(e)(nullptr, &event) )
                break;
        }
    }

    return false;
}

Common::Point GetResolution()
{
    return Common::Point(winRes.W, winRes.H);
}

int GetDisplayRefreshRate()
{
    if ( !window )
        return 0;

    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    return mode ? (int)(mode->refresh_rate + 0.5f) : 0;
}

bool IsFullscreenWindow()
{
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

SDL_Window *GetWindow()
{
    return window;
}

void SetVideoMode(const Common::Point &sz, bool full)
{
    if (window)
    {
        // Fullscreen is borderless at desktop resolution (SDL2 FULLSCREEN_DESKTOP):
        // the game keeps its own resolution and scales it to the screen.
        if (full)
            SDL_SetWindowFullscreenMode(window, nullptr);
        SDL_SetWindowFullscreen(window, full);
        SDL_SyncWindow(window);

        if (!full)
        {
            if (winRes != sz)
            {
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)
                {
                    SDL_RestoreWindow(window);
                    SDL_Delay(250);

                    SDL_SetWindowSize(window, sz.x, sz.y);

                    SDL_MaximizeWindow(window);
                    SDL_Delay(250);
                }
                else
                    SDL_SetWindowSize(window, sz.x, sz.y);
            }
        }

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        winRes = ResRatio(w, h);
    }
}

void SetReleativeMouse(bool mode)
{
    if (mode != curRelMouse)
    {
        SDL_SetWindowRelativeMouseMode(window, mode);
        if (!mode)
        {
            int WW = 0, WH = 0;
            SDL_GetWindowSize(window, &WW, &WH);
            SDL_WarpMouseInWindow(window, WW / 2.0f, WH / 2.0f);
        }

        curRelMouse = mode;
    }
}

void EventsAddHandler(SDL_EventFilter func, bool first)
{
    if (first)
        EventHandlers.push_front(func);
    else
        EventHandlers.push_back(func);
}

void EventsDeleteHandler(SDL_EventFilter func)
{
    EventHandlers.remove(func);
}

void PostQuitMessage()
{
    SDL_Event event;
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

std::vector<std::string>& GetCmdLineArray()
{
    return CommandLine;
}

void AddCmdLine(const std::string &arg)
{
    CommandLine.push_back(arg);
}

int32_t FindCmdLineArg(const std::string &arg)
{
    for (size_t i = 0; i < CommandLine.size(); i++)
    {
        if (StriCmp(CommandLine[i], arg) == 0)
            return i;
    }
    return -1;
}


}
