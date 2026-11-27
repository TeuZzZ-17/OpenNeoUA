// System: resolution/aspect-ratio types and platform helpers.
#ifndef UA_SYSTEM_H_INCLUDED
#define UA_SYSTEM_H_INCLUDED

#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "common/common.h"

namespace System
{
    struct ResRatio
    {
        int32_t W = 0;
        int32_t H = 0;
        float   R = 1.0;

        ResRatio();
        ResRatio(const Common::Point &p);
        ResRatio(int32_t w, int32_t h);
        bool operator==(const ResRatio &b) const;
        bool operator!=(const ResRatio &b) const;
        void Set(int32_t w, int32_t h);

        operator Common::Point() const;
    };

    void Init();
    void Deinit();

    // Events update cycle
    bool ProcessEvents();

    // The game window (the GPU device draws into it, see gpu.h).
    SDL_Window *GetWindow();

    void SetVideoMode(const Common::Point &sz, bool full);

    void SetReleativeMouse(bool mode);

    TTF_Font *LoadFont(const std::string &fontname, int height);
    void RescanFonts();
    std::vector<std::string> GetScannedFontNames();
    bool IsFontAvailable(const std::string &fontname);
    std::string MenuFontStorageValue(const std::string &fontName);
    std::string MenuFontDisplayValue(const std::string &storedValue);
    std::string GetConfiguredMenuFontName();
    std::string ResolveMenuFontDescr(const std::string &defaultDescr);
    std::string ResolveMenuFontDescr(const std::string &defaultDescr, const std::string &menuFont);

    void PostQuitMessage();

    void EventsAddHandler(SDL_EventFilter func, bool first = true);
    void EventsDeleteHandler(SDL_EventFilter func);

    Common::Point GetResolution();

    // Refresh rate (Hz) of the display showing the game window; 0 if unknown.
    int GetDisplayRefreshRate();
    // True when the game window covers the whole display.
    bool IsFullscreenWindow();

    std::vector<std::string>& GetCmdLineArray();
    void AddCmdLine(const std::string &);
    int32_t FindCmdLineArg(const std::string &);

}
#endif
