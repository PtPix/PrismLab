#ifndef PRISM_DEFERRED_DEPTH_DEBUG_CB_H
#define PRISM_DEFERRED_DEPTH_DEBUG_CB_H

struct FDepthDebugConstants
{
    float2 InverseSize;
    float ZNear;
    float ZFar;
    int DepthConvention;
    int Mode;
    float2 Padding;
};

#endif // PRISM_DEFERRED_DEPTH_DEBUG_CB_H
