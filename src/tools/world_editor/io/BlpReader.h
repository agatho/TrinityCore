/*
 * BlpReader - standalone BLP2 decoder for the world_editor.
 *
 * Decodes mip level 0 of a BLP2 blob into a top-to-bottom RGBA8 buffer.
 * Supports the three encodings the modern WoW client ships:
 *   - Encoding 1 (Pal8)   with alpha depths 0 / 1 / 4 / 8.
 *   - Encoding 2 (DXT)    subtypes 0 (DXT1), 1 (DXT3), 7 (DXT5).
 *   - Encoding 3 (ARGB8)  raw BGRA, swizzled to RGBA.
 *
 * Intentionally self-contained: no Qt, no CASC, no TC core headers.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace world_editor::io
{

struct BlpImage
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

// Decode mip 0 of a BLP2 blob.  Returns false on malformed input or
// unsupported encoding/subtype.
[[nodiscard]] bool decodeBlp(std::vector<uint8_t> const& blob, BlpImage& out);

} // namespace world_editor::io
