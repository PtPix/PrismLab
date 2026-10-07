// Shared constant layout for the debug view pass: included by both C++ and HLSL.
//
// A CPU semantic struct must not be uploaded by memory layout; this file is the GPU layout, with the
// same field order in both languages and static_asserts on the C++ side.
//
// Matrix convention: row-vector form mul(v, M), exactly like Donut. clipToWorld is the inverse of
// worldToClip = worldToView * viewToClip.

#ifndef PRISM_FORWARD_DEBUG_VIEW_CB_H
#define PRISM_FORWARD_DEBUG_VIEW_CB_H

struct FDebugViewConstants
{
	float4x4 ClipToWorld;
	float2 InverseSize;
	float ZNear;
	float ZFar;
	int Mode;			 // 0 = device depth, 1 = linear depth, 2 = world position, 3 = normal from depth
	int DepthConvention; // 0 = forward-Z [0, 1], 1 = reversed-Z
	float DepthScale;
	float Pad0;
};

#endif // PRISM_FORWARD_DEBUG_VIEW_CB_H
