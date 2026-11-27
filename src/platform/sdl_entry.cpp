// Platform entry point (WinMain on Windows, main elsewhere). SDL3 prepares the
// process and calls the game's SDL_main in main.cpp. Kept in its own file so the
// native probes can link every engine object and still define their own main.
#include <SDL3/SDL_main.h>
