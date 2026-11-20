// Resource base (NC_STACK_rsrc): shared, reference-counted resources such as bitmaps and skeletons.
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <set>

#include "nucleus/rsrc.h"
#include "nucleus/IFFile.h"
#include "common/utils.h"
#include "common/log.h"

RSRCList NC_STACK_rsrc::privateList;
RSRCList NC_STACK_rsrc::publicList;
void (*NC_STACK_rsrc::FallbackArchiveLoad)(int) = nullptr;
void (*NC_STACK_rsrc::FallbackArchiveRelease)() = nullptr;

// Same warning only once per run: a texture fallback repeats for every state of every model.
static void ypa_log_out_once(const char *format, ...)
{
    static std::set<std::string> seen;
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (seen.insert(text).second)
        ypa_log_out("%s", text);
}

static rsrc *rsrc_find(RSRCList *list, const std::string &name, const std::string &variantKey = {})
{
    for( rsrc* res : *list )
    {
        if (!StriCmp(variantKey, res->variantKey) && !StriCmp(name, res->name))
            return res;
    }

    return nullptr;
}

static int loadingSetTexture(IDVList &stak, const std::string &resourceClass)
{
    if (resourceClass == "ilbm.class" && IFFile::IsSetLooseBaseObjectScopeActive() &&
        (stak.Get<IFFile *>(NC_STACK_rsrc::RSRC_ATT_PIFFFILE, nullptr) ||
         !stak.Get<std::string>(NC_STACK_rsrc::RSRC_ATT_SET_LOOSE_PNG_PATH, "").empty()))
        return IFFile::ActiveResourceSet();
    return 0;
}

size_t NC_STACK_rsrc::Init(IDVList &stak)
{
    if ( !NC_STACK_nucleus::Init(stak) )
        return 0;

    int reuse_loaded = 1;

    const std::string res_name = stak.Get<std::string>(RSRC_ATT_NAME, "");
    reuse_loaded = stak.Get<int32_t>(RSRC_ATT_TRYSHARED, 1);

    if ( res_name.empty() )
    {
        Deinit();
        return 0;
    }

    rsrc *res = nullptr;

    const std::string resourceClass = ClassName();
    const bool externalReference = !stak.Get<IFFile *>(RSRC_ATT_PIFFFILE, nullptr);
    const bool externalTexture = resourceClass == "ilbm.class" && externalReference &&
                                 IFFile::IsExternalBaseResourceScopeActive();
    auto loadTextureFile = [&](const std::string &path) -> rsrc *
    {
        if (path.empty())
            return nullptr;
        if (reuse_loaded == 1)
            if (rsrc *cached = rsrc_find(&publicList, res_name, path))
                return cached;
        IDVList textureArgs = stak;
        textureArgs.Add(RSRC_ATT_EXTERNAL_TEXTURE_FILE, path);
        textureArgs.Add(RSRC_ATT_EXTERNAL_VARIANT_KEY, path);
        return rsrc_func64(textureArgs);
    };

    // Texture already loaded from the SET.BAS of set setId (same file name).
    auto findArchiveTexture = [&](int setId) -> rsrc *
    {
        auto fileName = [](const std::string &name)
        {
            const size_t separator = name.find_last_of("/\\");
            return name.substr(separator == std::string::npos ? 0 : separator + 1);
        };
        const std::string wanted = fileName(res_name);
        for (rsrc *cached : publicList)
            if (cached->textureArchiveSet == setId && !StriCmp(fileName(cached->name), wanted))
                return cached;
        return nullptr;
    };

    const int archiveSet = loadingSetTexture(stak, resourceClass);
    if (externalTexture)
    {
        // One shared order (same as OpenNeoUA Studio): Textures/Set<N>, BASE folder, subfolders,
        // shared set textures, active SET.BAS, then the Set1 SET.BAS as the last resort.
        const int setId = IFFile::ActiveResourceSet();
        const IFFile::ExternalTexture texture = IFFile::ResolveExternalTexture(res_name);
        if (!texture.path.empty())
        {
            res = loadTextureFile(texture.path);
            if (res && texture.source == IFFile::ExternalTexture::SUBFOLDER)
            {
                if (texture.candidates > 1)
                    ypa_log_out_once("WARNING: texture %s: %d files with this name in the model subfolders; using %s.\n",
                                     res_name.c_str(), texture.candidates, texture.path.c_str());
                if (texture.otherSet)
                    ypa_log_out_once("WARNING: texture %s of set %d taken from another set's folder: %s\n",
                                     res_name.c_str(), setId, texture.path.c_str());
            }
        }

        // A PNG with the same name found in the model folders comes before the SET.BAS copy;
        // the generic loader below reads it.
        std::string pngBesideBase;
        const bool pngFound = !res && IFFile::FindExternalBasePngFallback(res_name, &pngBesideBase);

        if (!res && !pngFound && setId)
            res = findArchiveTexture(setId);

        if (!res && !pngFound && setId != 1 && FallbackArchiveLoad && FallbackArchiveRelease)
        {
            res = findArchiveTexture(1);
            if (!res)
            {
                FallbackArchiveLoad(1);
                res = findArchiveTexture(1);
                if (res)
                    res->ref_cnt++;    // keep it alive while the rest of the archive is released
                FallbackArchiveRelease();
                if (res)
                    res->ref_cnt--;
            }
            if (res)
                ypa_log_out_once("WARNING: texture %s not found for set %d; using the Set1 archive copy.\n",
                                 res_name.c_str(), setId);
        }
    }
    else
    {
        const std::string animationKey = resourceClass == "bmpanim.class" && externalReference
                                       ? IFFile::ExternalAnimationVariantKey() : std::string();
        if (!animationKey.empty())
            stak.Add(RSRC_ATT_EXTERNAL_VARIANT_KEY, animationKey);
        if (reuse_loaded == 1 && archiveSet)
        {
            for (rsrc *cached : publicList)
            {
                if (cached->textureArchiveSet == archiveSet && !StriCmp(cached->name, res_name))
                {
                    res = cached;
                    break;
                }
            }
        }
        else if (reuse_loaded == 1)
            res = rsrc_find(&publicList, res_name, animationKey);
    }

    if (!res)
        res = rsrc_func64(stak);

    if ( res )
    {
        res->ref_cnt++;
        resource = res;

        if ( stak.Get<int32_t>(RSRC_ATT_DONTCOPY, 0) )
            _flags |= 1;

        return 1;
    }
    else
    {
        Deinit();
        return 0;
    }
}

size_t NC_STACK_rsrc::Deinit()
{
    if ( resource )
    {
        resource->ref_cnt--;

        if ( resource->ref_cnt == 0 )
            rsrc_func65(resource);

        resource = nullptr;
    }

    return NC_STACK_nucleus::Deinit();
}

// Allocate resource node
rsrc * NC_STACK_rsrc::rsrc_func64(IDVList &stak)
{
    const std::string resname = stak.Get<std::string>(RSRC_ATT_NAME, "");
    int shared = stak.Get<int32_t>(RSRC_ATT_TRYSHARED, 1);
    int toTail = stak.Get<int32_t>(RSRC_ATT_TOTAIL, 0);

    if ( resname.empty() )
        return nullptr;

    rsrc *res = new rsrc(resname, shared);

    if ( res )
    {
        res->variantKey = stak.Get<std::string>(RSRC_ATT_EXTERNAL_VARIANT_KEY, "");
        res->textureArchiveSet = loadingSetTexture(stak, ClassName());

        RSRCList *lst;

        if (shared == 1)
            lst = &publicList;
        else
            lst = &privateList;

        if ( toTail )
            lst->push_back(res);
        else
            lst->push_front(res);
    }
    return res;
}

size_t NC_STACK_rsrc::rsrc_func65(rsrc *res)
{
    if (res->shared)
        publicList.remove(res);
    else
        privateList.remove(res);

    if ( res )
        delete res;

    return 1;
}

const std::string NC_STACK_rsrc::getRsrc_name() const
{
    if ( resource )
        return resource->name;

    return std::string();
}

int NC_STACK_rsrc::getRsrc_tryShared()
{
    if ( resource )
        return resource->shared;

    return 2;
}

void *NC_STACK_rsrc::getRsrc_pData()
{
    if (resource)
        return resource->data;

    else return nullptr;
}

int NC_STACK_rsrc::getRsrc_dontCopy()
{
    if (_flags & 1)
        return 1;

    return 0;
}

