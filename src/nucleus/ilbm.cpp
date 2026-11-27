// ILBM/VBMP image loader (NC_STACK_ilbm).
#include <SDL3_image/SDL_image.h>
#include "common/includes.h"
#include "nucleus/nucleas.h"
#include "nucleus/rsrc.h"
#include "nucleus/ilbm.h"
#include "nucleus/image.h"
#include "common/utils.h"


size_t NC_STACK_ilbm::Init(IDVList &stak)
{
    if ( !NC_STACK_bitmap::Init(stak) )
        return 0;

    if ( stak.Get<bool>(ILBM_ATT_FMT, false) )
        _saveAsIlbm = true;

    return 1;
}


size_t NC_STACK_ilbm::ilbm_func5__sub0(NC_STACK_ilbm *obj, IFFile **pmfile)
{
    IFFile *mfile = *pmfile;

    int has_nam2 = 0;
    int has_opl = 0;

    char name[256];
    std::vector<tUtV> opls;

    while ( 1 )
    {
        int v6 = mfile->parse();

        if ( v6 == -2 )
            break;

        if ( v6 )
            return 0;

        const IFFile::Context &iff_chunk = mfile->GetCurrentChunk();

        if ( iff_chunk.Is(TAG_NAM2) )
        {
            mfile->read(name, 256);
            mfile->parse();

            has_nam2 = 1;
        }
        else if ( iff_chunk.Is(TAG_OTL2) )
        {
            uint8_t dst[128];

            mfile->read(&dst, 128);

            int opl_count = iff_chunk.TAG_SIZE / 2;
            opls.resize(opl_count);

            for (int i = 0; i < opl_count; ++i)
            {
                opls[i].tu = float(dst[0 + 2 * i]) / 256.0;
                opls[i].tv = float(dst[1 + 2 * i]) / 256.0;
            }

            mfile->parse();

            has_opl = 1;
        }
        else
        {
            mfile->skipChunk();
        }
    }

    if ( has_nam2 )
    {
        IDVList stk {
            {RSRC_ATT_NAME, std::string(name)},
            {RSRC_ATT_TRYSHARED, (int32_t)1},
            {BMD_ATT_CONVCOLOR, (int32_t)1}};

        if ( has_opl )
            stk.Add(BMD_ATT_OUTLINE, &opls);

        return NC_STACK_bitmap::Init(stk);
    }
    return 0;
}

size_t NC_STACK_ilbm::LoadingFromIFF(IFFile **file)
{
    if ( (*file)->GetCurrentChunk().TAG_EXTENSION == TAG_CIBO )
        return ilbm_func5__sub0(this, file);

    return 0;
}

size_t NC_STACK_ilbm::SavingIntoIFF(IFFile **pmfile)
{
    IFFile *mfile = *pmfile;

    std::string name = getRsrc_name();

    SetTime(1, 1);
    const std::vector<tUtV> &opl2 = GetOutline();

    if ( mfile->pushChunk(TAG_CIBO, TAG_FORM, -1) )
    {
        return 0;
    }
    else
    {

        mfile->pushChunk(0, TAG_NAM2, -1);
        mfile->write(name.c_str(), name.length() + 1);
        mfile->popChunk();

        if ( !opl2.empty() )
        {
            mfile->pushChunk(0, TAG_OTL2, opl2.size() * 2);

            for(tUtV uv : opl2)
            {
                mfile->writeU8(uv.tu * 256.0);
                mfile->writeU8(uv.tv * 256.0);
            }

            mfile->popChunk();
        }
        return mfile->popChunk() == IFFile::IFF_ERR_OK;
    }
    return 0;
}

void ILBM_BODY_READ__sub0(BMHD_type *bmhd, const std::vector<int8_t> &ilbm_data, ResBitmap *img)
{
    uint8_t masks[8] = {1, 2, 4, 8, 16, 32, 64, 128};

    SDL_LockSurface(img->swTex);
    uint8_t udp[1024];

    int pln_w = (bmhd->width + 7) / 8;
    pln_w += (pln_w & 1);

    int inByte = 0;

    for (int y = 0; y < bmhd->height; y++)
    {
        uint8_t *img_buffer = (uint8_t *)img->swTex->pixels + y * img->swTex->pitch;
        if ( bmhd->compression )
        {
            for (int plan = 0; plan < bmhd->nPlanes; plan++)
            {
                int planOff = plan << 7;

                int x = 0;

                while (x < pln_w)
                {
                    int n = ilbm_data[inByte];
                    inByte++;

                    if (n == -128)
                    {
                        ;
                    }
                    else if ( n < 0)
                    {
                        n = -n + 1;
                        int dat = ilbm_data[inByte];
                        inByte++;
                        for (int i = 0; i < n; i++)
                        {
                            udp[planOff + x] = dat;
                            x++;
                        }
                    }
                    else if (n >= 0)
                    {
                        int cnt = n + 1;
                        memcpy(udp + planOff + x, ilbm_data.data() + inByte, cnt);
                        inByte += cnt;
                        x += cnt;
                    }
                }
            }
        }
        else
        {
            for (int plan = 0; plan < bmhd->nPlanes; plan++)
            {
                int planOff = plan << 7;
                memcpy(udp + planOff, ilbm_data.data() + inByte, pln_w);
                inByte += pln_w;
            }
        }
        for (int x = 0; x < bmhd->width; x++)
        {
            int v16 = x / 8;

            uint8_t bt = 0;

            for (int plan = 0; plan < bmhd->nPlanes; plan++)
            {
                int planOff = plan << 7;

                if ( masks[7 - (x & 7)] & udp[planOff + v16] )
                    bt |= masks[plan];
            }
            *img_buffer = bt;
            img_buffer++;
        }
    }
    SDL_UnlockSurface(img->swTex);
}

int ILBM_BODY_READ(IFFile *mfile, BMHD_type *bmhd, ResBitmap *bitm)
{
    if ( !bitm->swTex )
        return false;

    const IFFile::Context &chunk = mfile->GetCurrentChunk();

    std::vector<int8_t> buffer;
    buffer.resize(chunk.TAG_SIZE);

    mfile->read(buffer.data(), chunk.TAG_SIZE);

    ILBM_BODY_READ__sub0(bmhd, buffer, bitm);

    return true;
}

rsrc * NC_STACK_ilbm::READ_ILBM(IDVList &stak, IFFile *mfil, int transp)
{
    int ILBM__OR__VBMP;

    BMHD_type bmhd;
    VBMP_type vbmp;

    int convertColor = stak.Get<int32_t>(NC_STACK_bitmap::BMD_ATT_CONVCOLOR, 0);
    int alphaPalette = stak.Get<int32_t>(ATT_ALPHAPALETTE, 1);

    ResBitmap *bitm = nullptr;

    if ( mfil->parse() )
    {
        ypa_log_out("ilbm.class: Not an IFF file!\n");
        return nullptr;
    }

    const IFFile::Context &formChunk = mfil->GetCurrentChunk();
    if ( !formChunk.Is(TAG_FORM) )
    {
        ypa_log_out("ilbm.class: Not an IFF FORM chunk!\n");
        return nullptr;
    }

    if ( formChunk.TAG_EXTENSION == TAG_ILBM )
        ILBM__OR__VBMP = 1;
    else if ( formChunk.TAG_EXTENSION == TAG_VBMP )
        ILBM__OR__VBMP = 0;
    else
    {
        ypa_log_out("ilbm.class: Not an ILBM or VBMP file!\n");
        return nullptr;
    }

    rsrc *res = NC_STACK_rsrc::rsrc_func64(stak);
    if (!res)
        return nullptr;

    while ( true )
    {
        int v10 = mfil->parse();
        if ( v10 == IFFile::IFF_ERR_EOC )
            break;
        if ( v10 )
        {
            if ( res )
                rsrc_func65(res);

            return nullptr;
        }

        const IFFile::Context &chunk = mfil->GetCurrentChunk();

        if ( chunk.Is(TAG_BMHD) )
        {
            bmhd.width = mfil->readU16B();
            bmhd.height = mfil->readU16B();
            bmhd.x = mfil->readU16B();
            bmhd.y = mfil->readU16B();
            bmhd.nPlanes = mfil->readS8();
            bmhd.masking = mfil->readS8();
            bmhd.compression = mfil->readS8();
            bmhd.flags = mfil->readS8();
            bmhd.transparentColor = mfil->readU16B();
            bmhd.xAspect = mfil->readS8();
            bmhd.yAspect = mfil->readS8();
            bmhd.pageWidth = mfil->readU16B();
            bmhd.pageHeight = mfil->readU16B();

            bitm = new ResBitmap;
            bitm->width = bmhd.width;
            bitm->height = bmhd.height;

            res->data = bitm;

            mfil->parse();
        }
        else if ( chunk.Is(TAG_HEAD) )
        {
            vbmp.width = mfil->readU16B();
            vbmp.height = mfil->readU16B();
            vbmp.flags = mfil->readU16B();

            bitm = new ResBitmap;
            bitm->width = vbmp.width;
            bitm->height = vbmp.height;

            res->data = bitm;

            mfil->parse();
        }
        else if ( chunk.Is(TAG_CMAP) )
        {
            if ( bitm )
            {
                if ( !bitm->palette )
                    bitm->palette = new UA_PALETTE;

                if ( bitm->palette )
                {
                    for (int i = 0; i < 256; i++)
                    {
                        bitm->palette->at(i).r = mfil->readU8();
                        bitm->palette->at(i).g = mfil->readU8();
                        bitm->palette->at(i).b = mfil->readU8();
                        bitm->palette->at(i).a = 255;
                    }
                }
            }
            mfil->parse();
        }
        else if ( chunk.Is(TAG_BODY) )
        {
            bool success = false;

            if ( bitm )
            {
                bitm->swTex = Common::CreateIndexedSurface(bitm->width, bitm->height);

                if ( ILBM__OR__VBMP )
                {
                    success = ILBM_BODY_READ(mfil, &bmhd, bitm);
                }
                else if ( bitm->swTex )              // VBMP READ
                {
                    SDL_LockSurface(bitm->swTex);
                    for(int y = 0; y < bitm->height; y++)
                        mfil->read((uint8_t *)bitm->swTex->pixels + y * bitm->swTex->pitch, bitm->width);
                    SDL_UnlockSurface(bitm->swTex);
                    success = true;
                }
                else
                {
                    success = false;
                }
            }
            if ( !success )
            {
                rsrc_func65(res);
                return nullptr;
            }
            mfil->parse();
        }
        else
        {
            mfil->skipChunk();
        }
    }

    if (bitm && bitm->swTex)
    {
        if (SDL_GetSurfacePalette(bitm->swTex))
        {
            UA_PALETTE *pal = bitm->palette;
            if (!pal)
                pal = GFX::Engine.GetPalette();

            if (alphaPalette)
            {
                UA_PALETTE tmp;
                GFX::Engine.ConvAlphaPalette(&tmp, *pal, transp);
                SDL_SetPaletteColors(SDL_GetSurfacePalette(bitm->swTex), tmp.data(), 0, 256);
            }
            else
                SDL_SetPaletteColors(SDL_GetSurfacePalette(bitm->swTex), pal->data(), 0, 256);
        }

        if ( convertColor )
        {
            SDL_Surface *screenFmt = GFX::Engine.ConvertToScreenFormat(bitm->swTex);
            if (screenFmt)
            {
                SDL_DestroySurface(bitm->swTex);
                bitm->swTex = screenFmt;

                if (bitm->palette)
                {
                    delete bitm->palette;
                    bitm->palette = nullptr;
                }
            }
        }
    }
    return res;
}

static rsrc *ILBM_ReadOpenedOrVanilla(NC_STACK_ilbm *obj, IDVList &stak, const std::string &name, int transp)
{
    IFFile *mfile = IFFile::RsrcOpenIFFile(name, "rb", "NC_STACK_ilbm::rsrc_func64");
    if ( !mfile )
        return nullptr;

    rsrc *res = obj->READ_ILBM(stak, mfile, transp);
    if ( res )
    {
        mfile->ReportSetLooseOverrideUsed();
        delete mfile;
        return res;
    }

    if ( mfile->IsSetLooseOverride() )
    {
        mfile->ReportSetLooseOverrideFailed("loose file existed but failed to load; vanilla fallback used.");
        delete mfile;

        mfile = IFFile::RsrcOpenIFFileVanilla(name, "rb");
        if ( !mfile )
            return nullptr;

        res = obj->READ_ILBM(stak, mfile, transp);
        delete mfile;
        return res;
    }

    delete mfile;
    return nullptr;
}

static void ILBM_SkipEmbeddedChunk(IFFile *mfile);

static UA_PALETTE *ILBM_CopyPngPalette(SDL_Surface *loaded)
{
    UA_PALETTE *pal = new UA_PALETTE;
    if ( !pal )
        return nullptr;

    UA_PALETTE *fallback = GFX::Engine.GetPalette();
    if ( fallback )
        *pal = *fallback;
    else
    {
        for (SDL_Color &clr : *pal)
            clr = {0, 0, 0, 255};
    }

    if ( SDL_GetSurfacePalette(loaded) )
    {
        int colors = std::min(SDL_GetSurfacePalette(loaded)->ncolors, 256);
        for (int i = 0; i < colors; i++)
        {
            pal->at(i) = SDL_GetSurfacePalette(loaded)->colors[i];
            pal->at(i).a = 255;
        }
    }

    return pal;
}

static uint8_t ILBM_ClampByte(int value)
{
    if ( value < 0 )
        return 0;
    if ( value > 255 )
        return 255;
    return (uint8_t)value;
}

static uint8_t ILBM_MaxRgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint8_t)((r >= g) ? (r > b ? r : b) : (g > b ? g : b));
}

static SDL_Surface *ILBM_PrepareTruecolorPngSurface(SDL_Surface *loaded, int transp)
{
    if ( !loaded || SDL_GetSurfacePalette(loaded) || !transp )
        return loaded;

    SDL_Surface *rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888);
    if ( !rgba )
        return loaded;

    SDL_LockSurface(rgba);
    for (int y = 0; y < rgba->h; y++)
    {
        uint8_t *row = (uint8_t *)rgba->pixels + y * rgba->pitch;
        for (int x = 0; x < rgba->w; x++)
        {
            uint32_t *pixel = (uint32_t *)(row + x * SDL_BYTESPERPIXEL(rgba->format));

            uint8_t r, g, b, a;
            SDL_GetRGBA(*pixel, SDL_GetPixelFormatDetails(rgba->format), SDL_GetSurfacePalette(rgba), &r, &g, &b, &a);

            const bool chromaTransparent = (r == 255 && g == 255 && b == 0);
            if ( chromaTransparent || a == 0 )
            {
                r = 0;
                g = 0;
                b = 0;
                a = 0;
            }
            else if ( !GFX::Engine.can_destblend && GFX::Engine.can_srcblend )
            {
                // Match GFXEngine::ConvAlphaPalette() for legacy HI/gamma ILBM:
                // source-alpha mode derives alpha from pixel brightness and
                // normalizes RGB. This keeps PNG FX visually equivalent to ILBM FX.
                int mx = ILBM_MaxRgb(r, g, b);
                if ( mx <= 8 )
                {
                    r = 0;
                    g = 0;
                    b = 0;
                    a = 0;
                }
                else
                {
                    r = ILBM_ClampByte((int)(255.0f * ((float)r / (float)mx)));
                    g = ILBM_ClampByte((int)(255.0f * ((float)g / (float)mx)));
                    b = ILBM_ClampByte((int)(255.0f * ((float)b / (float)mx)));
                    a = ILBM_ClampByte(((int)a * mx + 127) / 255);
                }
            }
            else
            {
                // Match legacy HI alpha/beta ILBM behavior. In additive/stipple
                // modes dark pixels are part of the effect texture: do not erase
                // them or blue/black energy barriers disappear. Only transparent
                // index/chroma pixels are removed above.
                a = 255;
            }

            *pixel = SDL_MapSurfaceRGBA(rgba, r, g, b, a);
        }
    }
    SDL_UnlockSurface(rgba);

    SDL_DestroySurface(loaded);
    return rgba;
}

void ILBM_NormalizeLegacyColorKeyPngSurface(SDL_Surface *s)
{
    if ( !s || !s->format )
        return;

    if ( SDL_BITSPERPIXEL(s->format) != 32 || !SDL_GetPixelFormatDetails(s->format)->Amask )
    {
        SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
        return;
    }

    bool locked = false;
    if ( SDL_MUSTLOCK(s) )
    {
        if ( !SDL_LockSurface(s) )
        {
            SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
            return;
        }
        locked = true;
    }

    const uint32_t colorKey = SDL_MapSurfaceRGB(s, 255, 255, 0);

    for (int y = 0; y < s->h; y++)
    {
        uint8_t *row = (uint8_t *)s->pixels + y * s->pitch;
        uint32_t *pixel = (uint32_t *)row;

        for (int x = 0; x < s->w; x++)
        {
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel[x], SDL_GetPixelFormatDetails(s->format), SDL_GetSurfacePalette(s), &r, &g, &b, &a);

            if ( a < 128 )
                pixel[x] = colorKey;
            else
                pixel[x] = SDL_MapSurfaceRGBA(s, r, g, b, 255);
        }
    }

    if ( locked )
        SDL_UnlockSurface(s);

    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
}

static SDL_Surface *ILBM_NormalizeIndexedPngSurface(SDL_Surface *loaded, UA_PALETTE *pal, int alphaPalette, int transp)
{
    if ( !loaded || !SDL_GetSurfacePalette(loaded) )
        return loaded;

    SDL_Surface *indexed = Common::CreateIndexedSurface(loaded->w, loaded->h);
    if ( !indexed )
        return nullptr;

    SDL_LockSurface(loaded);
    SDL_LockSurface(indexed);
    for (int y = 0; y < loaded->h; y++)
    {
        uint8_t *src = (uint8_t *)loaded->pixels + y * loaded->pitch;
        uint8_t *dst = (uint8_t *)indexed->pixels + y * indexed->pitch;
        memcpy(dst, src, loaded->w);
    }
    SDL_UnlockSurface(indexed);
    SDL_UnlockSurface(loaded);

    if ( SDL_GetSurfacePalette(indexed) && pal )
    {
        if ( alphaPalette )
        {
            UA_PALETTE tmp;
            GFX::Engine.ConvAlphaPalette(&tmp, *pal, transp);
            SDL_SetPaletteColors(SDL_GetSurfacePalette(indexed), tmp.data(), 0, 256);
        }
        else
        {
            SDL_SetPaletteColors(SDL_GetSurfacePalette(indexed), pal->data(), 0, 256);
        }
    }

    SDL_DestroySurface(loaded);
    return indexed;
}

static rsrc *ILBM_ReadPngOverride(NC_STACK_ilbm *obj, IDVList &stak, const std::string &path, int transp, bool legacyColorKey = false)
{
    FSMgr::FileHandle *fil = FSMgr::iDir::openFileAlloc(path, "rb");
    if ( !fil )
        fil = new FSMgr::FileHandle(path, "rb");

    if ( !fil || !fil->OK() )
    {
        delete fil;
        return nullptr;
    }

    SDL_IOStream *rwops = NC_STACK_image::OpenRWops(fil);
    if ( !rwops )
    {
        delete fil;
        return nullptr;
    }

    SDL_Surface *loaded = IMG_Load_IO(rwops, true);
    if ( !loaded )
        return nullptr;

    if ( loaded->w <= 0 || loaded->h <= 0 )
    {
        SDL_DestroySurface(loaded);
        return nullptr;
    }

    if ( legacyColorKey && SDL_GetSurfacePalette(loaded) )
    {
        SDL_Surface *rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888);
        if ( rgba )
        {
            SDL_DestroySurface(loaded);
            loaded = rgba;
        }
    }

    rsrc *res = obj->NC_STACK_rsrc::rsrc_func64(stak);
    if ( !res )
    {
        SDL_DestroySurface(loaded);
        return nullptr;
    }

    ResBitmap *bitm = new ResBitmap;
    if ( !bitm )
    {
        SDL_DestroySurface(loaded);
        obj->NC_STACK_rsrc::rsrc_func65(res);
        return nullptr;
    }

    bitm->width = loaded->w;
    bitm->height = loaded->h;
    res->data = bitm;

    if ( SDL_GetSurfacePalette(loaded) )
    {
        bitm->palette = ILBM_CopyPngPalette(loaded);
        int alphaPalette = stak.Get<int32_t>(NC_STACK_ilbm::ATT_ALPHAPALETTE, 1);
        bitm->swTex = ILBM_NormalizeIndexedPngSurface(loaded, bitm->palette, alphaPalette, transp);
        if ( !bitm->swTex )
        {
            obj->NC_STACK_rsrc::rsrc_func65(res);
            return nullptr;
        }
    }
    else
        bitm->swTex = ILBM_PrepareTruecolorPngSurface(loaded, transp);

    int convertColor = stak.Get<int32_t>(NC_STACK_bitmap::BMD_ATT_CONVCOLOR, 0);
    if ( convertColor )
    {
        SDL_Surface *screenFmt = GFX::Engine.ConvertToScreenFormat(bitm->swTex);
        if ( screenFmt )
        {
            SDL_DestroySurface(bitm->swTex);
            bitm->swTex = screenFmt;
            if ( bitm->palette )
            {
                delete bitm->palette;
                bitm->palette = nullptr;
            }
        }
    }

    if ( legacyColorKey )
        ILBM_NormalizeLegacyColorKeyPngSurface(bitm->swTex);

    return res;
}

static bool ILBM_ReadLegacyDimensions(const std::string &name, int *width, int *height)
{
    IFFile *mfile = IFFile::RsrcOpenIFFileVanilla(name, "rb");
    if ( !mfile )
        return false;

    bool ok = false;

    if ( !mfile->parse() )
    {
        const IFFile::Context &formChunk = mfile->GetCurrentChunk();
        if ( formChunk.Is(TAG_FORM) && (formChunk.TAG_EXTENSION == TAG_ILBM || formChunk.TAG_EXTENSION == TAG_VBMP) )
        {
            while ( true )
            {
                int parseResult = mfile->parse();
                if ( parseResult == IFFile::IFF_ERR_EOC )
                    break;
                if ( parseResult )
                    break;

                const IFFile::Context &chunk = mfile->GetCurrentChunk();
                if ( chunk.Is(TAG_BMHD) )
                {
                    if (width)
                        *width = mfile->readU16B();
                    if (height)
                        *height = mfile->readU16B();
                    ok = true;
                    break;
                }
                else if ( chunk.Is(TAG_HEAD) )
                {
                    if (width)
                        *width = mfile->readU16B();
                    if (height)
                        *height = mfile->readU16B();
                    ok = true;
                    break;
                }
                else
                {
                    mfile->skipChunk();
                }
            }
        }
    }

    delete mfile;
    return ok;
}

static rsrc *ILBM_ReadSet46RootPngOverride(NC_STACK_ilbm *obj, IDVList &stak, const std::string &name, IFFile *embeddedFile)
{
    IFFile::SetLooseOverride overrideInfo;
    if ( !IFFile::FindSet46RootPngOverride(name, "rb", &overrideInfo, "NC_STACK_ilbm::rsrc_func64") )
        return nullptr;

    IDVList pngStak = stak;
    pngStak.Add(NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1);
    pngStak.Add(NC_STACK_ilbm::ATT_ALPHAPALETTE, (int32_t)0);

    rsrc *pngRes = ILBM_ReadPngOverride(obj, pngStak, overrideInfo.resolvedPath, 0, true);
    if ( !pngRes )
    {
        ypa_log_out("WARNING: OpenNeoUA Set46 PNG override failed for %s; falling back to legacy asset %s.\n",
                    overrideInfo.resolvedPath.c_str(), name.c_str());
        return nullptr;
    }

    ResBitmap *pngBitmap = (ResBitmap *)pngRes->data;
    int legacyWidth = 0;
    int legacyHeight = 0;
    if ( pngBitmap && ILBM_ReadLegacyDimensions(name, &legacyWidth, &legacyHeight) &&
         (pngBitmap->width != legacyWidth || pngBitmap->height != legacyHeight) )
    {
        ypa_log_out("WARNING: OpenNeoUA Set46 PNG override %s size %dx%d differs from legacy %s size %dx%d; falling back to legacy asset.\n",
                    overrideInfo.resolvedPath.c_str(), pngBitmap->width, pngBitmap->height,
                    name.c_str(), legacyWidth, legacyHeight);
        obj->NC_STACK_rsrc::rsrc_func65(pngRes);
        return nullptr;
    }

    ypa_log_out("OpenNeoUA Set46 PNG override used: %s -> %s\n", name.c_str(), overrideInfo.resolvedPath.c_str());

    if ( embeddedFile )
        ILBM_SkipEmbeddedChunk(embeddedFile);

    return pngRes;
}

static void ILBM_SkipEmbeddedChunk(IFFile *mfile)
{
    if ( mfile && !mfile->parse() )
        mfile->skipChunk();
}

static rsrc *ILBM_ReadEmbeddedOverride(NC_STACK_ilbm *obj, IDVList &stak, const std::string &name, IFFile *embeddedFile)
{
    IFFile::SetLooseOverride overrideInfo;
    IFFile mfile = IFFile::UAOpenIFFileWithSetLooseEmbeddedOverride(name, "rb", &overrideInfo, "NC_STACK_ilbm::rsrc_func64");
    if ( !mfile.OK() )
        return nullptr;

    rsrc *res = obj->READ_ILBM(stak, &mfile, 0);
    if ( res )
    {
        mfile.ReportSetLooseOverrideUsed();
        ILBM_SkipEmbeddedChunk(embeddedFile);
        return res;
    }

    IFFile::ReportSetLooseOverrideFailed(overrideInfo, "loose embedded override existed but failed to load; embedded SET.BAS fallback used.");
    return nullptr;
}


// Create ilbm resource node and fill rsrc field data
rsrc * NC_STACK_ilbm::rsrc_func64(IDVList &stak)
{
    const std::string resName = stak.Get<std::string>(RSRC_ATT_NAME, "");
    const std::string setLoosePngPath = stak.Get<std::string>(RSRC_ATT_SET_LOOSE_PNG_PATH, "");
    const std::string externalTextureFile = stak.Get<std::string>(RSRC_ATT_EXTERNAL_TEXTURE_FILE, "");
    const char *reassignName = nullptr;

    if ( resName.empty() )
        return nullptr;

    if (!externalTextureFile.empty())
    {
        IFFile texture(FSMgr::iDir::openFile(externalTextureFile, "rb"));
        rsrc *loaded = texture.OK() ? READ_ILBM(stak, &texture, 0) : nullptr;
        if (loaded && (!loaded->data || !static_cast<ResBitmap *>(loaded->data)->swTex))
        {
            rsrc_func65(loaded);
            loaded = nullptr;
        }
        return loaded;
    }

    if ( !setLoosePngPath.empty() )
        return ILBM_ReadPngOverride(this, stak, setLoosePngPath, 0);

    if ( GFX::Engine.can_destblend )
    {
        if ( !StriCmp(resName, "fx1.ilbm") )
            reassignName = "hi/alpha/fx1.ilbm";
        else if ( !StriCmp(resName, "fx2.ilbm") )
            reassignName = "hi/alpha/fx2.ilbm";
        else if ( !StriCmp(resName, "fx3.ilbm") )
            reassignName = "hi/alpha/fx3.ilbm";
    }
    else if ( GFX::Engine.can_stippling )
    {
        if ( !StriCmp(resName, "fx1.ilbm") )
            reassignName = "hi/beta/fx1.ilbm";
        else if ( !StriCmp(resName, "fx2.ilbm") )
            reassignName = "hi/beta/fx2.ilbm";
        else if ( !StriCmp(resName, "fx3.ilbm") )
            reassignName = "hi/beta/fx3.ilbm";
    }
    else if ( GFX::Engine.can_srcblend )
    {
        if ( !StriCmp(resName, "fx1.ilbm") )
            reassignName = "hi/gamma/fx1.ilbm";
        else if ( !StriCmp(resName, "fx2.ilbm") )
            reassignName = "hi/gamma/fx2.ilbm";
        else if ( !StriCmp(resName, "fx3.ilbm") )
            reassignName = "hi/gamma/fx3.ilbm";
    }

    IFFile *mfile = stak.Get<IFFile *>(RSRC_ATT_PIFFFILE, nullptr);

    if ( reassignName )
    {
        IDVList hiStak = stak;
        if ( mfile )
            hiStak.Add(BMD_ATT_CONVCOLOR, (int32_t)1);

        IFFile::SetLooseOverride pngOverrideInfo;
        if ( IFFile::FindSetHiEffectPngOverride(reassignName, "rb", &pngOverrideInfo, "NC_STACK_ilbm::rsrc_func64") )
        {
            rsrc *pngRes = ILBM_ReadPngOverride(this, hiStak, pngOverrideInfo.resolvedPath, 1);
            if ( pngRes )
            {
                IFFile::ReportSetLooseOverrideUsed(pngOverrideInfo);

                if ( mfile )
                    ILBM_SkipEmbeddedChunk(mfile);

                return pngRes;
            }

            IFFile::ReportSetLooseOverrideFailed(pngOverrideInfo, "HI PNG effect override existed but failed to load; HI ILBM fallback used.");
        }

        rsrc *replacement = ILBM_ReadOpenedOrVanilla(this, hiStak, reassignName, 1);
        if ( replacement )
        {
            if ( mfile )
                ILBM_SkipEmbeddedChunk(mfile);
            return replacement;
        }

        if ( mfile )
        {
            stak.Add(BMD_ATT_CONVCOLOR, (int32_t)1);
            return READ_ILBM(stak, mfile, 0);
        }

        return nullptr;
    }
    else
    {
        rsrc *set46OverrideRes = ILBM_ReadSet46RootPngOverride(this, stak, resName, mfile);
        if ( set46OverrideRes )
            return set46OverrideRes;

        if ( mfile )
        {
            stak.Add(BMD_ATT_CONVCOLOR, (int32_t)1);
            if ( !stak.Get<int32_t>(RSRC_ATT_SKIP_SET_LOOSE_OVERRIDE, 0) )
            {
                rsrc *overrideRes = ILBM_ReadEmbeddedOverride(this, stak, resName, mfile);
                if ( overrideRes )
                    return overrideRes;
            }

            return READ_ILBM(stak, mfile, 0);
        }
    }

    // Texture of an external BASE without an ILBM beside it: use the PNG beside it instead.
    std::string externalPngPath;
    if ( IFFile::FindExternalBasePngFallback(resName, &externalPngPath) )
    {
        rsrc *pngRes = ILBM_ReadPngOverride(this, stak, externalPngPath, 0);
        if ( pngRes )
            return pngRes;

        ypa_log_out("WARNING: external BASE PNG texture failed to load: %s\n", externalPngPath.c_str());
    }

    return ILBM_ReadOpenedOrVanilla(this, stak, resName, 0);
}

void ILBM__WRITE_TO_FILE_BMHD(IFFile *mfile, ResBitmap *bitm)
{
    mfile->pushChunk(0, TAG_BMHD, -1); //20 bytes

    mfile->writeU16B(bitm->width);
    mfile->writeU16B(bitm->height);
    mfile->writeU16B(0); // x
    mfile->writeU16B(0); // y
    mfile->writeU8(8); // nPlanes
    mfile->writeU8(0); // masking
    mfile->writeU8(0); // compression
    mfile->writeU8(0x80); //flags
    mfile->writeU16B(0); // transparentColor
    mfile->writeS8(22); // xAspect
    mfile->writeS8(22); // yAspect
    mfile->writeU16B(bitm->width); // pageWidth
    mfile->writeU16B(bitm->height); // pageHeight

    mfile->popChunk();
}

int ILBM__WRITE_TO_FILE_BODY(IFFile *mfile, ResBitmap *bitm)
{
    int planeSz = 2 * ((bitm->width + 15) / 16);
    std::vector<uint8_t> buf(planeSz);

    mfile->pushChunk(0, TAG_BODY, 8 * bitm->height * planeSz);

    SDL_LockSurface(bitm->swTex);
    const uint8_t *bfline = (uint8_t *)bitm->swTex->pixels;

    for (int i = bitm->height; i > 0; i-- )
    {
        for (int plane = 0; plane < 8; plane++)
        {
            std::fill(buf.begin(), buf.end(), 0);

            for (int x = 0; x < bitm->width; x++)
            {
                if ( (1 << plane) & bfline[x] )
                    buf[ x / 8 ] |= ( 1 << ( (x & 7) ^ 7) );
            }

            mfile->write((const void *)buf.data(), planeSz);
        }

        bfline += bitm->width;
    }
    SDL_UnlockSurface(bitm->swTex);

    mfile->popChunk();
    return 1;
}

int ILBM__WRITE_TO_FILE(IFFile *mfile, ResBitmap *bitm)
{
    if ( mfile->pushChunk(TAG_ILBM, TAG_FORM, -1) )
        return 0;


    ILBM__WRITE_TO_FILE_BMHD(mfile, bitm);

    if ( bitm->palette )
    {
        mfile->pushChunk(0, TAG_CMAP, 256 * 3);
        mfile->write(bitm->palette, 256 * 3);
        mfile->popChunk();
    }

    if ( !ILBM__WRITE_TO_FILE_BODY(mfile, bitm) )
        return 0;

    return mfile->popChunk() == IFFile::IFF_ERR_OK;
}

int VBMP__WRITE_TO_FILE(IFFile *mfile, ResBitmap *bitm)
{
    int pixelCount = bitm->height * bitm->width;
    if ( mfile->pushChunk(TAG_VBMP, TAG_FORM, -1) )
        return 0;

    mfile->pushChunk(0, TAG_HEAD, -1); // 6 bytes

    mfile->writeU16B(bitm->width);
    mfile->writeU16B(bitm->height);
    mfile->writeU16B(0); // flags

    mfile->popChunk();

    if ( bitm->palette )
    {
        mfile->pushChunk(0, TAG_CMAP, 256 * 3);
        mfile->write(bitm->palette, 256 * 3);
        mfile->popChunk();
    }

    mfile->pushChunk(0, TAG_BODY, pixelCount);

    SDL_LockSurface(bitm->swTex);
    mfile->write(bitm->swTex->pixels, pixelCount);
    SDL_UnlockSurface(bitm->swTex);

    mfile->popChunk();

    return mfile->popChunk() == IFFile::IFF_ERR_OK;
}

size_t NC_STACK_ilbm::rsrc_func66(rsrc_func66_arg *arg)
{
    IFFile *mfile;

    if ( arg->OpenedStream == 1 )
    {
        if ( !arg->filename )
            return 0;

        mfile = IFFile::RsrcOpenIFFile(arg->filename, "wb");
    }
    else
        mfile = arg->file;

    if ( !mfile )
        return 0;

    SetTime(1, 1);

    ResBitmap *bitm = GetBitmap();

    if ( !bitm || !bitm->swTex )
        return 0;

    int res;

    if ( _saveAsIlbm )
        res = ILBM__WRITE_TO_FILE(mfile, bitm);
    else
        res = VBMP__WRITE_TO_FILE(mfile, bitm);

    //// CHECK THIS---
    if ( res )
    {
        if ( arg->OpenedStream == 1 )
            delete mfile;
        res = arg->OpenedStream;
    }
    return res;
    //// CHECK THIS+++
}

void NC_STACK_ilbm::setILBM_saveFmt(int fmt)
{
    if ( fmt )
        _saveAsIlbm = true;
    else
        _saveAsIlbm = false;
}

int NC_STACK_ilbm::getILBM_saveFmt()
{
    return _saveAsIlbm;
}
