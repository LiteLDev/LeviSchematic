#include "SchematicPlacement.h"

#include "levishematic/schematic/SchematicTransform.h"

namespace levishematic::placement {

// ================================================================
// 静态 ID 生成器
// ================================================================
SchematicPlacement::Id SchematicPlacement::sNextId = 1;

// ================================================================
// 构造
// ================================================================
SchematicPlacement::SchematicPlacement(
    std::shared_ptr<schematic::LitematicSchematic> schematic,
    BlockPos                                        origin,
    const std::string&                              name
)
    : mId(sNextId++)
    , mName(name.empty() ? schematic->getMetadata().name : name)
    , mSchematic(std::move(schematic))
    , mOrigin(origin)
{
    initSubRegions();
}

SchematicPlacement::SchematicPlacement(
    std::vector<LocalBlockEntry> localBlocks,
    BlockPos                    size,
    BlockPos                    origin,
    const std::string&          name
)
    : mId(sNextId++)
    , mName(name)
    , mLocalBlocks(std::move(localBlocks))
    , mLocalSize(size)
    , mOrigin(origin)
{}

// ================================================================
// 从 Schematic 的 Region 列表初始化子区域放置参数
// ================================================================
void SchematicPlacement::initSubRegions() {
    mSubRegions.clear();
    if (!mSchematic || !mSchematic->isLoaded()) return;

    for (const auto& region : mSchematic->getRegions()) {
        SubRegionPlacement sub;
        sub.name     = region.name;
        sub.offset   = region.position;
        sub.rotation = transform::Rotation::NONE;
        sub.mirror   = transform::Mirror::NONE;
        sub.enabled  = true;
        mSubRegions[region.name] = std::move(sub);
    }
}

// ================================================================
// 变换操作
// ================================================================
void SchematicPlacement::setOrigin(BlockPos origin) {
    mOrigin = origin;
    markDirty();
}

void SchematicPlacement::move(int dx, int dy, int dz) {
    mOrigin.x += dx;
    mOrigin.y += dy;
    mOrigin.z += dz;
    markDirty();
}

void SchematicPlacement::setRotation(transform::Rotation rot) {
    mRotation = rot;
    markDirty();
}

void SchematicPlacement::rotateCW90() {
    mRotation = transform::rotateBy(mRotation, transform::Rotation::CW_90);
    markDirty();
}

void SchematicPlacement::rotateCCW90() {
    mRotation = transform::rotateBy(mRotation, transform::Rotation::CCW_90);
    markDirty();
}

void SchematicPlacement::rotate180() {
    mRotation = transform::rotateBy(mRotation, transform::Rotation::CW_180);
    markDirty();
}

void SchematicPlacement::setMirror(transform::Mirror mir) {
    mMirror = mir;
    markDirty();
}

void SchematicPlacement::toggleMirrorX() {
    mMirror = (mMirror == transform::Mirror::MIRROR_X)
                  ? transform::Mirror::NONE
                  : transform::Mirror::MIRROR_X;
    markDirty();
}

void SchematicPlacement::toggleMirrorZ() {
    mMirror = (mMirror == transform::Mirror::MIRROR_Z)
                  ? transform::Mirror::NONE
                  : transform::Mirror::MIRROR_Z;
    markDirty();
}

void SchematicPlacement::resetTransform() {
    mRotation = transform::Rotation::NONE;
    mMirror   = transform::Mirror::NONE;
    // 保持 origin 不变，重置子区域变换
    for (auto& [name, sub] : mSubRegions) {
        sub.rotation = transform::Rotation::NONE;
        sub.mirror   = transform::Mirror::NONE;
    }
    markDirty();
}

// ================================================================
// 子区域管理
// ================================================================
SubRegionPlacement* SchematicPlacement::getSubRegion(const std::string& name) {
    auto it = mSubRegions.find(name);
    return (it != mSubRegions.end()) ? &it->second : nullptr;
}

void SchematicPlacement::setSubRegionEnabled(const std::string& name, bool enabled) {
    auto* sub = getSubRegion(name);
    if (sub) {
        sub->enabled = enabled;
        markDirty();
    }
}

void SchematicPlacement::setSubRegionTransform(
    const std::string& name,
    transform::Rotation rot,
    transform::Mirror mir
) {
    auto* sub = getSubRegion(name);
    if (sub) {
        sub->rotation = rot;
        sub->mirror   = mir;
        markDirty();
    }
}

// ================================================================
// 投影块生成 — 默认颜色
// ================================================================
static constexpr mce::Color kDefaultProjectionColor{0.75f, 0.85f, 1.0f, 0.85f};

std::vector<render::ProjEntry>
SchematicPlacement::generateProjEntries(bool skipAir) const {
    return generateProjEntries(kDefaultProjectionColor, skipAir);
}

// ================================================================
// 投影块生成 — 自定义颜色
//
// 利用 SchematicTransform 中已有的 toBedrockBlocksTransformed 逻辑
// 构建 PlacementTransform，然后转换为 ProjEntry
// ================================================================
std::vector<render::ProjEntry>
SchematicPlacement::generateProjEntries(mce::Color color, bool skipAir) const {
    std::vector<render::ProjEntry> result;

    if (!hasSchematic() || !mEnabled || !mRenderEnabled) {
        if (!hasLocalBlocks() || !mEnabled || !mRenderEnabled || !mLocalSize.has_value()) {
            return result;
        }

        result.reserve(mLocalBlocks.size());
        for (const auto& entry : mLocalBlocks) {
            if (!entry.block) continue;
            if (skipAir && entry.block->isAir()) continue;

            auto local = transform::transformLocalPos(
                entry.localPos,
                mLocalSize->x,
                mLocalSize->z,
                mMirror,
                mRotation
            );

            result.push_back(render::ProjEntry{
                BlockPos{
                    mOrigin.x + local.x,
                    mOrigin.y + local.y,
                    mOrigin.z + local.z,
                },
                entry.block,
                color
            });
        }

        return result;
    }

    // 构建 PlacementTransform
    transform::PlacementTransform pt;
    pt.origin   = mOrigin;
    pt.rotation = mRotation;
    pt.mirror   = mMirror;

    // 填充子区域变换
    for (const auto& [name, sub] : mSubRegions) {
        if (!sub.enabled) continue;
        transform::PlacementTransform::SubRegion sr;
        sr.name     = sub.name;
        sr.offset   = sub.offset;
        sr.rotation = sub.rotation;
        sr.mirror   = sub.mirror;
        sr.enabled  = sub.enabled;
        pt.subRegions.push_back(std::move(sr));
    }

    // 使用已有的变换函数生成方块列表
    auto blocks = transform::toBedrockBlocksTransformed(*mSchematic, pt, skipAir);

    // 转换为 ProjEntry
    result.reserve(blocks.size());
    for (const auto& entry : blocks) {
        result.push_back(render::ProjEntry{
            entry.worldPos,
            entry.block,
            color
        });
    }

    return result;
}

// ================================================================
// 包围盒计算
// ================================================================
std::pair<BlockPos, BlockPos> SchematicPlacement::getEnclosingBox() const {
    if (!hasSchematic() && !hasLocalBlocks()) {
        return {mOrigin, mOrigin};
    }

    auto entries = generateProjEntries(true);
    if (entries.empty()) {
        return {mOrigin, mOrigin};
    }

    BlockPos minPos = entries[0].pos;
    BlockPos maxPos = entries[0].pos;

    for (const auto& e : entries) {
        if (e.pos.x < minPos.x) minPos.x = e.pos.x;
        if (e.pos.y < minPos.y) minPos.y = e.pos.y;
        if (e.pos.z < minPos.z) minPos.z = e.pos.z;
        if (e.pos.x > maxPos.x) maxPos.x = e.pos.x;
        if (e.pos.y > maxPos.y) maxPos.y = e.pos.y;
        if (e.pos.z > maxPos.z) maxPos.z = e.pos.z;
    }

    return {minPos, maxPos};
}

// ================================================================
// 变换描述
// ================================================================
std::string SchematicPlacement::describeTransform() const {
    static const char* rotNames[] = {"0°", "CW90°", "180°", "CCW90°"};
    static const char* mirNames[] = {"None", "X", "Z"};

    std::string desc = "Origin:(" + std::to_string(mOrigin.x) + ","
                     + std::to_string(mOrigin.y) + "," + std::to_string(mOrigin.z) + ")"
                     + " Rot:" + rotNames[static_cast<int>(mRotation)]
                     + " Mir:" + mirNames[static_cast<int>(mMirror)];
    return desc;
}

// ================================================================
// 统计
// ================================================================
int SchematicPlacement::getTotalNonAirBlocks() const {
    if (hasLocalBlocks()) return static_cast<int>(mLocalBlocks.size());
    if (!hasSchematic()) return 0;
    return mSchematic->getTotalNonAirBlocks();
}

} // namespace levishematic::placement
