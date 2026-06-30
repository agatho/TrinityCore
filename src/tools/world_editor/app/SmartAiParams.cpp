// SmartAiParams.cpp - see SmartAiParams.h. Backed by the generated metadata.

#include "SmartAiParams.h"

#include "SmartAiMetadata.gen.h"   // build-time codegen from core headers

namespace world_editor::app
{
namespace
{

char const* lookup(world_editor::smartai::MetaEntry const* arr, int count, int value)
{
    for (int i = 0; i < count; ++i)
        if (arr[i].value == value)
            return arr[i].params;
    return "";
}

} // namespace

char const* smartEventParams(int value)
{
    return lookup(world_editor::smartai::kSmartEvents, world_editor::smartai::kSmartEventsCount, value);
}

char const* smartActionParams(int value)
{
    return lookup(world_editor::smartai::kSmartActions, world_editor::smartai::kSmartActionsCount, value);
}

char const* smartTargetParams(int value)
{
    return lookup(world_editor::smartai::kSmartTargets, world_editor::smartai::kSmartTargetsCount, value);
}

char const* conditionParams(int value)
{
    return lookup(world_editor::smartai::kConditions, world_editor::smartai::kConditionsCount, value);
}

} // namespace world_editor::app
