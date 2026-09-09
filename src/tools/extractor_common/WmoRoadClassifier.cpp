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

#include "WmoRoadClassifier.h"

#include "ListfileMap.h"
#include "RoadClassifier.h"

#include <cstring>

namespace Road::WmoRoad
{
    bool IsTriangleRoad(uint8 materialId,
                        std::span<WmoMaterialTexture const> materials)
    {
        // MPY2 / MOPY use materialId = 0xFF to indicate "no collision /
        // hint triangle". Such triangles are non-walkable and don't get
        // road-tagged.
        if (materialId == 0xFFu)
            return false;
        if (materialId >= materials.size())
            return false;
        // WMO materials live under a broader texture vocabulary than
        // ADTs (world/wmo/, world/azeroth/, dungeons/textures/), so we
        // use the scope-relaxed classifier here.
        return Road::IsRoadTexturePathForWmo(materials[materialId].textureBlp);
    }

    std::vector<uint8> ClassifyMaterials(
        std::span<WmoMaterialTexture const> materials)
    {
        std::vector<uint8> bitmap;
        bitmap.resize((materials.size() + 7) / 8, 0);
        for (std::size_t i = 0; i < materials.size(); ++i)
            if (Road::IsRoadTexturePathForWmo(materials[i].textureBlp))
                SetMaterialBit(bitmap, i);
        return bitmap;
    }

    std::vector<WmoMaterialTexture> ResolveFromMotx(
        std::span<uint8 const> motxBytes,
        std::span<uint32 const> motmTexture1Offsets)
    {
        std::vector<WmoMaterialTexture> out;
        out.reserve(motmTexture1Offsets.size());
        for (uint32 offset : motmTexture1Offsets)
        {
            WmoMaterialTexture m;
            if (offset < motxBytes.size())
            {
                // null-terminated string at the offset
                std::size_t end = offset;
                while (end < motxBytes.size() && motxBytes[end] != 0)
                    ++end;
                if (end > offset)
                    m.textureBlp.assign(
                        reinterpret_cast<char const*>(motxBytes.data() + offset),
                        end - offset);
            }
            out.push_back(std::move(m));
        }
        return out;
    }

    std::vector<WmoMaterialTexture> ResolveFromModi(
        std::span<uint32 const> modi,
        ListfileMap const* listfile)
    {
        std::vector<WmoMaterialTexture> out;
        out.reserve(modi.size());
        for (uint32 fdid : modi)
        {
            WmoMaterialTexture m;
            if (fdid != 0)
            {
                if (listfile)
                {
                    if (auto resolved = listfile->Lookup(fdid))
                        m.textureBlp = std::string(*resolved);
                    else
                        m.textureBlp = "[FDID:" + std::to_string(fdid) + "]";
                }
                else
                {
                    m.textureBlp = "[FDID:" + std::to_string(fdid) + "]";
                }
            }
            out.push_back(std::move(m));
        }
        return out;
    }
}
