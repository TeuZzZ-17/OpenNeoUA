// IFFile: IFF chunk reader/writer on top of the virtual file system, including loose-file overrides of SET.BAS
// resources.
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string.h>
#include <utility>
#include <vector>
#include "common/includes.h"
#include "nucleus/IFFile.h"
#include "common/utils.h"
#include "common/env.h"

namespace
{
int g_setLooseBaseObjectScopeDepth = 0;
std::vector<std::string> g_externalBaseResourceRoots;

std::string setLooseNormalizeSlashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

std::string setLooseLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return str;
}

std::string setLooseTagToString(uint32_t tag)
{
    char name[5] = {
        (char)((tag >> 24) & 0xFF),
        (char)((tag >> 16) & 0xFF),
        (char)((tag >> 8) & 0xFF),
        (char)(tag & 0xFF),
        0
    };
    return std::string(name);
}

std::string setLooseChunkLabel(const IFFile::Context &chunk)
{
    std::string label = setLooseTagToString(chunk.TAG);
    if (chunk.TAG == TAG_FORM)
        label += " " + setLooseTagToString(chunk.TAG_EXTENSION);
    return label;
}

std::string setLooseStripLeadingSlashes(std::string path)
{
    while (!path.empty() && (path.front() == '/' || path.front() == '\\'))
        path.erase(path.begin());
    return path;
}

std::string setLooseTrimResourceName(std::string path)
{
    while (!path.empty() && (unsigned char)path.front() <= 32)
        path.erase(path.begin());

    while (!path.empty() && (unsigned char)path.back() <= 32)
        path.pop_back();

    return path;
}

bool setLooseIsReadMode(const std::string &mode)
{
    return mode.find('r') != std::string::npos &&
           mode.find('w') == std::string::npos &&
           mode.find('a') == std::string::npos;
}

int32_t setLooseCurrentSetId()
{
    std::string prefix = setLooseLower(setLooseNormalizeSlashes(Common::Env.GetPrefix("rsrc")));
    const std::string tag = "data:set";

    if (prefix.compare(0, tag.size(), tag) != 0)
        return 0;

    size_t pos = tag.size();
    int32_t setId = 0;
    bool hasDigit = false;

    while (pos < prefix.size() && std::isdigit((unsigned char)prefix[pos]))
    {
        hasDigit = true;
        setId = setId * 10 + (prefix[pos] - '0');
        pos++;
    }

    if (!hasDigit || setId < 1 || setId > 7)
        return 0;

    return setId;
}

std::string setLooseAssetFromRequest(const std::string &filename, int32_t setId)
{
    std::string request = setLooseNormalizeSlashes(filename);
    size_t colonPos = request.find(':');

    if (colonPos != std::string::npos)
    {
        std::string prefix = setLooseLower(request.substr(0, colonPos));
        if (prefix == "rsrc")
            return setLooseStripLeadingSlashes(request.substr(colonPos + 1));
    }

    std::string lowered = setLooseLower(request);
    std::string setTag = "data:set" + std::to_string(setId);

    if (lowered.compare(0, setTag.size(), setTag) == 0)
    {
        size_t pos = setTag.size();
        if (pos < request.size() && (request[pos] == ':' || request[pos] == '/' || request[pos] == '\\'))
            pos++;

        return setLooseStripLeadingSlashes(request.substr(pos));
    }

    return std::string();
}

std::string setLooseExtension(const std::string &path)
{
    size_t slashPos = path.find_last_of("/\\");
    size_t dotPos = path.rfind('.');

    if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos))
        return std::string();

    return setLooseLower(path.substr(dotPos + 1));
}


std::string externalBaseParentPath(std::string path)
{
    path = setLooseNormalizeSlashes(path);
    while (!path.empty() && path.back() == '/')
        path.pop_back();

    const size_t slash = path.rfind('/');
    if (slash == std::string::npos)
        return std::string();

    return path.substr(0, slash);
}

bool externalBaseFindUniqueRecursive(const std::string &dirPath, const std::string &fileName,
                                     std::string *match, int *matchCount)
{
    FSMgr::DirIter it = FSMgr::iDir::readDir(dirPath);
    if (!it)
        return false;

    FSMgr::iNode *node = nullptr;
    while (it.getNext(&node))
    {
        if (!node)
            continue;

        const std::string childPath = setLooseNormalizeSlashes(node->getVPath());
        if (node->getType() == FSMgr::iNode::NTYPE_DIR)
        {
            externalBaseFindUniqueRecursive(childPath, fileName, match, matchCount);
            if (*matchCount > 1)
                return false;
        }
        else if (node->getType() == FSMgr::iNode::NTYPE_FILE && !StriCmp(node->getName(), fileName))
        {
            *match = childPath;
            (*matchCount)++;
            if (*matchCount > 1)
                return false;
        }
    }

    return *matchCount == 1;
}

std::string setLooseFileName(const std::string &path);

bool externalBaseResolveLocalResource(const std::string &filename, const std::string &mode,
                                      std::string *resolved)
{
    if (!resolved || g_externalBaseResourceRoots.empty() || !setLooseIsReadMode(mode))
        return false;

    std::string assetPath = setLooseTrimResourceName(setLooseStripLeadingSlashes(setLooseNormalizeSlashes(filename)));
    const size_t colon = assetPath.find(':');
    if (colon != std::string::npos)
        assetPath = setLooseStripLeadingSlashes(assetPath.substr(colon + 1));

    if (assetPath.empty() || assetPath.find(':') != std::string::npos)
        return false;

    size_t segmentStart = 0;
    while (segmentStart <= assetPath.size())
    {
        const size_t separator = assetPath.find('/', segmentStart);
        const size_t segmentEnd = separator == std::string::npos ? assetPath.size() : separator;
        const std::string segment = assetPath.substr(segmentStart, segmentEnd - segmentStart);
        if (segment.empty() || segment == "." || segment == "..")
            return false;
        if (separator == std::string::npos)
            break;
        segmentStart = separator + 1;
    }

    const std::string &root = g_externalBaseResourceRoots.back();

    // 1) Honor the path authored inside the BASE exactly, relative to its directory.
    const std::string exact = root + "/" + assetPath;
    if (FSMgr::iDir::fileExist(exact))
    {
        *resolved = exact;
        return true;
    }

    // 2) Legacy BASE families are often unpacked flat: ignore archived prefixes
    // such as Skeleton/ or rsrcpool/ and try the requested basename beside the BASE.
    const std::string basename = setLooseFileName(assetPath);
    if (basename.empty())
        return false;

    const std::string besideBase = root + "/" + basename;
    if (FSMgr::iDir::fileExist(besideBase))
    {
        *resolved = besideBase;
        return true;
    }

    // 3) Finally search the BASE family's subdirectories by basename. Only a unique
    // match is accepted so two identically named resources never resolve at random.
    std::string recursiveMatch;
    int matchCount = 0;
    externalBaseFindUniqueRecursive(root, basename, &recursiveMatch, &matchCount);
    if (matchCount == 1)
    {
        *resolved = recursiveMatch;
        return true;
    }

    return false;
}

std::string setLooseReplaceExtension(const std::string &path, const std::string &newExt)
{
    size_t slashPos = path.find_last_of("/\\");
    size_t dotPos = path.rfind('.');

    if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos))
        return path + newExt;

    return path.substr(0, dotPos) + newExt;
}

std::string setLooseFileName(const std::string &path)
{
    size_t slashPos = path.find_last_of("/\\");
    if (slashPos == std::string::npos)
        return path;
    return path.substr(slashPos + 1);
}

std::string setLooseDirName(const std::string &path)
{
    size_t slashPos = path.find_last_of("/\\");
    if (slashPos == std::string::npos)
        return std::string();
    return path.substr(0, slashPos);
}

bool setLooseCanOpenDiskFile(const std::string &path)
{
    FSMgr::FileHandle fil(path, "rb");
    return fil.OK();
}

bool setLooseResolveReadableFileAtPath(const std::string &virtualPath, std::string *openPath)
{
    if ( FSMgr::iDir::fileExist(virtualPath) )
    {
        if (openPath)
            *openPath = virtualPath;
        return true;
    }

    std::string dirName = setLooseDirName(virtualPath);
    std::string fileName = setLooseFileName(virtualPath);
    if ( dirName.empty() || fileName.empty() )
        return false;

    FSMgr::iNode *dirNode = FSMgr::iDir::findNode(dirName);
    if ( !dirNode || dirNode->getType() != FSMgr::iNode::NTYPE_DIR )
        return false;

    std::string diskPath = setLooseNormalizeSlashes(dirNode->getPath());
    if ( !diskPath.empty() && diskPath.back() != '/' )
        diskPath += "/";
    diskPath += fileName;

    if ( !setLooseCanOpenDiskFile(diskPath) )
        return false;

    if (openPath)
        *openPath = diskPath;
    return true;
}

bool setLooseResolveReadableFile(const std::string &virtualPath, std::string *openPath)
{
    const std::string organizedPath = uaSetDirectoryOrganizedPath(virtualPath);
    if ( organizedPath != setLooseNormalizeSlashes(virtualPath) &&
         setLooseResolveReadableFileAtPath(organizedPath, openPath) )
        return true;

    return setLooseResolveReadableFileAtPath(virtualPath, openPath);
}

bool setLooseSupportedExtension(const std::string &assetPath)
{
    std::string ext = setLooseExtension(assetPath);

    return ext == "base" || ext == "bas" ||
           ext == "sklt" || ext == "skl" ||
           ext == "ilbm" || ext == "ilb" ||
           ext == "anm" || ext == "vanm";
}

std::string setLooseCanonicalFolderForExtension(const std::string &assetPath)
{
    const std::string ext = setLooseExtension(assetPath);
    if ( ext == "base" || ext == "bas" )
        return "BASE";
    if ( ext == "sklt" || ext == "skl" )
        return "SKLT";
    if ( ext == "ilbm" || ext == "ilb" )
        return "ILBM";
    if ( ext == "anm" || ext == "vanm" )
        return "ANM";
    return std::string();
}

std::string setLooseCanonicalFolderForClass(const std::string &className)
{
    const std::string cls = setLooseLower(className);
    if ( cls == "ilbm.class" )
        return "ILBM";
    if ( cls == "sklt.class" )
        return "SKLT";
    if ( cls == "bmpanim.class" )
        return "ANM";
    return std::string();
}

std::string setLooseCanonicalAssetPath(const std::string &assetPath)
{
    const std::string folder = setLooseCanonicalFolderForExtension(assetPath);
    const std::string fileName = setLooseFileName(assetPath);
    if ( folder.empty() || fileName.empty() )
        return std::string();
    return folder + "/" + fileName;
}

std::string setLooseCanonicalEmrsAssetPath(const std::string &className, const std::string &assetPath)
{
    const std::string folder = setLooseCanonicalFolderForClass(className);
    const std::string fileName = setLooseFileName(assetPath);
    if ( folder.empty() || fileName.empty() )
        return std::string();
    return folder + "/" + fileName;
}

bool set46RootPrefixActive()
{
    std::string prefix = setLooseLower(setLooseNormalizeSlashes(Common::Env.GetPrefix("rsrc")));
    const std::string tag = "data:set46";

    if (prefix.compare(0, tag.size(), tag) != 0)
        return false;

    return prefix.size() == tag.size() || prefix[tag.size()] == '/' || prefix[tag.size()] == ':';
}

std::string set46RootAssetFromRequest(const std::string &filename)
{
    std::string request = setLooseTrimResourceName(setLooseNormalizeSlashes(filename));
    if ( request.empty() )
        return std::string();

    std::string lowered = setLooseLower(request);
    const std::string setTag = "data:set46";

    if (lowered.compare(0, setTag.size(), setTag) == 0)
    {
        size_t pos = setTag.size();
        if (pos < request.size() && (request[pos] == ':' || request[pos] == '/' || request[pos] == '\\'))
            pos++;

        return setLooseStripLeadingSlashes(request.substr(pos));
    }

    size_t colonPos = request.find(':');
    if (colonPos != std::string::npos)
    {
        std::string prefix = setLooseLower(request.substr(0, colonPos));
        if (prefix == "rsrc" && set46RootPrefixActive())
            return setLooseStripLeadingSlashes(request.substr(colonPos + 1));

        return std::string();
    }

    if ( set46RootPrefixActive() )
        return setLooseStripLeadingSlashes(request);

    return std::string();
}

bool set46RootSupportedPngOverrideAsset(const std::string &assetPath)
{
    if ( assetPath.empty() || assetPath.find(':') != std::string::npos )
        return false;

    std::string normalized = setLooseStripLeadingSlashes(setLooseNormalizeSlashes(assetPath));
    if ( normalized.find('/') != std::string::npos || normalized.find('\\') != std::string::npos )
        return false;

    std::string ext = setLooseExtension(normalized);
    return ext.empty() || ext == "ilbm" || ext == "ilb" || ext == "iff";
}

// True when Data/SetN/Loose exists. Cached per set: the folder is checked once.
bool setLooseRootAvailable(int32_t setId)
{
    static std::map<int32_t, bool> available;
    auto found = available.find(setId);
    if ( found != available.end() )
        return found->second;

    const std::string root = uaSetDirectoryResolvedReadPath("Data/Set" + std::to_string(setId) + "/Loose/", true);
    FSMgr::iNode *rootNode = FSMgr::iDir::findNode(root);
    const bool exists = rootNode && rootNode->getType() == FSMgr::iNode::NTYPE_DIR;
    available[setId] = exists;
    return exists;
}

// Finds `relative` in a Loose folder: first as written, then with the legacy
// 3-letter extension (.BASE -> .BAS). Returns the path to open, or "" when the
// file is missing. `legacyExt` tells which of the two names was found.
std::string setLooseFindFile(const std::string &looseRoot, const std::string &relative, bool *legacyExt = nullptr)
{
    const std::string path = looseRoot + relative;
    std::string openPath;
    if ( setLooseResolveReadableFile(path, &openPath) )
    {
        if (legacyExt)
            *legacyExt = false;
        return openPath;
    }

    const std::string legacyPath = setLooseNormalizeSlashes(correctSeparatorAndExt(path));
    if ( legacyPath != setLooseNormalizeSlashes(path) && setLooseResolveReadableFile(legacyPath, &openPath) )
    {
        if (legacyExt)
            *legacyExt = true;
        return openPath;
    }

    return std::string();
}

bool setLooseIsSkyArchive(const IFFile::SetLooseOverride &overrideInfo)
{
    return overrideInfo.active && overrideInfo.setId == 0 &&
           overrideInfo.extensionForm.find("sky loose archive") != std::string::npos;
}

void setLooseLogUsed(const IFFile::SetLooseOverride &overrideInfo)
{
    if ( setLooseIsSkyArchive(overrideInfo) )
        ypa_log_out("OpenNeoUA sky loose archive override used: %s -> %s\n", overrideInfo.requested.c_str(), overrideInfo.resolvedPath.c_str());
}

void setLooseLogFailed(const IFFile::SetLooseOverride &overrideInfo, const std::string &reason)
{
    if ( setLooseIsSkyArchive(overrideInfo) )
        ypa_log_out("WARNING: OpenNeoUA sky loose archive override failed for %s (%s); %s\n",
                    overrideInfo.requested.c_str(),
                    overrideInfo.resolvedPath.c_str(),
                    reason.c_str());
}

std::string setLooseEmbeddedAssetFromRequest(const std::string &filename, int32_t setId)
{
    std::string assetPath = setLooseAssetFromRequest(filename, setId);
    if (!assetPath.empty())
        return assetPath;

    return setLooseStripLeadingSlashes(setLooseNormalizeSlashes(filename));
}

struct SkyLooseScopeState
{
    bool active = false;
    int depth = 0;
    std::string stem;
    std::vector<std::string> stemCandidates;
};

SkyLooseScopeState g_skyLooseScope;

std::string skyLooseUpper(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return (char)std::toupper(c);
    });
    return str;
}

void skyLoosePushUnique(std::vector<std::string> *items, const std::string &value)
{
    if (!items || value.empty())
        return;

    for (const std::string &existing : *items)
    {
        if (!StriCmp(existing, value))
            return;
    }
    items->push_back(value);
}

std::vector<std::string> skyLooseBuildStemCandidates(const std::string &stem)
{
    std::vector<std::string> result;
    skyLoosePushUnique(&result, stem);
    skyLoosePushUnique(&result, skyLooseUpper(stem));
    skyLoosePushUnique(&result, setLooseLower(stem));
    return result;
}

std::string skyLooseStemFromName(std::string name)
{
    name = setLooseTrimResourceName(setLooseStripLeadingSlashes(setLooseNormalizeSlashes(name)));
    if (name.empty())
        return std::string();

    size_t colonPos = name.find(':');
    if (colonPos != std::string::npos)
        name = setLooseStripLeadingSlashes(name.substr(colonPos + 1));

    std::string lower = setLooseLower(name);
    const std::string objectsTag = "objects/";
    size_t objectsPos = lower.rfind(objectsTag);
    if (objectsPos != std::string::npos)
        name = name.substr(objectsPos + objectsTag.size());

    std::string fileName = setLooseFileName(name);
    size_t dotPos = fileName.rfind('.');
    if (dotPos != std::string::npos)
        fileName.resize(dotPos);

    return setLooseTrimResourceName(fileName);
}

std::string skyLooseResourceName(std::string name)
{
    name = setLooseTrimResourceName(setLooseStripLeadingSlashes(setLooseNormalizeSlashes(name)));
    if (name.empty())
        return std::string();

    size_t colonPos = name.find(':');
    if (colonPos != std::string::npos)
        name = setLooseStripLeadingSlashes(name.substr(colonPos + 1));

    return setLooseTrimResourceName(name);
}

bool skyLooseClassSupported(const std::string &className)
{
    std::string cls = setLooseLower(className);
    return cls == "ilbm.class" ||
           cls == "sklt.class" ||
           cls == "bmpanim.class";
}

bool skyLooseResolveFirst(const std::vector<std::string> &candidates, std::string *openPath)
{
    for (const std::string &candidate : candidates)
    {
        std::string resolved;
        if (setLooseResolveReadableFile(candidate, &resolved))
        {
            if (openPath)
                *openPath = resolved;
            return true;
        }
    }
    return false;
}

void skyLoosePushPathCandidate(std::vector<std::string> *candidates, const std::string &path)
{
    if (!candidates || path.empty())
        return;

    skyLoosePushUnique(candidates, path);

    std::string legacyPath = setLooseNormalizeSlashes(correctSeparatorAndExt(path));
    if (legacyPath != setLooseNormalizeSlashes(path))
        skyLoosePushUnique(candidates, legacyPath);
}

std::vector<std::string> skyLooseClassFolders(const std::string &className)
{
    std::vector<std::string> folders;
    std::string cls = setLooseLower(className);

    if (cls == "sklt.class")
    {
        folders.push_back("SKLT");
        folders.push_back("Skeleton");
        folders.push_back("skeleton");
    }
    else if (cls == "ilbm.class")
    {
        folders.push_back("ILBM");
        folders.push_back("Textures");
        folders.push_back("Texture");
        folders.push_back("textures");
        folders.push_back("texture");
    }
    else if (cls == "bmpanim.class")
    {
        folders.push_back("VANM");
        folders.push_back("ANM");
        folders.push_back("Animations");
        folders.push_back("animations");
    }

    return folders;
}

std::vector<std::string> skyLooseBuildEmbeddedCandidates(const std::string &stem, const std::string &assetPath, const std::string &className, bool pngTexture)
{
    std::vector<std::string> candidates;
    std::string root = "Data/Objects/Loose/" + stem + "/";
    std::string normalizedAsset = setLooseStripLeadingSlashes(setLooseNormalizeSlashes(assetPath));
    std::string fileName = setLooseFileName(normalizedAsset);
    std::vector<std::string> classFolders = skyLooseClassFolders(className);

    if (normalizedAsset.empty())
        return candidates;

    if (pngTexture)
    {
        std::string exactUpper = setLooseReplaceExtension(normalizedAsset, ".PNG");
        std::string exactLower = setLooseReplaceExtension(normalizedAsset, ".png");
        std::string baseUpper = setLooseReplaceExtension(fileName, ".PNG");
        std::string baseLower = setLooseReplaceExtension(fileName, ".png");

        skyLoosePushPathCandidate(&candidates, root + exactUpper);
        skyLoosePushPathCandidate(&candidates, root + exactLower);
        skyLoosePushPathCandidate(&candidates, root + baseUpper);
        skyLoosePushPathCandidate(&candidates, root + baseLower);

        for (const std::string &folder : classFolders)
        {
            skyLoosePushPathCandidate(&candidates, root + folder + "/" + baseUpper);
            skyLoosePushPathCandidate(&candidates, root + folder + "/" + baseLower);
        }
    }
    else
    {
        skyLoosePushPathCandidate(&candidates, root + normalizedAsset);
        skyLoosePushPathCandidate(&candidates, root + fileName);

        for (const std::string &folder : classFolders)
            skyLoosePushPathCandidate(&candidates, root + folder + "/" + fileName);
    }

    return candidates;
}
}


bool IFFile::BeginSkyLooseScope(const std::string &skyName)
{
    std::string stem = skyLooseStemFromName(skyName);
    if (stem.empty())
        return false;

    g_skyLooseScope.active = true;
    g_skyLooseScope.depth++;
    g_skyLooseScope.stem = stem;
    g_skyLooseScope.stemCandidates = skyLooseBuildStemCandidates(stem);
    return true;
}

void IFFile::EndSkyLooseScope()
{
    if (g_skyLooseScope.depth > 0)
        g_skyLooseScope.depth--;

    if (g_skyLooseScope.depth <= 0)
    {
        g_skyLooseScope.active = false;
        g_skyLooseScope.depth = 0;
        g_skyLooseScope.stem.clear();
        g_skyLooseScope.stemCandidates.clear();
    }
}

bool IFFile::IsSkyLooseScopeActive()
{
    return g_skyLooseScope.active && !g_skyLooseScope.stem.empty();
}

bool IFFile::FindSkyLooseArchiveOverride(const std::string &filename, const std::string &mode, std::string *outPath, const char *sourceFunction)
{
    (void)sourceFunction;

    if (outPath)
        outPath->clear();

    if (!IsSkyLooseScopeActive() || !setLooseIsReadMode(mode))
        return false;

    std::string requestedStem = skyLooseStemFromName(filename);
    if (!requestedStem.empty() && StriCmp(requestedStem, g_skyLooseScope.stem))
        return false;

    std::vector<std::string> candidates;
    for (const std::string &stem : g_skyLooseScope.stemCandidates)
    {
        candidates.push_back("Data/Objects/Loose/" + stem + ".BASE");
        candidates.push_back("Data/Objects/Loose/" + stem + ".BAS");
        candidates.push_back("Data/Objects/Loose/" + stem + ".base");
        candidates.push_back("Data/Objects/Loose/" + stem + ".bas");
    }

    return skyLooseResolveFirst(candidates, outPath);
}

bool IFFile::FindSkyLooseEmrsOverride(const std::string &filename, const std::string &mode, const std::string &className, const std::string &payload, SetLooseOverride *out, const char *sourceFunction, size_t currentOffset)
{
    (void)currentOffset;

    if (out)
        *out = SetLooseOverride();

    if (!IsSkyLooseScopeActive() || !setLooseIsReadMode(mode) || !skyLooseClassSupported(className))
        return false;

    std::string assetPath = skyLooseResourceName(filename);
    if (assetPath.empty() || assetPath.find(':') != std::string::npos)
        return false;

    std::vector<std::string> candidates;
    for (const std::string &stem : g_skyLooseScope.stemCandidates)
    {
        std::vector<std::string> stemCandidates = skyLooseBuildEmbeddedCandidates(stem, assetPath, className, false);
        candidates.insert(candidates.end(), stemCandidates.begin(), stemCandidates.end());
    }

    std::string openPath;
    if (!skyLooseResolveFirst(candidates, &openPath))
        return false;

    if (out)
    {
        out->active = true;
        out->setId = 0;
        out->requested = assetPath;
        out->resolvedPath = openPath;
        out->extensionForm = "Data/Objects sky loose embedded override";
        out->embedded = true;
        out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_embed::LoadingFromIFF";
        out->emrs = true;
        out->emrsClass = className;
        out->embeddedPayload = payload;
    }
    return true;
}

bool IFFile::FindSkyLooseEmrsPngOverride(const std::string &filename, const std::string &mode, const std::string &className, const std::string &payload, SetLooseOverride *out, const char *sourceFunction, size_t currentOffset)
{
    (void)currentOffset;

    if (out)
        *out = SetLooseOverride();

    if (!IsSkyLooseScopeActive() || !setLooseIsReadMode(mode))
        return false;

    if (setLooseLower(className) != "ilbm.class")
        return false;

    std::string assetPath = skyLooseResourceName(filename);
    if (assetPath.empty() || assetPath.find(':') != std::string::npos)
        return false;

    std::string ext = setLooseExtension(assetPath);
    if (ext != "ilbm" && ext != "ilb")
        return false;

    std::vector<std::string> candidates;
    for (const std::string &stem : g_skyLooseScope.stemCandidates)
    {
        std::vector<std::string> stemCandidates = skyLooseBuildEmbeddedCandidates(stem, assetPath, className, true);
        candidates.insert(candidates.end(), stemCandidates.begin(), stemCandidates.end());
    }

    std::string openPath;
    if (!skyLooseResolveFirst(candidates, &openPath))
        return false;

    if (out)
    {
        out->active = true;
        out->setId = 0;
        out->requested = assetPath;
        out->resolvedPath = openPath;
        out->extensionForm = "Data/Objects sky loose PNG texture override";
        out->embedded = true;
        out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_embed::LoadingFromIFF";
        out->emrs = true;
        out->emrsClass = className;
        out->embeddedPayload = payload;
    }
    return true;
}


IFFile::IFFile(const std::string &diskPath, const std::string &mode)
: file_handle(diskPath, mode)
{
    ctxStack.emplace_front(TAG_FORM, TAG_NONE, 0x80000000, 0);
}


IFFile::IFFile(FSMgr::FileHandle *f, bool del)
: file_handle(f, false)
{
    ctxStack.emplace_front(TAG_FORM, TAG_NONE, 0x80000000, 0);

    if (f && del)
        delete f;
}

IFFile::IFFile(FSMgr::FileHandle &f)
: file_handle(&f, false)
{
    ctxStack.emplace_front(TAG_FORM, TAG_NONE, 0x80000000, 0);
}

IFFile::IFFile(FSMgr::FileHandle &&f)
: file_handle( std::move(f) )
{
    ctxStack.emplace_front(TAG_FORM, TAG_NONE, 0x80000000, 0);
}

IFFile *IFFile::RsrcOpenIFFile(const std::string &filename, const std::string &mode, const char *sourceFunction)
{
    IFFile result = UAOpenIFFile("rsrc:" + filename, mode, sourceFunction ? sourceFunction : "IFFile::RsrcOpenIFFile");
    if ( !result.OK() )
        return nullptr;

    return new IFFile(std::move(result));
}

IFFile *IFFile::RsrcOpenIFFileVanilla(const std::string &filename, const std::string &mode)
{
    IFFile result = UAOpenIFFileVanilla("rsrc:" + filename, mode);
    if ( !result.OK() )
        return nullptr;

    return new IFFile(std::move(result));
}

FSMgr::FileHandle IFFile::UAOpenFileVanilla(const std::string &filename, const std::string &mode)
{
    // Keep IFF/ILBM/SKLT reads on the same Data-first path used by the rest
    // of OpenNeoUA, while preserving organized SET and legacy root fallbacks.
    std::string tmpBuf = uaDataFirstResolvedReadPath(filename);

    if ( !FSMgr::iDir::fileExist(tmpBuf) )
        return FSMgr::FileHandle();

    return FSMgr::iDir::openFile(tmpBuf, mode);
}

IFFile IFFile::UAOpenIFFileVanilla(const std::string &filename, const std::string &mode)
{
    FSMgr::FileHandle fil = UAOpenFileVanilla(filename, mode);
    if ( !fil.OK() )
        return IFFile();

    return IFFile(std::move(fil));
}

// Shared search of FindSetLooseOverride / FindSetLooseEmbeddedOverride: the
// canonical Runtime Loose path first, then the pre-canonical layout.
static bool FindSetLooseFileOverride(const std::string &filename, const std::string &mode, IFFile::SetLooseOverride *out,
                                     const char *sourceFunction, bool embedded)
{
    if (out)
        *out = IFFile::SetLooseOverride();

    if ( !setLooseIsReadMode(mode) )
        return false;

    int32_t setId = setLooseCurrentSetId();
    if ( !setId )
        return false;

    std::string assetPath = embedded ? setLooseEmbeddedAssetFromRequest(filename, setId)
                                     : setLooseAssetFromRequest(filename, setId);
    if ( assetPath.empty() || !setLooseSupportedExtension(assetPath) )
        return false;

    assetPath = setLooseStripLeadingSlashes(setLooseNormalizeSlashes(assetPath));
    if ( assetPath.find(':') != std::string::npos )
        return false;

    const std::string canonicalAsset = setLooseCanonicalAssetPath(assetPath);
    if ( canonicalAsset.empty() || !setLooseRootAvailable(setId) )
        return false;

    const std::string looseRoot = "Data/Set" + std::to_string(setId) + "/Loose/";
    const std::string kind = embedded ? "canonical Runtime Loose embedded path" : "canonical Runtime Loose path";
    bool legacyExt = false;
    std::string resolvedPath = setLooseFindFile(looseRoot, canonicalAsset, &legacyExt);
    std::string extensionForm = legacyExt ? kind + ", legacy 3-letter extension" : kind;
    if ( resolvedPath.empty() )
    {
        resolvedPath = setLooseFindFile(looseRoot, assetPath);
        if ( resolvedPath.empty() )
            return false;
        extensionForm = embedded ? "legacy pre-canonical Runtime Loose embedded path" : "legacy pre-canonical Runtime Loose path";
    }

    if (out)
    {
        out->active = true;
        out->setId = setId;
        out->requested = assetPath;
        out->resolvedPath = resolvedPath;
        out->extensionForm = extensionForm;
        if ( !embedded )
            out->vanillaPath = setLooseNormalizeSlashes(uaSetDirectoryResolvedReadPath(filename));
        out->embedded = embedded;
        out->sourceFunction = sourceFunction ? sourceFunction :
                              embedded ? "IFFile::UAOpenFileWithSetLooseEmbeddedOverride" : "IFFile::UAOpenFileWithSetLooseOverride";
    }
    return true;
}

bool IFFile::FindSetLooseOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    return FindSetLooseFileOverride(filename, mode, out, sourceFunction, false);
}

bool IFFile::FindSetLooseEmbeddedOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    return FindSetLooseFileOverride(filename, mode, out, sourceFunction, true);
}

bool IFFile::FindSetLooseVisprotoListOverride(const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    if (out)
        *out = SetLooseOverride();

    if ( !setLooseIsReadMode(mode) )
        return false;

    int32_t setId = setLooseCurrentSetId();
    if ( !setId || !setLooseRootAvailable(setId) )
        return false;

    const std::string requested = "VISPROTO.LST";
    const std::string candidate = "Data/Set" + std::to_string(setId) + "/Loose/" + requested;
    std::string resolvedPath;
    if ( !setLooseResolveReadableFile(candidate, &resolvedPath) )
        return false;

    if (out)
    {
        out->active = true;
        out->setId = setId;
        out->requested = requested;
        out->resolvedPath = resolvedPath;
        out->extensionForm = "per-set VISPROTO list override";
        out->vanillaPath = uaSetDirectoryResolvedReadPath("Data/Set" + std::to_string(setId) + "/Scripts/VISPROTO.LST");
        out->sourceFunction = sourceFunction ? sourceFunction : "load_set_base";
    }

    return true;
}

bool IFFile::FindSetLooseBaseObjectOverride(const std::string &objectName, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    if (out)
        *out = SetLooseOverride();

    if ( !setLooseIsReadMode(mode) || !IsSetLooseBaseObjectScopeActive() )
        return false;

    int32_t setId = setLooseCurrentSetId();
    if ( !setId )
        return false;

    std::string cleanName = setLooseTrimResourceName(objectName);
    if ( cleanName.empty() || cleanName == "." || cleanName == ".." ||
         cleanName.find_first_of("/\\:") != std::string::npos )
        return false;

    if ( !setLooseRootAvailable(setId) )
        return false;

    const std::string looseRoot = "Data/Set" + std::to_string(setId) + "/Loose/";
    struct CandidateGroup
    {
        const char *folder;
        const char *label;
    };
    const CandidateGroup groups[] = {
        {"BASE", "BASE object override (canonical BASE folder)"},
        {"Base", "BASE object override (legacy Base folder)"},
        {"Skeleton", "BASE object override (legacy Skeleton folder)"}
    };

    for (const CandidateGroup &group : groups)
    {
        const std::string requested = std::string(group.folder) + "/" + cleanName + ".BASE";
        const std::string resolvedPath = setLooseFindFile(looseRoot, requested);
        if ( resolvedPath.empty() )
            continue;

        if (out)
        {
            out->active = true;
            out->setId = setId;
            out->requested = requested;
            out->resolvedPath = resolvedPath;
            out->extensionForm = group.label;
            out->embedded = true;
            out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_nucleus::LoadObjectFromIFF";
        }
        return true;
    }

    return false;
}

bool IFFile::FindSetLooseEmrsOverride(const std::string &filename, const std::string &mode, const std::string &className, const std::string &payload, SetLooseOverride *out, const char *sourceFunction, size_t currentOffset)
{
    (void)currentOffset;

    if (out)
        *out = SetLooseOverride();

    if ( !setLooseIsReadMode(mode) )
        return false;

    int32_t setId = setLooseCurrentSetId();
    if ( !setId )
        return false;

    std::string assetPath = setLooseEmbeddedAssetFromRequest(filename, setId);
    if ( assetPath.empty() )
        return false;

    assetPath = setLooseStripLeadingSlashes(setLooseNormalizeSlashes(assetPath));
    if ( assetPath.find(':') != std::string::npos )
        return false;

    const std::string canonicalAsset = setLooseCanonicalEmrsAssetPath(className, assetPath);
    if ( canonicalAsset.empty() || !setLooseRootAvailable(setId) )
        return false;

    const std::string looseRoot = "Data/Set" + std::to_string(setId) + "/Loose/";
    bool legacyExt = false;
    std::string resolvedPath = setLooseFindFile(looseRoot, canonicalAsset, &legacyExt);
    std::string extensionForm = legacyExt ? "canonical Runtime Loose EMRS path, legacy 3-letter extension"
                                          : "canonical Runtime Loose EMRS path";
    if ( resolvedPath.empty() )
    {
        // Pre-canonical layout: textures could also sit in Loose/Texture.
        std::vector<std::string> oldRelativeCandidates;
        if ( setLooseLower(className) == "ilbm.class" )
            oldRelativeCandidates.push_back("Texture/" + assetPath);
        oldRelativeCandidates.push_back(assetPath);

        for (const std::string &oldRelative : oldRelativeCandidates)
        {
            resolvedPath = setLooseFindFile(looseRoot, oldRelative);
            if ( !resolvedPath.empty() )
            {
                extensionForm = "legacy pre-canonical Runtime Loose EMRS path";
                break;
            }
        }
        if ( resolvedPath.empty() )
            return false;
    }

    if (out)
    {
        out->active = true;
        out->setId = setId;
        out->requested = assetPath;
        out->resolvedPath = resolvedPath;
        out->extensionForm = extensionForm;
        out->embedded = true;
        out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_embed::LoadingFromIFF";
        out->emrs = true;
        out->emrsClass = className;
        out->embeddedPayload = payload;
    }
    return true;
}

bool IFFile::FindSetLooseEmrsPngOverride(const std::string &filename, const std::string &mode, const std::string &className, const std::string &payload, SetLooseOverride *out, const char *sourceFunction, size_t currentOffset)
{
    (void)currentOffset;

    if (out)
        *out = SetLooseOverride();

    if ( !setLooseIsReadMode(mode) || setLooseLower(className) != "ilbm.class" )
        return false;

    int32_t setId = setLooseCurrentSetId();
    if ( !setId )
        return false;

    std::string assetPath = setLooseEmbeddedAssetFromRequest(filename, setId);
    if ( assetPath.empty() )
        return false;

    assetPath = setLooseTrimResourceName(setLooseStripLeadingSlashes(setLooseNormalizeSlashes(assetPath)));
    if ( assetPath.find(':') != std::string::npos )
        return false;

    const std::string assetFile = setLooseFileName(assetPath);
    const std::string assetBase = setLooseLower(assetFile);
    if ( assetBase == "fx1.ilbm" || assetBase == "fx1.ilb" ||
         assetBase == "fx2.ilbm" || assetBase == "fx2.ilb" ||
         assetBase == "fx3.ilbm" || assetBase == "fx3.ilb" )
        return false;

    const std::string ext = setLooseExtension(assetFile);
    if ( ext != "ilbm" && ext != "ilb" )
        return false;

    if ( !setLooseRootAvailable(setId) )
        return false;

    const std::string looseRoot = "Data/Set" + std::to_string(setId) + "/Loose/";
    const std::string canonicalUpper = "ILBM/" + setLooseReplaceExtension(assetFile, ".PNG");
    const std::string canonicalLower = "ILBM/" + setLooseReplaceExtension(assetFile, ".png");
    std::vector<std::pair<std::string, std::string>> relativeCandidates = {
        {canonicalUpper, "canonical ILBM folder"},
        {canonicalLower, "canonical ILBM folder"},
        {"Texture/" + setLooseReplaceExtension(assetPath, ".PNG"), "legacy Texture folder"},
        {"Texture/" + setLooseReplaceExtension(assetPath, ".png"), "legacy Texture folder"},
        {setLooseReplaceExtension(assetPath, ".PNG"), "legacy Loose root"},
        {setLooseReplaceExtension(assetPath, ".png"), "legacy Loose root"}
    };

    std::set<std::string> seen;
    for (const auto &candidate : relativeCandidates)
    {
        const std::string fullPath = looseRoot + candidate.first;
        const std::string key = setLooseLower(setLooseNormalizeSlashes(fullPath));
        if ( !seen.insert(key).second )
            continue;

        std::string openPath;
        if ( setLooseResolveReadableFile(fullPath, &openPath) )
        {
            if (out)
            {
                out->active = true;
                out->setId = setId;
                out->requested = assetPath;
                out->resolvedPath = openPath;
                out->extensionForm = "PNG texture override (" + candidate.second + ")";
                out->embedded = true;
                out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_embed::LoadingFromIFF";
                out->emrs = true;
                out->emrsClass = className;
                out->embeddedPayload = payload;
            }
            return true;
        }
    }

    return false;
}

bool IFFile::FindSetHiEffectPngOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    if (out)
        *out = SetLooseOverride();

    if ( !setLooseIsReadMode(mode) )
        return false;

    int32_t setId = setLooseCurrentSetId();
    if ( !setId )
        return false;

    std::string assetPath = setLooseTrimResourceName(setLooseStripLeadingSlashes(setLooseNormalizeSlashes(filename)));
    if ( assetPath.empty() || assetPath.find(':') != std::string::npos )
        return false;

    std::string assetLower = setLooseLower(assetPath);
    if ( assetLower.compare(0, 3, "hi/") != 0 )
        return false;

    std::string assetBase = setLooseFileName(assetPath);
    std::string assetBaseLower = setLooseLower(assetBase);
    if ( assetBaseLower != "fx1.ilbm" && assetBaseLower != "fx1.ilb" &&
         assetBaseLower != "fx2.ilbm" && assetBaseLower != "fx2.ilb" &&
         assetBaseLower != "fx3.ilbm" && assetBaseLower != "fx3.ilb" )
        return false;

    std::string setRoot = "Data/Set" + std::to_string(setId) + "/";
    std::vector<std::string> candidates;
    candidates.push_back(setRoot + setLooseReplaceExtension(assetPath, ".PNG"));

    std::string lowerCandidate = setRoot + setLooseReplaceExtension(assetPath, ".png");
    if ( setLooseNormalizeSlashes(lowerCandidate) != setLooseNormalizeSlashes(candidates.front()) )
        candidates.push_back(lowerCandidate);

    for (const std::string &candidate : candidates)
    {
        std::string openPath;
        if ( setLooseResolveReadableFile(candidate, &openPath) )
        {
            if (out)
            {
                out->active = true;
                out->setId = setId;
                out->requested = assetPath;
                out->resolvedPath = openPath;
                out->extensionForm = "HI PNG effect override";
                out->vanillaPath = setLooseNormalizeSlashes(uaSetDirectoryResolvedReadPath("rsrc:" + assetPath));
                out->embedded = false;
                out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_ilbm::rsrc_func64";
            }
            return true;
        }
    }

    return false;
}

bool IFFile::FindSet46RootPngOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    if (out)
        *out = SetLooseOverride();

    if ( !setLooseIsReadMode(mode) )
        return false;

    std::string assetPath = set46RootAssetFromRequest(filename);
    assetPath = setLooseTrimResourceName(setLooseStripLeadingSlashes(setLooseNormalizeSlashes(assetPath)));
    if ( !set46RootSupportedPngOverrideAsset(assetPath) )
        return false;

    std::vector<std::string> candidates;
    candidates.push_back("Data/Set46/" + setLooseReplaceExtension(assetPath, ".PNG"));

    std::string lowerCandidate = "Data/Set46/" + setLooseReplaceExtension(assetPath, ".png");
    if ( setLooseNormalizeSlashes(lowerCandidate) != setLooseNormalizeSlashes(candidates.front()) )
        candidates.push_back(lowerCandidate);

    for (const std::string &candidate : candidates)
    {
        std::string openPath;
        if ( setLooseResolveReadableFile(candidate, &openPath) )
        {
            if (out)
            {
                out->active = true;
                out->setId = 46;
                out->requested = assetPath;
                out->resolvedPath = openPath;
                out->extensionForm = "SET46 PNG GUI override";
                out->vanillaPath = setLooseNormalizeSlashes(uaSetDirectoryResolvedReadPath("rsrc:" + assetPath));
                out->embedded = false;
                out->sourceFunction = sourceFunction ? sourceFunction : "NC_STACK_ilbm::rsrc_func64";
            }
            return true;
        }
    }

    return false;
}

// PNG fallback for textures of an external BASE ("Data/..." path). The original ILBM beside the
// BASE always wins; the PNG is used only when that ILBM does not exist.
bool IFFile::FindExternalBasePngFallback(const std::string &filename, std::string *outPath)
{
    if (outPath)
        outPath->clear();

    if (!outPath || !IsExternalBaseResourceScopeActive())
        return false;

    std::string localIlbm;
    if (externalBaseResolveLocalResource(filename, "rb", &localIlbm))
        return false;

    const std::string pngName = setLooseReplaceExtension(setLooseNormalizeSlashes(filename), ".PNG");
    return externalBaseResolveLocalResource(pngName, "rb", outPath);
}

FSMgr::FileHandle IFFile::UAOpenFileWithSetLooseOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    SetLooseOverride overrideInfo;


    std::string externalBaseLocalPath;
    if ( externalBaseResolveLocalResource(filename, mode, &externalBaseLocalPath) )
    {
        FSMgr::FileHandle fil = FSMgr::iDir::openFile(externalBaseLocalPath, mode);
        if ( fil.OK() )
        {
            if (out)
                *out = SetLooseOverride();
            return fil;
        }
    }

    std::string skyArchiveOverridePath;
    if ( FindSkyLooseArchiveOverride(filename, mode, &skyArchiveOverridePath, sourceFunction) )
    {
        FSMgr::FileHandle fil = FSMgr::iDir::openFile(skyArchiveOverridePath, mode);
        if ( fil.OK() )
        {
            overrideInfo.active = true;
            overrideInfo.setId = 0;
            overrideInfo.requested = filename;
            overrideInfo.resolvedPath = skyArchiveOverridePath;
            overrideInfo.extensionForm = "Data/Objects sky loose archive override";
            overrideInfo.vanillaPath = setLooseNormalizeSlashes(correctSeparatorAndExt(Common::Env.ApplyPrefix(filename)));
            overrideInfo.sourceFunction = sourceFunction ? sourceFunction : "IFFile::UAOpenFileWithSetLooseOverride";

            if (out)
                *out = overrideInfo;

            return fil;
        }

        ypa_log_out("WARNING: OpenNeoUA sky loose archive override failed to open: %s; vanilla Data/Objects fallback used.\n", skyArchiveOverridePath.c_str());
    }

    if ( FindSetLooseOverride(filename, mode, &overrideInfo, sourceFunction) )
    {
        FSMgr::FileHandle fil = FSMgr::iDir::openFile(overrideInfo.resolvedPath, mode);
        if ( fil.OK() )
        {
            if (out)
                *out = overrideInfo;

            return fil;
        }

        setLooseLogFailed(overrideInfo, "loose file existed but failed to load; vanilla fallback used.");
    }

    if (out)
        *out = SetLooseOverride();

    return UAOpenFileVanilla(filename, mode);
}

FSMgr::FileHandle IFFile::UAOpenFileWithSetLooseEmbeddedOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    SetLooseOverride overrideInfo;

    if ( FindSetLooseEmbeddedOverride(filename, mode, &overrideInfo, sourceFunction) )
    {
        FSMgr::FileHandle fil = FSMgr::iDir::openFile(overrideInfo.resolvedPath, mode);
        if ( fil.OK() )
        {
            if (out)
                *out = overrideInfo;

            return fil;
        }

        setLooseLogFailed(overrideInfo, "loose embedded override existed but failed to open; embedded SET.BAS fallback used.");
    }

    if (out)
        *out = SetLooseOverride();

    return FSMgr::FileHandle();
}

FSMgr::FileHandle IFFile::UAOpenFileWithSetLooseEmrsOverride(const std::string &filename, const std::string &mode, const std::string &className, const std::string &payload, SetLooseOverride *out, const char *sourceFunction, size_t currentOffset)
{
    SetLooseOverride overrideInfo;

    if ( FindSetLooseEmrsOverride(filename, mode, className, payload, &overrideInfo, sourceFunction, currentOffset) )
    {
        FSMgr::FileHandle fil = FSMgr::iDir::openFile(overrideInfo.resolvedPath, mode);
        if ( fil.OK() )
        {
            if (out)
                *out = overrideInfo;

            return fil;
        }

        setLooseLogFailed(overrideInfo, "loose EMRS override existed but failed to open; embedded payload fallback used.");
    }

    if (out)
        *out = SetLooseOverride();

    return FSMgr::FileHandle();
}

IFFile IFFile::UAOpenIFFile(const std::string &filename, const std::string &mode, const char *sourceFunction)
{
    SetLooseOverride overrideInfo;
    FSMgr::FileHandle fil = UAOpenFileWithSetLooseOverride(filename, mode, &overrideInfo, sourceFunction ? sourceFunction : "IFFile::UAOpenIFFile");

    if ( !fil.OK() )
        return IFFile();

    IFFile result(std::move(fil));
    result._setLooseOverride = overrideInfo;
    return result;
}

IFFile IFFile::UAOpenIFFileWithSetLooseEmbeddedOverride(const std::string &filename, const std::string &mode, SetLooseOverride *out, const char *sourceFunction)
{
    SetLooseOverride overrideInfo;
    FSMgr::FileHandle fil = UAOpenFileWithSetLooseEmbeddedOverride(filename, mode, &overrideInfo, sourceFunction);

    if ( !fil.OK() )
        return IFFile();

    IFFile result(std::move(fil));
    result._setLooseOverride = overrideInfo;

    if (out)
        *out = overrideInfo;

    return result;
}

void IFFile::BeginSetLooseBaseObjectScope()
{
    g_setLooseBaseObjectScopeDepth++;
}

void IFFile::EndSetLooseBaseObjectScope()
{
    if ( g_setLooseBaseObjectScopeDepth > 0 )
        g_setLooseBaseObjectScopeDepth--;
}

bool IFFile::IsSetLooseBaseObjectScopeActive()
{
    return g_setLooseBaseObjectScopeDepth > 0;
}

void IFFile::BeginExternalBaseResourceScope(const std::string &baseFilePath)
{
    const std::string root = externalBaseParentPath(baseFilePath);
    if (!root.empty())
        g_externalBaseResourceRoots.push_back(root);
}

void IFFile::EndExternalBaseResourceScope()
{
    if (!g_externalBaseResourceRoots.empty())
        g_externalBaseResourceRoots.pop_back();
}

bool IFFile::IsExternalBaseResourceScopeActive()
{
    return !g_externalBaseResourceRoots.empty();
}

bool IFFile::IsSetLooseOverride() const
{
    return _setLooseOverride.active;
}

void IFFile::ReportSetLooseOverrideUsed() const
{
    setLooseLogUsed(_setLooseOverride);
}

void IFFile::ReportSetLooseOverrideFailed(const std::string &reason) const
{
    setLooseLogFailed(_setLooseOverride, reason);
}

void IFFile::ReportSetLooseOverrideUsed(const SetLooseOverride &overrideInfo)
{
    setLooseLogUsed(overrideInfo);
}

void IFFile::ReportSetLooseOverrideFailed(const SetLooseOverride &overrideInfo, const std::string &reason)
{
    setLooseLogFailed(overrideInfo, reason);
}

std::string IFFile::ChunkLabel(const Context &chunk)
{
    return setLooseChunkLabel(chunk);
}

int IFFile::pushChunk(uint32_t TAG1, uint32_t TAG2, int32_t TAG_SZ)
{
    int32_t position = 0;

    if ( file_handle.IsWriting() )
    {
        Context &currentCtx = ctxStack.front();

        if ( currentCtx.TAG != TAG_FORM )
            return IFF_ERR_SYNTAX;

        if ( !file_handle.writeU32B(TAG2) )
            return IFF_ERR_WRITE;

        if ( !file_handle.writeU32B(TAG_SZ) )
            return IFF_ERR_WRITE;

        if ( TAG2 == TAG_FORM )
        {
            if ( !file_handle.writeU32B(TAG1) )
                return IFF_ERR_WRITE;

            position = 4;
        }
        else
        {
            TAG1 = currentCtx.TAG_EXTENSION;
        }
    }
    else                                          // READING
    {
        Context &currentCtx = ctxStack.front();

        if ( currentCtx.TAG != TAG_FORM )
            return IFF_ERR_EOC;

        if ( currentCtx.TAG_SIZE == currentCtx.position )
            return IFF_ERR_EOC;

        TAG2 = file_handle.readU32B();

        TAG_SZ = file_handle.readU32B();

        TAG1 = currentCtx.TAG_EXTENSION;

        if ( TAG2 == TAG_FORM )
        {
            TAG1 = file_handle.readU32B();

            position = 4;
        }
    }

    ctxStack.emplace_front(TAG2, TAG1, TAG_SZ, position);

    depth++;

    return IFF_ERR_OK;
}

int IFFile::popChunk()
{
    Context &ctx = ctxStack.front();
    int TAG_SZ = ctx.TAG_SIZE;

    if ( ctx.TAG == TAG_FORM && ctx.TAG_EXTENSION == TAG_NONE )
        return IFF_ERR_SYNTAX;

    if ( file_handle.IsWriting() )
    {
        if ( TAG_SZ == -1 ) // UNKNOWN SIZE
        {
            TAG_SZ = ctx.position;

            if ( file_handle.seek(-(ctx.position + 4), SEEK_CUR) ) // seek for write TAG SIZE
                return IFF_ERR_SEEK;

            if ( !file_handle.writeU32B(ctx.position) )
                return IFF_ERR_WRITE;

            if ( file_handle.seek(ctx.position, SEEK_CUR) )
                return IFF_ERR_SEEK;
        }

        if ( ctx.position < TAG_SZ ) // if we not in the end
            return IFF_ERR_CORRUPT;

        if ( TAG_SZ & 1 ) // pad 1 byte
        {
            if ( !file_handle.writeU8(0) )
                return -IFF_ERR_WRITE;

            TAG_SZ++;
        }

        ctxStack.pop_front();

        ctxStack.front().position += TAG_SZ + 8;

        depth--;
    }
    else
    {
        if ( ctx.TAG != TAG_FORM )
        {
            if ( ctx.position < TAG_SZ && file_handle.seek(TAG_SZ -  ctx.position, SEEK_CUR) )
                return IFF_ERR_SEEK;

            if ( TAG_SZ & 1 ) // pad 1 byte
            {
                if ( file_handle.seek(1, SEEK_CUR) )
                    return IFF_ERR_SEEK;

                TAG_SZ++;
            }
        }

        ctxStack.pop_front();

        Context &ctx2 = ctxStack.front();
        if ( ctx2.TAG == TAG_FORM && ctx2.TAG_EXTENSION == TAG_NONE )
            return IFF_ERR_EOF;

        ctx2.position += TAG_SZ + 8;
        depth--;
    }

    return IFF_ERR_OK;
}

int IFFile::parse()
{
    int res = IFF_ERR_SYNTAX;

    if (_flagPop)
    {
        res = popChunk();
        _flagPop = false;

        if (res != IFF_ERR_OK)
            return res;
    }

    res = pushChunk(TAG_NONE, TAG_NONE, -1);
    if ( res == IFF_ERR_EOC )
        _flagPop = true;

    return res;
}

bool IFFile::skipChunk()
{
    while ( 1 )
    {
        int res = parse();

        if (res == IFF_ERR_EOC)
            break;
        else if ( res )
            return false;

        if ( !skipChunk() )
            return false;
    }

    return true;
}

size_t IFFile::read(void *buf, size_t sz)
{
    Context &ctx = ctxStack.front();

    if ( ctx.Is(TAG_FORM, TAG_NONE) )
        return IFF_ERR_SYNTAX;

    if ( ctx.TAG == TAG_FORM )
        return IFF_ERR_SYNTAX;

    if ( sz + ctx.position > (size_t)ctx.TAG_SIZE )
    {
        if (ctx.TAG_SIZE == ctx.position)
            return 0;

        sz = ctx.TAG_SIZE - ctx.position;
    }

    ctx.position += sz;

    size_t readed = file_handle.read(buf, sz);

    if ( readed != sz )
        return IFF_ERR_READ;

    return readed;
}

size_t IFFile::write(const void *buf, size_t sz)
{
    Context &ctx = ctxStack.front();

    if ( ctx.Is(TAG_FORM, TAG_NONE) )
        return IFF_ERR_SYNTAX;

    if ( ctx.TAG == TAG_FORM )
        return IFF_ERR_SYNTAX;

    if ( ctx.TAG_SIZE != -1 &&
            ctx.position + sz > (size_t)ctx.TAG_SIZE)
    {
        if ( ctx.TAG_SIZE == ctx.position )
            return 0;

        sz = ctx.TAG_SIZE - ctx.position;
    }

    ctx.position += sz;
    size_t writed = file_handle.write(buf, sz);

    if ( writed != sz )
        return IFF_ERR_WRITE;

    return writed;
}

const IFFile::Context &IFFile::GetCurrentChunk()
{
    return ctxStack.front();
}

std::string IFFile::readStr(int maxSz)
{
    char *bf = new char[maxSz]();
    std::string tmp(bf, read(bf, maxSz));
    delete[] bf;
    return tmp;
}

bool IFFile::eof() const
{
    return file_handle.eof();
}

bool IFFile::OK() const
{
    return file_handle.OK();
}

size_t IFFile::tell() const
{
    return file_handle.tell();
}

int IFFile::seek(long int offset, int origin)
{
    return 0;
}

void IFFile::close()
{
    file_handle.close();
}
