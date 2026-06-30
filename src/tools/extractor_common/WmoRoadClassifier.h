/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_WMO_ROAD_CLASSIFIER_H
#define TRINITYCORE_WMO_ROAD_CLASSIFIER_H

#include "Define.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Road
{
    class ListfileMap;
}

namespace Road::WmoRoad
{
    // One row of the WMO MOMT chunk (32 bytes on disk, but we only need
    // the texture1 field for road classification). The other fields
    // are flags, shader, blend mode, colors, etc. — irrelevant here.
    //
    // In legacy WMOs the texture1 field is an offset into the MOTX string
    // table. In modern (Legion+) WMOs MOMT uses FileDataID directly via
    // the MODI/MOTX-replaced layout — caller resolves via listfile.
    struct WmoMaterialTexture
    {
        // Resolved BLP path (lowercased / normalized). May be empty if
        // the material has no usable texture reference.
        std::string textureBlp;
    };

    // Per-triangle road verdict produced from a single MPY2/MOPY entry +
    // the WMO's material→texture table.
    //
    // Input: triangle's materialId byte (the high byte of an MPY2 entry,
    // or the second byte of an MOPY entry); plus the resolved material
    // table.
    //
    // Output: true if the material's texture path is a road texture.
    bool IsTriangleRoad(uint8 materialId,
                        std::span<WmoMaterialTexture const> materials);

    // -------------------------------------------------------------------
    // Material-table classifier — bulk version. Given the resolved texture
    // path for every material in a WMO, return a bitmap (one bit per
    // material) indicating road materials.
    //
    // The bitmap is encoded as a vector<uint8>: bit (i % 8) of byte (i / 8)
    // is set iff material i is a road material. Compact (1 bit/material)
    // because WMOs typically have <256 materials and we serialize this
    // into the per-WMO road sidecar.
    // -------------------------------------------------------------------
    std::vector<uint8> ClassifyMaterials(
        std::span<WmoMaterialTexture const> materials);

    inline bool MaterialBitSet(std::span<uint8 const> bitmap, std::size_t idx)
    {
        std::size_t byteIdx = idx / 8;
        if (byteIdx >= bitmap.size())
            return false;
        return (bitmap[byteIdx] >> (idx % 8)) & 1u;
    }

    inline void SetMaterialBit(std::vector<uint8>& bitmap, std::size_t idx)
    {
        std::size_t byteIdx = idx / 8;
        if (byteIdx >= bitmap.size())
            bitmap.resize(byteIdx + 1, 0);
        bitmap[byteIdx] |= (uint8(1) << (idx % 8));
    }

    // -------------------------------------------------------------------
    // Texture path resolution from raw MOTX/MOMT/MODI inputs.
    //
    // For LEGACY (MOTX present): MOMT.texture1 is a byte offset into the
    // null-terminated string blob; this helper resolves them.
    //
    // For MODERN (no MOTX, MODI present): MODI has one FileDataID per
    // material slot — we resolve via the provided ListfileMap.
    //
    // Returns vector<WmoMaterialTexture> of size materialCount.
    // -------------------------------------------------------------------

    // Inputs to legacy resolution: raw MOTX bytes + the MOMT entry's
    // texture1 offset (uint32). The materialCount drives output size.
    std::vector<WmoMaterialTexture> ResolveFromMotx(
        std::span<uint8 const> motxBytes,
        std::span<uint32 const> motmTexture1Offsets);

    // Modern resolution: MODI is one uint32 (FileDataID) per material.
    // listfile may be nullptr (then we emit `[FDID:N]` placeholders).
    std::vector<WmoMaterialTexture> ResolveFromModi(
        std::span<uint32 const> modi,
        ListfileMap const* listfile);
}

#endif // TRINITYCORE_WMO_ROAD_CLASSIFIER_H
