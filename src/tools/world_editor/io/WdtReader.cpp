/*
 * WdtReader - implementation.  See header for the why.
 *
 * Layout cribbed from wow.export's WDTLoader.js + TC's map_extractor wdt.h:
 *   MPHD: 32 bytes -- 8 uint32 (flags + 7 FDIDs we don't need here).
 *   MAIN: 64*64 entries of (flag:u32, asyncId:u32).  Row-major outer y.
 *   MAID: 64*64 entries of 8 uint32 FDIDs (root, obj0, obj1, tex0, lod,
 *         mapTexture, mapTextureN, minimapTexture).  Same row-major layout.
 *
 * On-disk MAID layout: Blizzard writes outer y, inner x, so byte offset
 * = (y * 64 + x) * sizeof(WdtMaidEntry).  This matches both wow.export's
 * `entries[y * MAP_SIZE + x]` access and TC map_extractor's
 * `_adtFileDataIds->Data[y][x]`.  Our editor's (gx, gy) = (row, col)
 * maps to (y, x).
 */

#include "WdtReader.h"

#include "CascClient.h"

#include <QDebug>

#include <cstdint>
#include <cstring>
#include <vector>

namespace world_editor::io
{

namespace
{

constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
{
    // ADTs/WDTs store chunk magics in big-endian on disk -- bytes ABCD
    // appear as 'ABCD' in a hex dump.  Memory layout is the reverse on
    // little-endian hosts (DCBA), so reading via memcpy into a uint32_t
    // gives 'DCBA' -- match that here.
    return (uint32_t(uint8_t(d)) << 24)
         | (uint32_t(uint8_t(c)) << 16)
         | (uint32_t(uint8_t(b)) << 8)
         | (uint32_t(uint8_t(a)));
}

// Chunk magics are stored REVERSED on disk in WoW's chunked formats
// (same convention as ADT MCNK -> 'KNCM' on disk).  MakeFourCC builds
// a little-endian u32 whose byte-0 is the FIRST argument, so to match
// memcpy'd disk bytes we pass the magic in REVERSE ASCII order: MPHD
// is stored as bytes 'D','H','P','M' -> MakeFourCC('D','H','P','M').
constexpr uint32_t kFccMPHD = MakeFourCC('D', 'H', 'P', 'M');
constexpr uint32_t kFccMAIN = MakeFourCC('N', 'I', 'A', 'M');
constexpr uint32_t kFccMAID = MakeFourCC('D', 'I', 'A', 'M');

// Walk top-level chunks invoking visitor(magic, body, bodySize).  Stops
// when visitor returns false.
template <typename V>
void ForEachTopLevelChunk(uint8_t const* data, std::size_t size, V&& visitor)
{
    std::size_t pos = 0;
    while (pos + 8 <= size)
    {
        uint32_t magic;
        uint32_t bodyLen;
        std::memcpy(&magic, data + pos, 4);
        std::memcpy(&bodyLen, data + pos + 4, 4);
        std::size_t const bodyStart = pos + 8;
        if (bodyLen > size || bodyStart + bodyLen > size)
            return;
        if (!visitor(magic, data + bodyStart, std::size_t(bodyLen)))
            return;
        pos = bodyStart + bodyLen;
    }
}

} // namespace

bool loadWdt(CascClient& casc, std::string const& mapDir, Wdt& out)
{
    out = Wdt{};

    if (mapDir.empty() || !casc.isOpen())
        return false;

    std::string const path = "world/maps/" + mapDir + "/" + mapDir + ".wdt";
    std::vector<uint8_t> bytes;
    if (!casc.readByPath(path, bytes) || bytes.empty())
        return false;

    bool mphdSeen = false;
    bool mainSeen = false;
    int  chunkCount = 0;
    // Capture the first N magics as diag so we can see whether the WDT
    // is structured the way we expect (or whether it's MVER-prefixed,
    // empty, or in an unknown layout).
    std::string magicTrace;
    magicTrace.reserve(64);

    ForEachTopLevelChunk(bytes.data(), bytes.size(),
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize) -> bool
        {
            ++chunkCount;
            if (chunkCount <= 16)
            {
                // Magic stored as little-endian u32; bytes-on-disk order
                // is its low-byte first.  Decode back to ASCII.
                char asc[5] = {
                    char((magic      ) & 0xff),
                    char((magic >>  8) & 0xff),
                    char((magic >> 16) & 0xff),
                    char((magic >> 24) & 0xff),
                    '\0'
                };
                if (!magicTrace.empty()) magicTrace += ',';
                magicTrace += asc;
                magicTrace += '(';
                magicTrace += std::to_string(bodySize);
                magicTrace += ')';
            }
            if (magic == kFccMPHD)
            {
                // First uint32 of MPHD is the flags word; everything
                // after that is the seven LGT/OCC/etc FDIDs which we
                // don't need to surface in the editor.
                if (bodySize >= sizeof(uint32_t))
                {
                    std::memcpy(&out.mphdFlags, body, sizeof(uint32_t));
                    mphdSeen = true;
                }
                return true;
            }
            if (magic == kFccMAIN)
            {
                // Each entry is (flag:u32, asyncId:u32) = 8 bytes.  We
                // only care about flag bit 0x1 (= "tile exists in
                // world").  Order is row-major: outer y, inner x.
                constexpr std::size_t kEntrySize = 8;
                std::size_t const cells = bodySize / kEntrySize;
                std::size_t const cap = std::min<std::size_t>(cells, Wdt::kGridSize * Wdt::kGridSize);
                for (std::size_t i = 0; i < cap; ++i)
                {
                    uint32_t flag;
                    std::memcpy(&flag, body + i * kEntrySize, sizeof(uint32_t));
                    out.tileExists[i] = (flag & 0x1u) != 0;
                }
                mainSeen = true;
                return true;
            }
            if (magic == kFccMAID)
            {
                // MAID layout: 64*64 entries, each 8 uint32 = 32 bytes.
                // Field order on disk:
                //   0: rootADT
                //   1: obj0ADT
                //   2: obj1ADT
                //   3: tex0ADT
                //   4: lodADT
                //   5: mapTexture
                //   6: mapTextureN
                //   7: minimapTexture
                constexpr std::size_t kEntrySize = sizeof(uint32_t) * 8;
                static_assert(kEntrySize == 32, "MAID entry must be 8 uint32s");
                std::size_t const cells = bodySize / kEntrySize;
                std::size_t const cap = std::min<std::size_t>(cells, Wdt::kGridSize * Wdt::kGridSize);
                for (std::size_t i = 0; i < cap; ++i)
                {
                    uint8_t const* p = body + i * kEntrySize;
                    WdtMaidEntry& e = out.maid[i];
                    std::memcpy(&e.rootADT,        p +  0, 4);
                    std::memcpy(&e.obj0ADT,        p +  4, 4);
                    std::memcpy(&e.obj1ADT,        p +  8, 4);
                    std::memcpy(&e.tex0ADT,        p + 12, 4);
                    std::memcpy(&e.lodADT,         p + 16, 4);
                    std::memcpy(&e.mapTexture,     p + 20, 4);
                    std::memcpy(&e.mapTextureN,    p + 24, 4);
                    std::memcpy(&e.minimapTexture, p + 28, 4);
                }
                return true;
            }
            return true;
        });

    qInfo("[wdt] '%s' bytes=%zu chunks=%d mphdSeen=%d mainSeen=%d trace=[%s]",
        path.c_str(), bytes.size(), chunkCount, mphdSeen ? 1 : 0,
        mainSeen ? 1 : 0, magicTrace.c_str());

    // A "good" WDT must at minimum have MPHD + MAIN.  MAID is optional
    // (Classic-era maps without MAID return rootADT=0 and the caller
    // falls back to listfile resolution).
    return mphdSeen && mainSeen;
}

} // namespace world_editor::io
