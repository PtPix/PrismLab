// Shared constant layout for the contract check pass: included by both C++ and HLSL.
//
// Kept deliberately tiny: the contract check only needs the inverse view-projection, the render size
// and the depth convention, because its job is to reconstruct a value that the CPU can verify.

#ifndef RENDERLAB_LAB_CONTRACT_CHECK_CB_H
#define RENDERLAB_LAB_CONTRACT_CHECK_CB_H

struct ContractCheckConstants
{
    float4x4 clipToWorld;
    float2 inverseSize;
    uint2 size;
    float zNear;
    float zFar;
    int depthConvention;      // 0 = forward-Z [0, 1], 1 = reversed-Z
    int pad0;
};

#endif // RENDERLAB_LAB_CONTRACT_CHECK_CB_H
