#include <algorithm>
#include <cmath>
#include <limits>
#include "3ds.h"

#include "image.h"
#include "env.h"
#include "system/inivals.h"
#include "utils.h"

NC_STACK_3ds::NC_STACK_3ds()
{}

NC_STACK_3ds::~NC_STACK_3ds()
{}

size_t NC_STACK_3ds::Init(IDVList &stak)
{
    if ( !NC_STACK_base::Init(stak) )
        return 0;

    return 1;
}

size_t NC_STACK_3ds::Deinit()
{
    return NC_STACK_base::Deinit();
}

bool NC_STACK_3ds::LoadFromFile(const std::string &filename)
{
    FSMgr::FileHandle fil = uaOpenFile(filename, "rb");

    if (!fil.OK())
        return false;

    std::string normalized = filename;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const size_t slash = normalized.find_last_of('/');
    _sourceDir = slash == std::string::npos ? std::string() : normalized.substr(0, slash);

    return LoadFromFile(&fil);
}

bool NC_STACK_3ds::LoadFromFile(FSMgr::FileHandle *fil)
{
    // A loader instance owns one parsed asset. Do not replace live resources.
    if (!fil || !fil->OK() || _skeleton || !materials.empty())
        return false;
    const size_t start = fil->tell();
    if (fil->seek(0, SEEK_END) != 0)
        return false;
    _fileEnd = fil->tell();
    if (fil->seek(start, SEEK_SET) != 0 || !requireBytes(fil, 6))
        return false;
    if (fil->readU16L() != 0x4D4D)
        return false;
    const size_t mainSize = fil->readU32L();
    if (mainSize < 6 || mainSize > _fileEnd - start)
        return false;
    _legacyNameBudget = _fileEnd - start - mainSize;
    size_t datSz = mainSize - 6;
    size_t readed = 0;

    while (!_parseError && readed < datSz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, datSz, readed, tag, tagsz))
            break;
        switch(tag)
        {
        case 0x3D3D: // Editor
            readed += readChunkEditor(fil, tagsz);
            break;
        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }
    if (_parseError || fil->readErr() || readed != datSz ||
        fil->tell() != start + mainSize + _legacyNameBytes ||
        (_legacyNameLengths && _legacyNameBytes != _legacyNameBudget))
        return false;

    // FaceMaterial may precede its Material, even in another Editor block.
    for (size_t i = 0; i < _faceMaterialNames.size(); ++i)
        faceMaterial[i] = findMaterial(_faceMaterialNames[i]);
    std::vector<std::string>().swap(_faceMaterialNames);

    RecalcInternal();
    MakeCoordsCache();

    return true;
}

NC_STACK_3ds *NC_STACK_3ds::Load3DS(const std::string &filename)
{
    NC_STACK_3ds *tmp = Nucleus::CInit<NC_STACK_3ds>();
    if (!tmp)
        return NULL;
    if(tmp->LoadFromFile(filename))
        return tmp;

    tmp->Delete();
    return NULL;
}

size_t NC_STACK_3ds::readChunkEditor(FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        switch(tag)
        {
        case 0x4000: //Trimesh
            readed += readChunkObject(fil, tagsz);
            break;

        case 0xAFFF: //Material
            readed += readChunkMaterial(fil, tagsz);
            break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    return readed;
}

size_t NC_STACK_3ds::readChunkObject(FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    std::string objectName;
    readed += readName(fil, &objectName, sz, false, false);

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        switch(tag)
        {
        case 0x4100: //Trimesh
            readed += readChunkTrimesh(fil, tagsz);
            break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    return readed;
}

size_t NC_STACK_3ds::readChunkTrimesh(FSMgr::FileHandle *fil, size_t sz)
{
    if (!_skeleton)
        _skeleton = Nucleus::CInit<NC_STACK_skeleton>({
            {NC_STACK_rsrc::RSRC_ATT_NAME, std::string("3ds_sklt")},
            {NC_STACK_rsrc::RSRC_ATT_TRYSHARED, (int32_t)2}});
    if (!_skeleton)
    {
        _parseError = true;
        return 0;
    }
    UAskeleton::Data *dat = _skeleton->GetSkelet();
    _meshVertexStart = dat->POO.size();
    _meshFaceStart = dat->polygons.size();
    _meshVertices = _meshFaces = _meshUV = false;
    size_t readed = 0;

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        switch(tag)
        {
        case 0x4110: //VertexL
            readed += readChunkVertex(fil, tagsz);
            break;

        case 0x4120: //Faces
            readed += readChunkFaces(fil, tagsz);
            break;

        case 0x4140: //Vertex coords
        {
            if (_meshUV || tagsz < 2 || !requireBytes(fil, 2))
            { _parseError = true; return 0; }
            _meshUV = true;
            uint16_t coNum = fil->readU16L();
            if (tagsz != 2 + size_t(coNum) * 8 || !requireBytes(fil, size_t(coNum) * 8))
            { _parseError = true; return 0; }
            texCoords.resize(_meshVertexStart + coNum);
            for (size_t i = _meshVertexStart; i < texCoords.size(); ++i)
            {
                tUtV &uv = texCoords[i];
                uv.tu = fil->readFloatL();
                uv.tv = fil->readFloatL();
                if (!std::isfinite(uv.tu) || !std::isfinite(uv.tv))
                    _parseError = true;
            }
            readed += tagsz;
        }
        break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    if (_parseError)
        return 0;
    if ((_meshFaces && !_meshVertices) || (_meshUV && texCoords.size() != dat->POO.size()))
    { _parseError = true; return 0; }
    texCoords.resize(dat->POO.size()); // Missing UVs remain unused, not synthesized.
    const size_t vertexCount = dat->POO.size() - _meshVertexStart;
    for (size_t i = _meshFaceStart; i < dat->polygons.size(); ++i)
    {
        _faceHasUV[i] = _meshUV;
        for (int j = 0; j < 3; ++j)
        {
            int32_t &idx = dat->polygons[i].v[j];
            if (idx < 0 || size_t(idx) >= vertexCount)
            { _parseError = true; return 0; }
            idx += _meshVertexStart;
        }
    }
    return readed;
}

size_t NC_STACK_3ds::readChunkVertex(FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    if (_meshVertices || sz < 2 || !requireBytes(fil, 2))
    { _parseError = true; return 0; }
    _meshVertices = true;
    uint16_t numvertex = fil->readU16L();
    readed += 2;


    if (sz != 2 + size_t(numvertex) * 12 || !requireBytes(fil, size_t(numvertex) * 12) ||
        _meshVertexStart + numvertex > size_t(std::numeric_limits<int32_t>::max()))
    { _parseError = true; return 0; }
    UAskeleton::Data *dat = _skeleton->GetSkelet();
    dat->POO.resize(_meshVertexStart + numvertex);

    for (int i = 0; i < numvertex; i++)
    {
        dat->POO[_meshVertexStart + i].x = fil->readFloatL();
        dat->POO[_meshVertexStart + i].z = fil->readFloatL();
        dat->POO[_meshVertexStart + i].y = -fil->readFloatL();
        dat->POO[_meshVertexStart + i].flags = 0;

        const auto &v = dat->POO[_meshVertexStart + i];
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
            _parseError = true;
        readed += 12;
    }

    return readed;
}

size_t NC_STACK_3ds::readChunkFaces(FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    if (_meshFaces || sz < 2 || !requireBytes(fil, 2))
    { _parseError = true; return 0; }
    _meshFaces = true;
    uint16_t numfaces = fil->readU16L();
    readed += 2;

    UAskeleton::Data *dat = _skeleton->GetSkelet();

    if (size_t(numfaces) * 8 > sz - readed || !requireBytes(fil, size_t(numfaces) * 8))
    { _parseError = true; return 0; }
    dat->polygons.resize(_meshFaceStart + numfaces);
    _faceMaterialNames.resize(dat->polygons.size());
    _faceHasUV.resize(dat->polygons.size(), false);

    faceMaterial.resize(dat->polygons.size());

    for (int i = 0; i < numfaces; i++)
    {
        dat->polygons[_meshFaceStart + i].num_vertices = 3;
        dat->polygons[_meshFaceStart + i].v[2] = fil->readU16L();
        dat->polygons[_meshFaceStart + i].v[1] = fil->readU16L();
        dat->polygons[_meshFaceStart + i].v[0] = fil->readU16L();

        //uint16_t diff = fil->readU16L();
        fil->readU16L();

        dat->polygons[_meshFaceStart + i].A = 0;
        dat->polygons[_meshFaceStart + i].B = 0;
        dat->polygons[_meshFaceStart + i].C = 0;
        dat->polygons[_meshFaceStart + i].D = 0;

        faceMaterial[_meshFaceStart + i] = NULL;

        readed += 8;
    }

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        switch(tag)
        {
        case 0x4130: //FaceMaterial
        {
            std::string matName;
            if (tagsz < 3)
            { _parseError = true; return 0; }
            size_t nameSize = readName(fil, &matName, tagsz - 2);
            if (_parseError || nameSize > tagsz - 2 || !requireBytes(fil, 2))
            { _parseError = true; return 0; }
            readed += nameSize;
            uint16_t n = fil->readU16L();
            if (tagsz - nameSize - 2 != size_t(n) * 2 || !requireBytes(fil, size_t(n) * 2))
            { _parseError = true; return 0; }
            readed += 2;

            for (int i = 0; i < n; i++)
            {
                uint16_t idx = fil->readU16L();
                readed += 2;

                if (idx >= numfaces)
                { _parseError = true; return 0; }
                _faceMaterialNames[_meshFaceStart + idx] = matName;
            }
        }
        break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    faceNum = dat->polygons.size();

    return readed;
}

size_t NC_STACK_3ds::readChunkMaterial(FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    materials.emplace_back();
    d3dsMaterial &mat = materials.back();

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        switch(tag)
        {
        case 0xA000: //Mat name
        {
            std::string matName;
            const size_t used = readName(fil, &matName, tagsz, true);
            if (_parseError || used != tagsz)
            { _parseError = true; return 0; }
            readed += used;

            mat.name = matName;
        }
        break;

        case 0xA020: //Diffuse color
            readed += readChunkColor(mat.diffuse, fil, tagsz);
            break;

        case 0xA200: //Tex map
            readed += readChunkTexMap(mat.texture1_map, fil, tagsz);
            break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    return readed;
}

size_t NC_STACK_3ds::readChunkTexMap(d3dsTextureMap &texmap, FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        switch(tag)
        {
        case 0xA300: //Mat name
        {
            std::string texName;
            const size_t used = readName(fil, &texName, tagsz, true);
            if (_parseError || used != tagsz)
            { _parseError = true; return 0; }
            readed += used;

            std::string resolvedTexName = texName;
            std::replace(resolvedTexName.begin(), resolvedTexName.end(), '\\', '/');

            if ( !_sourceDir.empty() && !resolvedTexName.empty() )
            {
                bool resolvedLocalTexture = false;

                // First preserve the existing relative-path behaviour, e.g.
                // Textures/missile.png -> <3ds dir>/Textures/missile.png.
                const bool rootedTexturePath = resolvedTexName[0] == '/' ||
                                               resolvedTexName.find(':') != std::string::npos;
                if ( !rootedTexturePath )
                {
                    const std::string localCandidate = _sourceDir + "/" + resolvedTexName;
                    if ( uaFileExist(localCandidate) )
                    {
                        resolvedTexName = localCandidate;
                        resolvedLocalTexture = true;
                    }
                }

                // Legacy exporters often store an absolute authoring-machine
                // path in A300. If the relative candidate above was not found,
                // also try the basename next to the 3DS before falling back to
                // the original authored name/path.
                if ( !resolvedLocalTexture )
                {
                    std::string normalizedTexName = texName;
                    std::replace(normalizedTexName.begin(), normalizedTexName.end(), '\\', '/');
                    const size_t separator = normalizedTexName.find_last_of("/:");
                    const std::string textureBaseName = separator == std::string::npos
                                                      ? normalizedTexName
                                                      : normalizedTexName.substr(separator + 1);

                    if ( !textureBaseName.empty() )
                    {
                        const std::string siblingCandidate = _sourceDir + "/" + textureBaseName;
                        if ( uaFileExist(siblingCandidate) )
                            resolvedTexName = siblingCandidate;
                    }
                }
            }

            texmap.name = resolvedTexName;

            if (!texmap.tex)
            {
                // External 3DS assets are authored under Data. Resolve material
                // textures from the mesh directory first, then keep the exact old
                // resource-name fallback when no sibling texture exists.
                std::string oldprefix = Common::Env.SetPrefix("rsrc", "");

                texmap.tex = Nucleus::CInit<NC_STACK_image>( {
                    {NC_STACK_rsrc::RSRC_ATT_NAME, resolvedTexName},
                    {NC_STACK_rsrc::RSRC_ATT_TRYSHARED, (int32_t)2},
                    {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}} );

                Common::Env.SetPrefix("rsrc", oldprefix);
            }
        }
        break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    return readed;
}

size_t NC_STACK_3ds::readChunkColor(float colors[3], FSMgr::FileHandle *fil, size_t sz)
{
    size_t readed = 0;

    bool hasLin = false;

    while (!_parseError && readed < sz)
    {
        uint16_t tag;
        uint32_t tagsz;
        if (!readChunkHeader(fil, sz, readed, tag, tagsz))
            break;

        if (((tag == 0x0011 || tag == 0x0012) && tagsz != 3) ||
            ((tag == 0x0010 || tag == 0x0013) && tagsz != 12))
        { _parseError = true; return 0; }

        switch(tag)
        {
        case 0x0012: //LIN_COLOR_24
        {
            for (int i = 0; i < 3; i++)
                colors[i] = fil->readU8() / 255.0;

            readed += 3;
            hasLin = true;
        }
        break;

        case 0x0011: //COLOR_24
            if (!hasLin)
            {
                for (int i = 0; i < 3; i++)
                    colors[i] = fil->readU8() / 255.0;
                readed += 3;
            }
            else
            {
                fil->seek(tagsz, SEEK_CUR);
                readed += tagsz;
            }
            break;

        case 0x0013: //LIN_COLOR_F
        {
            for (int i = 0; i < 3; i++)
                colors[i] = fil->readFloatL();

            readed += 3 * 4;
            hasLin = true;
        }
        break;

        case 0x0010: //COLOR_F
            if (!hasLin)
            {
                for (int i = 0; i < 3; i++)
                    colors[i] = fil->readFloatL();
                readed += 3 * 4;
            }
            else
            {
                fil->seek(tagsz, SEEK_CUR);
                readed += tagsz;
            }
            break;

        default:
            fil->seek(tagsz, SEEK_CUR);
            readed += tagsz;
            break;
        }
    }

    return readed;
}

bool NC_STACK_3ds::requireBytes(FSMgr::FileHandle *fil, size_t count)
{
    if (_parseError || fil->readErr() || fil->tell() > _fileEnd || count > _fileEnd - fil->tell())
    {
        _parseError = true;
        return false;
    }
    return true;
}

bool NC_STACK_3ds::readChunkHeader(FSMgr::FileHandle *fil, size_t size, size_t &readed,
                                 uint16_t &tag, uint32_t &payload)
{
    if (readed > size || size - readed < 6 || !requireBytes(fil, 6))
    { _parseError = true; return false; }
    tag = fil->readU16L();
    const uint32_t length = fil->readU32L();
    readed += 6;
    if (length < 6 || length - 6 > size - readed || !requireBytes(fil, length - 6))
    { _parseError = true; return false; }
    payload = length - 6;
    return true;
}

size_t NC_STACK_3ds::readName(FSMgr::FileHandle *fil, std::string *dst, size_t maxn,
                            bool nameOnly, bool legacyName)
{
    dst->clear();
    size_t used = 0;
    bool terminated = false;
    while (used < maxn && requireBytes(fil, 1))
    {
        const char c = fil->readS8();
        ++used;
        if (!c) { terminated = true; break; }
        *dst += c;
    }
    // The shipped test missile omits these NUL bytes from name chunks and
    // all ancestor sizes. Accept only that measurable legacy convention:
    // a NUL exactly one byte past a name-only payload, paid for by the
    // physical bytes beyond Main. Standard files never enter this mode.
    if (!terminated && nameOnly && legacyName && _legacyNameBudget > _legacyNameBytes &&
        requireBytes(fil, 1) && fil->readU8() == 0)
    {
        ++used;
        terminated = true;
        _legacyNameLengths = true;
    }
    if (!terminated)
    { _parseError = true; return 0; }
    if (_legacyNameLengths && legacyName)
    {
        if (++_legacyNameBytes > _legacyNameBudget)
        { _parseError = true; return 0; }
        --used;
    }
    return used;
}

d3dsMaterial * NC_STACK_3ds::findMaterial(const std::string &matName)
{
    for (d3dsMaterial &mat : materials)
    {
        if ( StriCmp(mat.name, matName) == 0 )
            return &mat;
    }
    return NULL;
}

void NC_STACK_3ds::RecalcInternal(bool kids)
{
    NC_STACK_base::RecalcInternal(kids);

    if ( _skeleton )
    {
        UAskeleton::Data *skeldat = _skeleton->GetSkelet();

        for(size_t i = 0; i < faceNum; ++i)
        {
            GFX::TRenderParams mat;
            if (faceMaterial[i])
                mat = GenRenderParams(faceMaterial[i]);

            if (!_faceHasUV[i])
            {
                mat.Flags &= ~(GFX::RFLAGS_TEXTURED | GFX::RFLAGS_DYNAMIC_TEXTURE);
                mat.TexSource = nullptr;
                mat.Tex = nullptr;
            }

            GFX::TMesh *msh = NC_STACK_base::FindMeshByRenderParams(&Meshes, mat);

            if (!msh)
            {
                Meshes.emplace_back();
                msh = &Meshes.back();
                msh->Mat = mat;
            }

            UAskeleton::Polygon &pol = skeldat->polygons[ i ];

            uint32_t fid = msh->Vertexes.size();

            if (pol.num_vertices >= 3)
            {
                for(int j = 0; j < pol.num_vertices; ++j)
                {
                    msh->Vertexes.emplace_back();
                    msh->Vertexes.back().Pos = skeldat->POO[ pol.v[j] ];
                    msh->Vertexes.back().TexCoordId = j;
                    msh->Vertexes.back().Color = mat.Color;

                    msh->BoundBox.Add( skeldat->POO[ pol.v[j] ] );

                    if (_faceHasUV[i])
                        msh->Vertexes.back().TexCoord = texCoords.at(pol.v[j]);
                }

                for(int j = 2; j < pol.num_vertices; ++j)
                {
                    msh->Indixes.push_back(fid + 0);
                    msh->Indixes.push_back(fid + j);
                    msh->Indixes.push_back(fid + j - 1);
                }
            }
        }
    }
}

GFX::TRenderParams NC_STACK_3ds::GenRenderParams(d3dsMaterial *mat)
{
    GFX::TRenderParams tmp;

    if (!mat)
        return tmp;

    tmp.Flags |= GFX::RFLAGS_SHADED;

    if (mat->texture1_map.tex)
    {
        mat->texture1_map.tex->PrepareTexture();
        tmp.Flags |= GFX::RFLAGS_TEXTURED;
        tmp.TexSource = mat->texture1_map.tex;

        if (mat->texture1_map.tex->IsDynamic())
            tmp.Flags |= GFX::RFLAGS_DYNAMIC_TEXTURE;
        else
            tmp.Tex = mat->texture1_map.tex->GetBitmap();
    }

    tmp.Color = GFX::TGLColor(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 1.0 - mat->transparency);
    return tmp;
}
