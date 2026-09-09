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

#include "../io/BlpReader.h"

#include <QOpenGLFunctions>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

    // PERF: insert a texture the worker thread already CASC-read + BLP-decoded,
    // so the GL thread only uploads RGBA bytes.  When the key is already cached
    // (two in-flight tiles decoded the same texture) the payload is discarded
    // and the existing handle returned.  Caller's GL context MUST be current.
    GLuint insertDecodedFdid(uint32_t fdid, io::BlpImage const& img, QOpenGLFunctions& gl);
    GLuint insertDecodedPath(std::string const& vpath, io::BlpImage const& img, QOpenGLFunctions& gl);

    // Snapshots of the cached key sets, copied into worker tasks at dispatch
    // time so workers can skip decoding textures the GL thread already holds.
    // Purely an optimization hint: staleness only costs a duplicate decode
    // (discarded by insertDecoded*), never correctness -- the textureFor*
    // lookups keep their synchronous CASC fallback.  GL thread only.
    [[nodiscard]] std::unordered_set<uint32_t>    knownFdidKeys() const;
    [[nodiscard]] std::unordered_set<std::string> knownPathKeys() const;

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
    GLuint uploadFromImage(io::BlpImage const& img, QOpenGLFunctions& gl);

    io::CascClient*                              m_casc = nullptr;  // borrowed
    std::unordered_map<uint32_t, GLuint>         m_byFdid;
    std::unordered_map<std::string, GLuint>      m_byPath;
};

} // namespace world_editor::render
