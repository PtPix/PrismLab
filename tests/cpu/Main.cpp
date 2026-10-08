#include <framework/adapters/donut/CameraController.h>
#include <framework/render/data/CameraData.h>

#include <cmath>
#include <filesystem>
#include <iostream>

bool RunReplayTests(const std::filesystem::path& Directory);

bool RunDepthConventionTests()
{
	using namespace Prism;
	constexpr float Near = 0.1f;
	constexpr float Far = 100.f;
	bool bPassed = true;
	for (EDepthConvention Mode : {EDepthConvention::ForwardZ0To1, EDepthConvention::ReversedZ0To1})
	{
		const dm::float4x4 Projection = MakePerspectiveProjection(dm::radians(60.f), 16.f / 9.f, Near, Far, Mode);
		for (float Z : {Near, 1.f, 10.f, Far})
		{
			const dm::float4 Clip = dm::float4(0.f, 0.f, Z, 1.f) * Projection;
			const float Depth = Clip.z / Clip.w;
			const float Expected = DeviceDepthFromLinear(Z, Near, Far, Mode);
			const float Back = LinearizeDepth(Depth, Near, Far, Mode);
			bPassed &= std::fabs(Depth - Expected) < 1e-5f;
			bPassed &= std::fabs(Back - Z) < 0.002f;
		}
		bPassed &= IsBackgroundDepth(GetDepthClearValue(Mode), Mode);
		FCameraData Camera;
		Camera.Current = FViewMatrices::Build(dm::affine3::identity(), Projection);
		const dm::float3 Point(0.3f, 0.1f, 2.f);
		const dm::float4 Clip = dm::float4(Point, 1.f) * Camera.Current.WorldToClip;
		const dm::float2 Uv(Clip.x / Clip.w * 0.5f + 0.5f, 0.5f - Clip.y / Clip.w * 0.5f);
		bPassed &= dm::length(Camera.ReconstructWorldPosition(Uv, Clip.z / Clip.w) - Point) < 1e-4f;
	}

	Prism::Adapter::FCameraController Controller;
	Controller.Initialize(FCameraPreset{});
	Controller.Update(0.f, {640, 360}, false);
	Controller.SetDepthConvention(EDepthConvention::ReversedZ0To1);
	Controller.Update(0.f, {640, 360}, false);
	bPassed &= Controller.GetView().IsReverseDepth();
	bPassed &= Controller.GetCameraData().DepthConvention == EDepthConvention::ReversedZ0To1;
	bPassed &= !Controller.GetCameraData().bHasPrevious;
	bPassed &= Controller.ConsumeDiscontinuity();
	Controller.SetJitter(dm::float2(0.25f, -0.125f));
	const FCameraData& Jittered = Controller.GetCameraData();
	const dm::float3 WorldPoint = Jittered.Position + Jittered.Forward * 2.f;
	const dm::float4 RasterClip = dm::float4(WorldPoint, 1.f) * Jittered.Raster.WorldToClip;
	const dm::float2 RasterUv = Jittered.WorldToRasterUv(WorldPoint);
	bPassed &= dm::length(Jittered.ReconstructRasterWorldPosition(RasterUv, RasterClip.z / RasterClip.w) - WorldPoint) < 1e-4f;
	bPassed &= dm::length(RasterUv - Jittered.WorldToUnjitteredUv(WorldPoint)) > 1e-5f;

	if (!bPassed)
		std::cerr << "Depth convention tests failed\n";
	return bPassed;
}

int main(int argc, char** argv)
{
	auto Directory = std::filesystem::absolute(argv[0]).parent_path() / "cpu-test-output";
	std::filesystem::create_directories(Directory);
	(void)argc;
	const bool bReplayPassed = RunReplayTests(Directory);
	const bool bDepthPassed = RunDepthConventionTests();
	return bReplayPassed && bDepthPassed ? 0 : 1;
}
