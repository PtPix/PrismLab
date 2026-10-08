#pragma once

#include <framework/render/data/CameraData.h>

namespace Prism::Surface
{
    inline dm::float4x4 MakeReverseZProjection(float FovY, float Aspect, float ZNear, float ZFar)
    {
        return MakePerspectiveProjection(FovY, Aspect, ZNear, ZFar, EDepthConvention::ReversedZ0To1);
    }
}