// Shared constant layout for the contract check pass: included by both C++ and HLSL.
//
// Kept deliberately tiny: the contract check only needs the inverse view-projection, the render size
// and the depth convention, because its job is to reconstruct a value that the CPU can verify.

#ifndef PRISM_CONTRACT_CHECK_CB_H
#define PRISM_CONTRACT_CHECK_CB_H

struct FContractCheckConstants
{
	float4x4 ClipToWorld;
	float2 InverseSize;
	uint2 Size;
	float ZNear;
	float ZFar;
	int DepthConvention; // 0 = forward-Z [0, 1], 1 = reversed-Z
	int Pad0;
};

#endif // PRISM_CONTRACT_CHECK_CB_H
