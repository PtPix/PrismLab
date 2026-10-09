#ifndef PRISM_SURFACE_DEPTH_CB_H
#define PRISM_SURFACE_DEPTH_CB_H

struct FSurfaceDepthConstants
{
#ifdef __cplusplus
	float ObjectToClip[16];
#else
	float4x4 ObjectToClip;
#endif
};

#endif // PRISM_SURFACE_DEPTH_CB_H
