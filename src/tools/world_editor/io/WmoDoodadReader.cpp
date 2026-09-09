/*
 * WmoDoodadReader - implementation.
 *
 * Mirrors src/tools/vmap4_extractor/wmo.cpp's MOHD/MODS/MODN/MODI/MODD walk
 * and the world-transform math in src/tools/vmap4_extractor/model.cpp's
 * Doodad::ExtractSet, with the final position+rotation re-projected into the
 * editor's TC world frame (X=north, Y=west, Z=up) the same way
 * AdtDoodadReader does it for MDDF entries.
 */

#include "WmoDoodadReader.h"

#include "CascClient.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace world_editor::io
{

namespace
{

constexpr float kPi        = 3.14159265358979323846f;
constexpr float kDegToRad  = kPi / 180.0f;
constexpr float kRadToDeg  = 180.0f / kPi;
constexpr float kClientMid = 17066.66656f;   // (32 tiles * 533.33333) - client origin shift.

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) noexcept
{
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

constexpr uint32_t kFccMOHD = MakeFourCC('M', 'O', 'H', 'D');
constexpr uint32_t kFccMODS = MakeFourCC('M', 'O', 'D', 'S');
constexpr uint32_t kFccMODN = MakeFourCC('M', 'O', 'D', 'N');
constexpr uint32_t kFccMODI = MakeFourCC('M', 'O', 'D', 'I');
constexpr uint32_t kFccMODD = MakeFourCC('M', 'O', 'D', 'D');

#pragma pack(push, 1)

// WMO MODS - 32 bytes.  Matches src/tools/vmap4_extractor/wmo.h:49.
struct ModsEntry
{
    char     name[20];
    uint32_t startIndex;
    uint32_t count;
    char     pad[4];
};
static_assert(sizeof(ModsEntry) == 32, "ModsEntry layout drift");

// WMO MODD - 40 bytes.  Matches src/tools/vmap4_extractor/wmo.h:57:
//   uint32 NameIndex : 24;  <- low 24 bits of first uint32; top 8 are flags.
//   Vec3D  Position (12)
//   Quaternion(X,Y,Z,W) (16)
//   float  Scale (4)
//   uint32 Color (4)
struct ModdEntry
{
    uint32_t nameIndexAndFlags;     // low 24 = name index, high 8 = flags.
    float    posX, posY, posZ;
    float    rotX, rotY, rotZ, rotW;
    float    scale;
    uint32_t color;
};
static_assert(sizeof(ModdEntry) == 40, "ModdEntry layout drift");

#pragma pack(pop)

struct ChunkHeader
{
    uint32_t magic;
    uint32_t size;
};

template<typename Cb>
void ForEachTopLevelChunk(uint8_t const* data, std::size_t size, Cb&& cb)
{
    std::size_t pos = 0;
    while (pos + sizeof(ChunkHeader) <= size)
    {
        auto const* hdr = reinterpret_cast<ChunkHeader const*>(data + pos);
        std::size_t const bodyOffset = pos + sizeof(ChunkHeader);
        std::size_t const bodyEnd    = bodyOffset + hdr->size;
        if (bodyEnd > size)
            break;
        if (!cb(hdr->magic, data + bodyOffset, hdr->size))
            return;
        pos = bodyEnd;
    }
}

// 3x3 row-major matrix and helpers.  Convention matches VmapReader.cpp:
// fromEulerAnglesZYX(yaw, pitch, roll) = Rz(yaw) * Ry(pitch) * Rx(roll).
struct Mat3
{
    float m[3][3];

    static Mat3 identity()
    {
        Mat3 r{};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0f;
        return r;
    }

    static Mat3 fromEulerAnglesZYX(float yaw, float pitch, float roll)
    {
        float const cy = std::cos(yaw),   sy = std::sin(yaw);
        float const cp = std::cos(pitch), sp = std::sin(pitch);
        float const cr = std::cos(roll),  sr = std::sin(roll);
        Mat3 r{};
        r.m[0][0] =  cy * cp;
        r.m[0][1] =  cy * sp * sr - sy * cr;
        r.m[0][2] =  cy * sp * cr + sy * sr;
        r.m[1][0] =  sy * cp;
        r.m[1][1] =  sy * sp * sr + cy * cr;
        r.m[1][2] =  sy * sp * cr - cy * sr;
        r.m[2][0] = -sp;
        r.m[2][1] =  cp * sr;
        r.m[2][2] =  cp * cr;
        return r;
    }

    // Build a rotation matrix from a unit quaternion (X, Y, Z, W).
    static Mat3 fromQuat(float qx, float qy, float qz, float qw)
    {
        float const xx = qx * qx, yy = qy * qy, zz = qz * qz;
        float const xy = qx * qy, xz = qx * qz, yz = qy * qz;
        float const wx = qw * qx, wy = qw * qy, wz = qw * qz;
        Mat3 r{};
        r.m[0][0] = 1.0f - 2.0f * (yy + zz);
        r.m[0][1] = 2.0f * (xy - wz);
        r.m[0][2] = 2.0f * (xz + wy);
        r.m[1][0] = 2.0f * (xy + wz);
        r.m[1][1] = 1.0f - 2.0f * (xx + zz);
        r.m[1][2] = 2.0f * (yz - wx);
        r.m[2][0] = 2.0f * (xz - wy);
        r.m[2][1] = 2.0f * (yz + wx);
        r.m[2][2] = 1.0f - 2.0f * (xx + yy);
        return r;
    }
};

inline Mat3 mul(Mat3 const& A, Mat3 const& B)
{
    Mat3 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.m[i][j] = A.m[i][0] * B.m[0][j] + A.m[i][1] * B.m[1][j] + A.m[i][2] * B.m[2][j];
    return r;
}

inline void mulVec(Mat3 const& M, float const v[3], float out[3])
{
    float const x = v[0], y = v[1], z = v[2];
    out[0] = M.m[0][0] * x + M.m[0][1] * y + M.m[0][2] * z;
    out[1] = M.m[1][0] * x + M.m[1][1] * y + M.m[1][2] * z;
    out[2] = M.m[2][0] * x + M.m[2][1] * y + M.m[2][2] * z;
}

// Decompose a rotation matrix into Euler angles applied as Rz(yaw) *
// Ry(pitch) * Rx(roll) - matches the renderer's per-instance model
// composition order (translate * Rz * Ry * Rx * scale).  Handles
// gimbal-lock at pitch == +/- pi/2 by collapsing roll into yaw.
void matToEulerZYX(Mat3 const& M, float& yaw, float& pitch, float& roll)
{
    // From the fromEulerAnglesZYX layout:
    //   m20 = -sin(pitch)
    //   m21 =  cos(pitch) * sin(roll)
    //   m22 =  cos(pitch) * cos(roll)
    //   m00 =  cos(pitch) * cos(yaw)
    //   m10 =  cos(pitch) * sin(yaw)
    float const sp = -M.m[2][0];
    if (sp >= 1.0f - 1e-6f)
    {
        // pitch == +pi/2 - gimbal lock: roll absorbed into yaw.
        pitch = 0.5f * kPi;
        roll  = 0.0f;
        yaw   = std::atan2(-M.m[0][1], M.m[1][1]);
        return;
    }
    if (sp <= -1.0f + 1e-6f)
    {
        pitch = -0.5f * kPi;
        roll  = 0.0f;
        yaw   = std::atan2(-M.m[0][1], M.m[1][1]);
        return;
    }
    pitch = std::asin(sp);
    yaw   = std::atan2(M.m[1][0], M.m[0][0]);
    roll  = std::atan2(M.m[2][1], M.m[2][2]);
}

} // namespace

bool loadWmoDoodads(CascClient& casc,
                    uint32_t wmoRootFileDataId,
                    uint32_t setIndex,
                    WmoPlacement const& placement,
                    std::vector<DoodadInstance>& out)
{
    if (wmoRootFileDataId == 0 || !casc.isOpen())
        return false;

    std::vector<uint8_t> bytes;
    if (!casc.readByFileDataId(wmoRootFileDataId, bytes) || bytes.empty())
        return false;

    // Walk the chunked WMO root.  We only need MOHD (count sanity) +
    // MODS/MODN/MODI/MODD; everything else (textures, materials, group
    // table, portals, lights, ...) is irrelevant for doodad placement.
    uint32_t nDoodadNames = 0, nDoodadDefs = 0, nDoodadSets = 0;
    bool sawMohd = false;

    uint8_t const* modsBody = nullptr; std::size_t modsSize = 0;
    uint8_t const* modnBody = nullptr; std::size_t modnSize = 0;
    uint8_t const* modiBody = nullptr; std::size_t modiSize = 0;
    uint8_t const* moddBody = nullptr; std::size_t moddSize = 0;

    ForEachTopLevelChunk(bytes.data(), bytes.size(),
        [&](uint32_t magic, uint8_t const* body, std::size_t bodySize)
        {
            if (magic == kFccMOHD && bodySize >= 9 * sizeof(uint32_t))
            {
                // MOHD layout: nTextures, nGroups, nPortals, nLights,
                // nDoodadNames, nDoodadDefs, nDoodadSets, color, RootWMOID, ...
                std::memcpy(&nDoodadNames, body + 4 * 4, 4);
                std::memcpy(&nDoodadDefs,  body + 5 * 4, 4);
                std::memcpy(&nDoodadSets,  body + 6 * 4, 4);
                sawMohd = true;
            }
            else if (magic == kFccMODS && modsBody == nullptr) { modsBody = body; modsSize = bodySize; }
            else if (magic == kFccMODN && modnBody == nullptr) { modnBody = body; modnSize = bodySize; }
            else if (magic == kFccMODI && modiBody == nullptr) { modiBody = body; modiSize = bodySize; }
            else if (magic == kFccMODD && moddBody == nullptr) { moddBody = body; moddSize = bodySize; }
            return true;
        });

    if (!sawMohd || nDoodadDefs == 0 || nDoodadSets == 0 || moddBody == nullptr || modsBody == nullptr)
        return true;     // valid WMO with no interior doodads.

    // Bound MODD/MODS by what the chunk actually carries: a corrupt WMO
    // can ship inconsistent counts and we don't want an OOB read.
    std::size_t const setCount  = std::min<std::size_t>(nDoodadSets, modsSize / sizeof(ModsEntry));
    std::size_t const spawnCount = std::min<std::size_t>(nDoodadDefs, moddSize / sizeof(ModdEntry));
    if (setCount == 0 || spawnCount == 0)
        return true;

    // Resolve modelFileDataId per nameIndex via MODI (modern) or MODN
    // (legacy: name-index is a byte offset into the cstring blob).  Both
    // are optional; we just emit whichever lookup is present and skip
    // doodads whose FDID resolves to 0 (the renderer requires a non-zero
    // FDID to bind the M2 mesh cache).
    auto resolveFdid = [&](uint32_t nameIndex) -> uint32_t
    {
        if (modiBody != nullptr && modiSize >= sizeof(uint32_t))
        {
            std::size_t const fdidCount = modiSize / sizeof(uint32_t);
            if (nameIndex < fdidCount)
            {
                uint32_t fdid = 0;
                std::memcpy(&fdid, modiBody + nameIndex * sizeof(uint32_t), sizeof(uint32_t));
                return fdid;
            }
        }
        return 0;     // legacy MODN-only WMOs: caller would need a path->FDID listfile; v1 skips.
    };

    auto resolvePath = [&](uint32_t nameIndex) -> std::string
    {
        if (modnBody == nullptr || nameIndex >= modnSize)
            return {};
        std::size_t end = nameIndex;
        while (end < modnSize && modnBody[end] != '\0')
            ++end;
        return std::string(reinterpret_cast<char const*>(modnBody + nameIndex), end - nameIndex);
    };

    // Build the WMO -> world transform in vmap_extractor's intermediate
    // frame (X = client.Z, Y = client.X, Z = client.Y_up), exactly the
    // way model.cpp:217-218 does it.  We rotate the MODD local position
    // by this matrix and add the swapped translation; only at the very
    // end do we re-project to the editor's TC frame (mid - X, mid - Y, Z).
    float const wmoExtractorPos[3] = {
        placement.positionXYZ[2],   // X_extractor = client.Z
        placement.positionXYZ[0],   // Y_extractor = client.X
        placement.positionXYZ[1],   // Z_extractor = client.Y_up
    };

    // WMO ROOT rotation, decoded as Euler degrees (ZYX order).  This is
    // correct ONLY for the ADT MODF on-disk placement, which stores the
    // WMO rotation as three Euler angles in degrees (rot.x/rot.y/rot.z).
    //
    // Note: the client's RUNTIME placement record, CMapObjInst,
    // does NOT store Euler angles -- it carries the rotation as a
    // QUATERNION (an OWORD at offset +16).  So this Euler->Mat3 path
    // applies to MODF disk records ONLY.
    //
    // DO NOT reuse this Euler decode for quaternion-sourced rotations
    // (CMapObjInst, or any extracted/runtime record that already holds a
    // quaternion).  Feeding quaternion components in as degrees produces
    // garbage orientations.
    Mat3 const wmoRot = Mat3::fromEulerAnglesZYX(
        placement.rotationDegXYZ[1] * kDegToRad,   // yaw   <- rot.y
        placement.rotationDegXYZ[0] * kDegToRad,   // pitch <- rot.x
        placement.rotationDegXYZ[2] * kDegToRad);  // roll  <- rot.z

    float const wmoScale = (placement.scale > 0.0f) ? placement.scale : 1.0f;

    out.reserve(out.size() + spawnCount);

    auto emitSet = [&](ModsEntry const& set)
    {
        std::size_t const begin = set.startIndex;
        std::size_t const end   = std::size_t(set.startIndex) + std::size_t(set.count);
        for (std::size_t i = begin; i < end && i < spawnCount; ++i)
        {
            ModdEntry e;
            std::memcpy(&e, moddBody + i * sizeof(ModdEntry), sizeof(ModdEntry));

            uint32_t const nameIndex = e.nameIndexAndFlags & 0x00FFFFFFu;
            uint32_t const fdid      = resolveFdid(nameIndex);

            DoodadInstance d;
            d.modelFileDataId = fdid;
            d.modelPath       = (fdid == 0) ? resolvePath(nameIndex) : std::string{};
            d.uniqueId        = 0;     // WMO doodads have no MDDF.UniqueId equivalent.

            // World position in extractor frame: wmoPos + wmoRot * (localPos * wmoScale).
            float const localScaled[3] = {
                e.posX * wmoScale,
                e.posY * wmoScale,
                e.posZ * wmoScale,
            };
            float rotated[3];
            mulVec(wmoRot, localScaled, rotated);
            float const worldExtractor[3] = {
                wmoExtractorPos[0] + rotated[0],
                wmoExtractorPos[1] + rotated[1],
                wmoExtractorPos[2] + rotated[2],
            };

            // Project to editor TC frame (X=north, Y=west, Z=up): the
            // extractor frame's X/Y axes correspond to client.Z / client.X
            // which both flip-sign under the editor's mid-origin mapping.
            d.x = kClientMid - worldExtractor[0];
            d.y = kClientMid - worldExtractor[1];
            d.z = worldExtractor[2];

            // Combined rotation: wmoRot * doodadQuat (model.cpp:253 order).
            // Then decompose into editor-frame Euler ZYX.  The translation
            // axis-flip from extractor to editor frame negates the X and Y
            // axes; to keep `Rz * Ry * Rx` of editor angles equal to the
            // remapped rotation matrix M, we conjugate by S = diag(-1,-1,1):
            //   M_editor = S * M_extractor * S^-1 = S * M_extractor * S.
            // For a 3x3, this flips the sign of the (0,2), (1,2), (2,0)
            // and (2,1) entries (i.e. row 2 cross-terms) and leaves the
            // top-left 2x2 + the (2,2) entry unchanged.  Apply that
            // directly without an extra mat-mul.
            Mat3 const doodadRot   = Mat3::fromQuat(e.rotX, e.rotY, e.rotZ, e.rotW);
            Mat3 const combinedExt = mul(doodadRot, wmoRot);
            Mat3 combined = combinedExt;
            combined.m[0][2] = -combined.m[0][2];
            combined.m[1][2] = -combined.m[1][2];
            combined.m[2][0] = -combined.m[2][0];
            combined.m[2][1] = -combined.m[2][1];

            float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
            matToEulerZYX(combined, yaw, pitch, roll);
            d.rotZ = yaw;
            d.rotY = pitch;
            d.rotX = roll;

            d.scale = (e.scale > 0.0f) ? e.scale : 1.0f;

            out.push_back(std::move(d));
        }
        (void)kRadToDeg;     // reserved for future diagnostics.
    };

    // Retail behaviour: set 0 (Default) is always emitted; setIndex (if
    // non-zero and in range) is unioned on top.  Mirrors
    // vmap_extractor/model.cpp:303-307.
    if (setCount >= 1)
    {
        ModsEntry s0;
        std::memcpy(&s0, modsBody, sizeof(ModsEntry));
        emitSet(s0);
    }
    if (setIndex != 0 && setIndex < setCount)
    {
        ModsEntry sN;
        std::memcpy(&sN, modsBody + setIndex * sizeof(ModsEntry), sizeof(ModsEntry));
        emitSet(sN);
    }

    return true;
}

WmoRootPlacement computeWmoRootPlacement(WmoPlacementInstance const& wp)
{
    WmoRootPlacement rp;

    // Translation: project the MODF client-frame position into the
    // extractor frame (X = client.Z, Y = client.X, Z = client.Y_up) the
    // same way loadWmoDoodads' wmoExtractorPos does, then to the editor TC
    // frame (mid - X, mid - Y, Z).  For the ROOT mesh there is no interior
    // local offset to rotate in, so the extractor position IS the world
    // position.
    float const ex = wp.posXYZ[2];   // X_extractor = client.Z
    float const ey = wp.posXYZ[0];   // Y_extractor = client.X
    float const ez = wp.posXYZ[1];   // Z_extractor = client.Y_up
    rp.x = kClientMid - ex;
    rp.y = kClientMid - ey;
    rp.z = ez;

    // Rotation: MODF stores Euler degrees (rotX pitch, rotY yaw, rotZ roll);
    // build the extractor-frame Mat3 exactly like loadWmoDoodads' wmoRot,
    // conjugate by S = diag(-1, -1, 1) (negate the row-2 cross terms), then
    // decompose to editor-frame Euler ZYX so the renderer's
    // translate * Rz * Ry * Rx reproduces the conjugated matrix.  This is
    // the doodad path with an identity interior-doodad quaternion.
    Mat3 const wmoRot = Mat3::fromEulerAnglesZYX(
        wp.rotDegXYZ[1] * kDegToRad,   // yaw   <- rot.y
        wp.rotDegXYZ[0] * kDegToRad,   // pitch <- rot.x
        wp.rotDegXYZ[2] * kDegToRad);  // roll  <- rot.z

    Mat3 combined = wmoRot;
    combined.m[0][2] = -combined.m[0][2];
    combined.m[1][2] = -combined.m[1][2];
    combined.m[2][0] = -combined.m[2][0];
    combined.m[2][1] = -combined.m[2][1];

    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    matToEulerZYX(combined, yaw, pitch, roll);
    rp.rotZ = yaw;
    rp.rotY = pitch;
    rp.rotX = roll;

    rp.scale = (wp.scale > 0.0f) ? wp.scale : 1.0f;
    return rp;
}

} // namespace world_editor::io
