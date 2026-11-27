// NC_STACK_image: bitmap created from an image file.
#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>
#include "nucleus/nucleas.h"
#include "nucleus/rsrc.h"
#include "nucleus/image.h"
#include "common/utils.h"
#include "common/includes.h"


NC_STACK_image::NC_STACK_image()
{
}

NC_STACK_image::~NC_STACK_image()
{
}

size_t NC_STACK_image::Init(IDVList &stak)
{
    if ( !NC_STACK_bitmap::Init(stak) )
        return 0;

    return 1;
}



// SDL_IOStream over an engine file handle (Data paths, archives and overrides).
static Sint64 FileIOSize(void *userdata)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    const size_t curr = fil->tell();
    fil->seek(0, SEEK_END);
    const size_t sz = fil->tell();
    fil->seek(curr, SEEK_SET);
    return (Sint64)sz;
}

static Sint64 FileIOSeek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    const int origin = whence == SDL_IO_SEEK_CUR ? SEEK_CUR : whence == SDL_IO_SEEK_END ? SEEK_END : SEEK_SET;
    if ( fil->seek(offset, origin) != 0 )
        return -1;
    return (Sint64)fil->tell();
}

static size_t FileIORead(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    const size_t got = size ? fil->read(ptr, size) : 0;
    if ( got < size )
        *status = fil->eof() ? SDL_IO_STATUS_EOF : SDL_IO_STATUS_ERROR;
    return got;
}

static size_t FileIOWrite(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    const size_t put = size ? fil->write(ptr, size) : 0;
    if ( put < size )
        *status = SDL_IO_STATUS_ERROR;
    return put;
}

static bool FileIOClose(void *userdata)
{
    delete (FSMgr::FileHandle *)userdata;
    return true;
}

SDL_IOStream *NC_STACK_image::OpenRWops(FSMgr::FileHandle *fil)
{
    SDL_IOStreamInterface iface;
    SDL_INIT_INTERFACE(&iface);
    iface.size = FileIOSize;
    iface.seek = FileIOSeek;
    iface.read = FileIORead;
    iface.write = FileIOWrite;
    iface.close = FileIOClose;
    return SDL_OpenIO(&iface, fil);
}

static bool IsSvgResourceName(const std::string &resName)
{
    const size_t extPos = resName.find_last_of('.');
    if ( extPos == std::string::npos || extPos + 1 >= resName.size() )
        return false;

    return !StriCmp(resName.substr(extPos + 1), "svg");
}



rsrc * NC_STACK_image::rsrc_func64(IDVList &stak)
{
    const std::string resName = stak.Get<std::string>(RSRC_ATT_NAME, "");
    int convertColor = stak.Get<int32_t>(BMD_ATT_CONVCOLOR, 0);

    if ( resName.empty() )
        return nullptr;

    std::string tmpBuf = "rsrc:";
    tmpBuf += resName;

    FSMgr::FileHandle *fil = uaOpenFileAlloc(tmpBuf, "rb");
    if (!fil)
        return nullptr;

    SDL_IOStream *rwops = OpenRWops(fil);
    if (!rwops)
    {
        delete fil;
        return nullptr;
    }

    SDL_Surface *loaded = nullptr;

    const int requestedWidth = stak.Get<int32_t>(BMD_ATT_WIDTH, 0);
    const int requestedHeight = stak.Get<int32_t>(BMD_ATT_HEIGHT, 0);
    const bool hasSizedSvgRequest = IsSvgResourceName(resName) &&
                                    requestedWidth >= 0 && requestedHeight >= 0 &&
                                    (requestedWidth > 0 || requestedHeight > 0);

    if ( hasSizedSvgRequest )
    {
        loaded = IMG_LoadSizedSVG_IO(rwops, requestedWidth, requestedHeight);
        if ( loaded )
        {
            SDL_CloseIO(rwops);
            rwops = nullptr;
        }
        else if ( SDL_SeekIO(rwops, 0, SDL_IO_SEEK_SET) < 0 )
        {
            // The custom stream is normally seekable. If a backend refuses
            // the rewind, reopen it so the legacy loader still gets a full
            // fallback attempt instead of turning a sized SVG failure into a
            // new hard failure.
            SDL_CloseIO(rwops);
            rwops = nullptr;

            fil = uaOpenFileAlloc(tmpBuf, "rb");
            if ( fil )
            {
                rwops = OpenRWops(fil);
                if ( !rwops )
                    delete fil;
            }
        }
    }

    // Keep the existing loader as the universal fallback.
    if ( !loaded && rwops )
        loaded = IMG_Load_IO(rwops, true);

    if (!loaded)
    {
        printf("%s\n", SDL_GetError());
        return nullptr;
    }

    rsrc *res = NC_STACK_rsrc::rsrc_func64(stak); // bitmap_func64

    if ( !res )
    {
        SDL_DestroySurface(loaded);
        return nullptr;
    }

    ResBitmap *bitm = new ResBitmap;
    if ( !bitm )
    {
        SDL_DestroySurface(loaded);
        rsrc_func65(res);
        return nullptr;
    }

    res->data = bitm;

    bitm->width = loaded->w;
    bitm->height = loaded->h;
    bitm->swTex = loaded;

    if (convertColor)
    {
        SDL_Surface *screenFmt = GFX::Engine.ConvertToScreenFormat(loaded);
        if (screenFmt)
        {
            SDL_DestroySurface(bitm->swTex);
            bitm->swTex = screenFmt;
        }
    }
    else if (SDL_GetSurfacePalette(loaded))
    {
        if ( !bitm->palette )
            bitm->palette = new UA_PALETTE;

        if ( bitm->palette )
        {
            for (int i = 0; i < SDL_GetSurfacePalette(loaded)->ncolors; i++)
                (*bitm->palette)[i] = SDL_GetSurfacePalette(loaded)->colors[i];
        }
    }

    return res;
}

bool NC_STACK_image::SavePng(ResBitmap *bitm, const std::string &fname)
{
    if (!bitm)
        return false;

    if (!bitm->swTex)
        return false;

    FSMgr::FileHandle *fil = uaOpenFileAlloc(fname, "wb");
    if (!fil || !fil->OK())
        return false;

    SDL_IOStream *rwops = OpenRWops(fil);
    if (!rwops)
        return false;

    return IMG_SavePNG_IO(bitm->swTex, rwops, true);
}