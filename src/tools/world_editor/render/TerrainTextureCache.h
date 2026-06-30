/*
 * TerrainTextureCache - on-demand BLP terrain-texture upload for the
 * realistic-pass ADT renderer.
 *
 * Resolves a terrain BLP (typically tileable 256x256 / 512x512) by its
 * FileDataID or virtual path, decodes via BlpReader, and uploads to a
 * GL texture handle.  Cached for the lifetime of the cache; eviction
 * is intentionally absent in the editor v1 -- a typical continent
 * touches ~200 unique terrain textures totalling <100 MB after upload.
 *
 * Borrowed handle: callers must NOT glDeleteTextures the returned GLuint
 * (the cache owns it).  `clear()` releases everything when the GL
 * context can still bind, e.g. when the realistic pipeline tears down.
 */

#pragma once

#include <QOpenGLFunctions>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace world_editor::io { class CascClient; }

namespace world_editor::render
{

class TerrainTextureCache
{
public:
    explicit TerrainTextureCache(io::CascClient* casc);

    // Look up by FileDataID (Legion+ MDID).  Returns a borrowed GL
    // texture handle, or 0 when the file is missing / decode fails.
    // Caller's GL context MUST be current.
    GLuint textureForFileDataId(uint32_t fdid, QOpenGLFunctions& gl);

    // Look up by virtual path (legacy MTEX strings, e.g.
    // "Tileset\Elwynn\Elwynn_Grass01.blp").  Same semantics.
    GLuint textureForPath(std::string const& vpath, QOpenGLFunctions& gl);

    // Drop and glDeleteTextures every cached entry.  Caller's GL
    // context MUST be current.
    void clear(QOpenGLFunctions& gl);

    // True when at least one texture is currently held.
    [[nodiscard]] bool empty() const noexcept
    {
        return m_byFdid.empty() && m_byPath.empty();
    }

private:
    GLuint uploadFromBlob(std::vector<uint8_t> const& blob, QOpenGLFunctions& gl);

    io::CascClient*                              m_casc = nullptr;  // borrowed
    std::unordered_map<uint32_t, GLuint>         m_byFdid;
    std::unordered_map<std::string, GLuint>      m_byPath;
};

} // namespace world_editor::render
