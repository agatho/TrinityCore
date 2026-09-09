/*
 * BlpReader - BLP2 decoder.
 *
 * BLP2 layout (little-endian):
 *   0x00  magic 'BLP2'
 *   0x04  version       (uint32, expected 1)
 *   0x08  colorEncoding (uint8: 1 = Pal8, 2 = DXT, 3 = ARGB8888)
 *   0x09  alphaDepth    (uint8: 0/1/4/8)
 *   0x0A  alphaEncoding (uint8: 0 = DXT1, 1 = DXT3, 7 = DXT5; for DXT only)
 *   0x0B  hasMips       (uint8)
 *   0x0C  width         (uint32)
 *   0x10  height        (uint32)
 *   0x14  mipOffsets[16]
 *   0x54  mipSizes[16]
 *   0x94  for Pal8: 256 BGRA palette entries (1024 bytes), then mip data.
 *
 * Only mip 0 is decoded.  The DXT block decoders are written by hand
 * (no S3TC / libsquish dep).  All output is top-to-bottom RGBA8.
 */

#include "BlpReader.h"

#include <algorithm>
#include <cstring>

namespace world_editor::io
{

namespace
{

constexpr uint32_t BLP2_MAGIC = 0x32504C42u; // 'BLP2' little-endian

uint32_t readU32(uint8_t const* p)
{
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

uint16_t readU16(uint8_t const* p)
{
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

// Expand a 5-6-5 packed 16-bit color into 8-bit RGB.
void unpack565(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b)
{
    uint32_t const r5 = (c >> 11) & 0x1F;
    uint32_t const g6 = (c >> 5) & 0x3F;
    uint32_t const b5 = c & 0x1F;
    r = uint8_t((r5 << 3) | (r5 >> 2));
    g = uint8_t((g6 << 2) | (g6 >> 4));
    b = uint8_t((b5 << 3) | (b5 >> 2));
}

// Decode a single DXT1 block (8 bytes) into a 4x4 RGBA tile.
// dxt1Alpha=true honors the 1-bit punch-through alpha when c0 <= c1.
void decodeDxt1Block(uint8_t const* src, uint8_t* dstRgba, int dstPitchBytes, bool dxt1Alpha)
{
    uint16_t const c0 = readU16(src + 0);
    uint16_t const c1 = readU16(src + 2);
    uint32_t const bits = readU32(src + 4);

    uint8_t r[4], g[4], b[4], a[4];
    unpack565(c0, r[0], g[0], b[0]);
    unpack565(c1, r[1], g[1], b[1]);
    a[0] = a[1] = 0xFF;

    if (c0 > c1)
    {
        // Four-color block.
        r[2] = uint8_t((2 * r[0] + r[1] + 1) / 3);
        g[2] = uint8_t((2 * g[0] + g[1] + 1) / 3);
        b[2] = uint8_t((2 * b[0] + b[1] + 1) / 3);
        a[2] = 0xFF;
        r[3] = uint8_t((r[0] + 2 * r[1] + 1) / 3);
        g[3] = uint8_t((g[0] + 2 * g[1] + 1) / 3);
        b[3] = uint8_t((b[0] + 2 * b[1] + 1) / 3);
        a[3] = 0xFF;
    }
    else
    {
        // Three-color + transparent.
        r[2] = uint8_t((r[0] + r[1]) / 2);
        g[2] = uint8_t((g[0] + g[1]) / 2);
        b[2] = uint8_t((b[0] + b[1]) / 2);
        a[2] = 0xFF;
        r[3] = 0;
        g[3] = 0;
        b[3] = 0;
        a[3] = dxt1Alpha ? 0x00 : 0xFF;
    }

    for (int row = 0; row < 4; ++row)
    {
        uint8_t* dst = dstRgba + row * dstPitchBytes;
        for (int col = 0; col < 4; ++col)
        {
            uint32_t const idx = (bits >> (2 * (4 * row + col))) & 0x3u;
            dst[0] = r[idx];
            dst[1] = g[idx];
            dst[2] = b[idx];
            dst[3] = a[idx];
            dst += 4;
        }
    }
}

// Decode the 8-byte DXT3 alpha sub-block (explicit 4-bit alphas).
void decodeDxt3Alpha(uint8_t const* src, uint8_t* dstRgba, int dstPitchBytes)
{
    for (int row = 0; row < 4; ++row)
    {
        uint16_t const word = readU16(src + row * 2);
        uint8_t* dst = dstRgba + row * dstPitchBytes + 3;
        for (int col = 0; col < 4; ++col)
        {
            uint32_t const a4 = (word >> (4 * col)) & 0xFu;
            *dst = uint8_t((a4 << 4) | a4);
            dst += 4;
        }
    }
}

// Decode the 8-byte DXT5 alpha sub-block (2 endpoints + 3-bit indices).
void decodeDxt5Alpha(uint8_t const* src, uint8_t* dstRgba, int dstPitchBytes)
{
    uint8_t const a0 = src[0];
    uint8_t const a1 = src[1];
    uint8_t alpha[8];
    alpha[0] = a0;
    alpha[1] = a1;
    if (a0 > a1)
    {
        for (int i = 1; i <= 6; ++i)
            alpha[i + 1] = uint8_t(((7 - i) * a0 + i * a1 + 3) / 7);
    }
    else
    {
        for (int i = 1; i <= 4; ++i)
            alpha[i + 1] = uint8_t(((5 - i) * a0 + i * a1 + 2) / 5);
        alpha[6] = 0x00;
        alpha[7] = 0xFF;
    }
    // 48-bit index table packed into bytes 2..7.
    uint64_t indices = 0;
    for (int i = 0; i < 6; ++i)
        indices |= uint64_t(src[2 + i]) << (8 * i);
    for (int row = 0; row < 4; ++row)
    {
        uint8_t* dst = dstRgba + row * dstPitchBytes + 3;
        for (int col = 0; col < 4; ++col)
        {
            uint32_t const idx = uint32_t((indices >> (3 * (4 * row + col))) & 0x7u);
            *dst = alpha[idx];
            dst += 4;
        }
    }
}

bool decodeDxt(uint8_t const* mip, std::size_t mipSize, int w, int h,
               uint8_t subtype, std::vector<uint8_t>& outRgba)
{
    int const blocksX = (w + 3) / 4;
    int const blocksY = (h + 3) / 4;
    std::size_t const stride = (subtype == 0) ? 8u : 16u;
    std::size_t const needed = std::size_t(blocksX) * blocksY * stride;
    if (mipSize < needed)
        return false;

    outRgba.assign(std::size_t(w) * h * 4, 0);
    int const pitch = w * 4;

    for (int by = 0; by < blocksY; ++by)
    {
        for (int bx = 0; bx < blocksX; ++bx)
        {
            uint8_t const* block = mip + (by * blocksX + bx) * stride;
            // The color block in DXT3/5 follows the 8-byte alpha block.
            uint8_t const* colorBlock = (subtype == 0) ? block : block + 8;

            // Decode into a temporary 4x4 RGBA tile, then blit the clipped
            // region into the output (handles non-multiple-of-4 dims).
            uint8_t tile[4 * 4 * 4];
            // For DXT3/5 the color sub-block has no punch-through alpha.
            decodeDxt1Block(colorBlock, tile, 16, /*dxt1Alpha=*/ subtype == 0);
            if (subtype == 1)
                decodeDxt3Alpha(block, tile, 16);
            else if (subtype == 7)
                decodeDxt5Alpha(block, tile, 16);

            int const x0 = bx * 4;
            int const y0 = by * 4;
            int const cw = std::min(4, w - x0);
            int const ch = std::min(4, h - y0);
            for (int row = 0; row < ch; ++row)
            {
                std::memcpy(outRgba.data() + (y0 + row) * pitch + x0 * 4,
                            tile + row * 16,
                            std::size_t(cw) * 4);
            }
        }
    }
    return true;
}

bool decodePal8(uint8_t const* palette, uint8_t const* mip, std::size_t mipSize,
                int w, int h, uint8_t alphaDepth, std::vector<uint8_t>& outRgba)
{
    std::size_t const pixels = std::size_t(w) * h;
    if (mipSize < pixels)
        return false;
    outRgba.assign(pixels * 4, 0);

    // Alpha bitstream follows the index bytes (LSB-first per byte).
    uint8_t const* alphaStream = mip + pixels;
    std::size_t alphaBytes = 0;
    switch (alphaDepth)
    {
        case 0: alphaBytes = 0; break;
        case 1: alphaBytes = (pixels + 7) / 8; break;
        case 4: alphaBytes = (pixels + 1) / 2; break;
        case 8: alphaBytes = pixels; break;
        default: return false;
    }
    if (mipSize < pixels + alphaBytes)
        return false;

    for (std::size_t i = 0; i < pixels; ++i)
    {
        uint8_t const idx = mip[i];
        uint8_t const* pal = palette + std::size_t(idx) * 4;
        // BLP palette is BGRA on disk.
        outRgba[i * 4 + 0] = pal[2];
        outRgba[i * 4 + 1] = pal[1];
        outRgba[i * 4 + 2] = pal[0];
        uint8_t a = 0xFF;
        if (alphaDepth == 1)
            a = ((alphaStream[i >> 3] >> (i & 7)) & 1) ? 0xFF : 0x00;
        else if (alphaDepth == 4)
        {
            uint8_t const byte = alphaStream[i >> 1];
            uint8_t const nibble = (i & 1) ? (byte >> 4) : (byte & 0x0F);
            a = uint8_t((nibble << 4) | nibble);
        }
        else if (alphaDepth == 8)
            a = alphaStream[i];
        outRgba[i * 4 + 3] = a;
    }
    return true;
}

bool decodeArgb8(uint8_t const* mip, std::size_t mipSize, int w, int h,
                 std::vector<uint8_t>& outRgba)
{
    std::size_t const pixels = std::size_t(w) * h;
    if (mipSize < pixels * 4)
        return false;
    outRgba.assign(pixels * 4, 0);
    // BLP ARGB8888 mip is stored as BGRA bytes per pixel.
    for (std::size_t i = 0; i < pixels; ++i)
    {
        outRgba[i * 4 + 0] = mip[i * 4 + 2];
        outRgba[i * 4 + 1] = mip[i * 4 + 1];
        outRgba[i * 4 + 2] = mip[i * 4 + 0];
        outRgba[i * 4 + 3] = mip[i * 4 + 3];
    }
    return true;
}

} // namespace

bool decodeBlp(std::vector<uint8_t> const& blob, BlpImage& out)
{
    constexpr std::size_t kHeaderSize = 0x94;
    if (blob.size() < kHeaderSize)
        return false;
    uint8_t const* p = blob.data();
    if (readU32(p) != BLP2_MAGIC)
        return false;
    uint8_t const colorEncoding = p[0x08];
    uint8_t const alphaDepth    = p[0x09];
    uint8_t const alphaEncoding = p[0x0A];
    uint32_t const width  = readU32(p + 0x0C);
    uint32_t const height = readU32(p + 0x10);
    if (width == 0 || height == 0 || width > 16384 || height > 16384)
        return false;

    uint32_t const mip0Off  = readU32(p + 0x14);
    uint32_t const mip0Size = readU32(p + 0x54);
    if (mip0Off == 0 || mip0Size == 0)
        return false;
    if (std::size_t(mip0Off) + mip0Size > blob.size())
        return false;

    out.width  = int(width);
    out.height = int(height);

    uint8_t const* mip = p + mip0Off;

    switch (colorEncoding)
    {
        case 1: // Pal8 - 256-entry BGRA palette (1024 bytes) directly after the header at 0x94.
            if (blob.size() < kHeaderSize + 1024)
                return false;
            return decodePal8(p + kHeaderSize, mip, mip0Size,
                              int(width), int(height), alphaDepth, out.rgba);
        case 2: // DXT - subtype carried in alphaEncoding.
            if (alphaEncoding != 0 && alphaEncoding != 1 && alphaEncoding != 7)
                return false;
            return decodeDxt(mip, mip0Size, int(width), int(height),
                             alphaEncoding, out.rgba);
        case 3: // ARGB8888 raw.
            return decodeArgb8(mip, mip0Size, int(width), int(height), out.rgba);
        default:
            return false;
    }
}

} // namespace world_editor::io
