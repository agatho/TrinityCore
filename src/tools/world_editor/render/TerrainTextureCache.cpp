/*
 * TerrainTextureCache - implementation.
 */

#include "TerrainTextureCache.h"

#include "../io/BlpReader.h"
#include "../io/CascClient.h"

#include <vector>

namespace world_editor::render
{

TerrainTextureCache::TerrainTextureCache(io::CascClient* casc)
    : m_casc(casc)
{
}

GLuint TerrainTextureCache::uploadFromBlob(std::vector<uint8_t> const& blob,
                                           QOpenGLFunctions& gl)
{
    io::BlpImage img;
    if (!io::decodeBlp(blob, img))
        return 0;
    return uploadFromImage(img, gl);
}

GLuint TerrainTextureCache::uploadFromImage(io::BlpImage const& img,
                                            QOpenGLFunctions& gl)
{
    if (img.width <= 0 || img.height <= 0
        || img.rgba.size() < size_t(img.width) * size_t(img.height) * 4)
        return 0;

    GLuint tex = 0;
    gl.glGenTextures(1, &tex);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    // Tileable terrain textures: GL_REPEAT so the same BLP can wrap
    // across multiple chunks if the renderer ever scales UV up.  v1
    // uses chunk-local [0..1] UVs so REPEAT is a no-op there but
    // future-proofs the cache.
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    img.width, img.height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    gl.glGenerateMipmap(GL_TEXTURE_2D);
    gl.glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint TerrainTextureCache::textureForFileDataId(uint32_t fdid,
                                                 QOpenGLFunctions& gl)
{
    if (fdid == 0 || !m_casc || !m_casc->isOpen())
        return 0;
    if (auto it = m_byFdid.find(fdid); it != m_byFdid.end())
        return it->second;
    std::vector<uint8_t> blob;
    GLuint tex = 0;
    if (m_casc->readByFileDataId(fdid, blob) && !blob.empty())
        tex = uploadFromBlob(blob, gl);
    m_byFdid[fdid] = tex;  // cache zero too, so we don't retry
    return tex;
}

GLuint TerrainTextureCache::textureForPath(std::string const& vpath,
                                           QOpenGLFunctions& gl)
{
    if (vpath.empty() || !m_casc || !m_casc->isOpen())
        return 0;
    if (auto it = m_byPath.find(vpath); it != m_byPath.end())
        return it->second;
    std::vector<uint8_t> blob;
    GLuint tex = 0;
    if (m_casc->readByPath(vpath, blob) && !blob.empty())
        tex = uploadFromBlob(blob, gl);
    m_byPath[vpath] = tex;
    return tex;
}

GLuint TerrainTextureCache::insertDecodedFdid(uint32_t fdid,
                                              io::BlpImage const& img,
                                              QOpenGLFunctions& gl)
{
    if (fdid == 0)
        return 0;
    if (auto it = m_byFdid.find(fdid); it != m_byFdid.end())
        return it->second;
    GLuint const tex = uploadFromImage(img, gl);
    m_byFdid[fdid] = tex;  // cache zero too, so we don't retry
    return tex;
}

GLuint TerrainTextureCache::insertDecodedPath(std::string const& vpath,
                                              io::BlpImage const& img,
                                              QOpenGLFunctions& gl)
{
    if (vpath.empty())
        return 0;
    if (auto it = m_byPath.find(vpath); it != m_byPath.end())
        return it->second;
    GLuint const tex = uploadFromImage(img, gl);
    m_byPath[vpath] = tex;
    return tex;
}

std::unordered_set<uint32_t> TerrainTextureCache::knownFdidKeys() const
{
    std::unordered_set<uint32_t> keys;
    keys.reserve(m_byFdid.size());
    for (auto const& [fdid, _] : m_byFdid)
        keys.insert(fdid);
    return keys;
}

std::unordered_set<std::string> TerrainTextureCache::knownPathKeys() const
{
    std::unordered_set<std::string> keys;
    keys.reserve(m_byPath.size());
    for (auto const& [path, _] : m_byPath)
        keys.insert(path);
    return keys;
}

void TerrainTextureCache::clear(QOpenGLFunctions& gl)
{
    for (auto& [_, tex] : m_byFdid)
        if (tex != 0) gl.glDeleteTextures(1, &tex);
    for (auto& [_, tex] : m_byPath)
        if (tex != 0) gl.glDeleteTextures(1, &tex);
    m_byFdid.clear();
    m_byPath.clear();
}

} // namespace world_editor::render
