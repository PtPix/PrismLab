#include "CameraController.h"

#include <donut/core/log.h>

#include <algorithm>

namespace Prism::Adapter
{
	void FCameraController::Initialize(const FCameraPreset& InPreset)
	{
		CameraPreset = InPreset;

		Camera.GetFirstPersonCamera().LookAt(CameraPreset.Position, CameraPreset.Target);
		Camera.GetFirstPersonCamera().SetMoveSpeed(CameraPreset.MoveSpeed);

		const float OrbitDistance = std::max(dm::length(CameraPreset.Position - CameraPreset.Target), 0.5f);
		Camera.GetThirdPersonCamera().SetTargetPosition(CameraPreset.Target);
		Camera.GetThirdPersonCamera().SetDistance(OrbitDistance);

		Camera.SwitchToFirstPerson(false);

		TargetPosition = CameraPreset.Target;
		bDiscontinuity = true;
	}

	void FCameraController::Update(float DeltaTimeSeconds, const FExtent2D& InRenderSize, bool bAnimate)
	{
		if (bAnimate)
		{
			Camera.Animate(DeltaTimeSeconds);
		}

		RenderSize = dm::uint2(std::max(InRenderSize.Width, 1u), std::max(InRenderSize.Height, 1u));

		// 上一帧的矩阵先落到 previous，再计算当前帧，供重投影使用。
		CameraData.Previous = CameraData.Current;
		CameraData.bHasPrevious = bUpdatedOnce;
		CameraData.PreviousJitter = CameraData.Jitter;

		const float AspectRatio = float(RenderSize.x) / float(RenderSize.y);
		CameraData.VerticalFovRadians = dm::radians(CameraPreset.FovDegrees);
		CameraData.AspectRatio = AspectRatio;
		CameraData.ZNearMeters = CameraPreset.ZNear;
		CameraData.ZFarMeters = CameraPreset.ZFar;

		const dm::float4x4 Projection = dm::perspProjD3DStyle(CameraData.VerticalFovRadians, AspectRatio,
															  CameraData.ZNearMeters, CameraData.ZFarMeters);

		View.SetViewport(nvrhi::Viewport(float(RenderSize.x), float(RenderSize.y)));
		View.SetMatrices(Camera.GetWorldToViewMatrix(), Projection);
		View.UpdateCache();

		// 环绕相机需要视口与投影矩阵才能把拖拽增量转换成旋转
		Camera.GetThirdPersonCamera().SetView(View);

		CameraData.Current = Prism::FViewMatrices::Build(View.GetViewMatrix(), View.GetProjectionMatrix(false));

		if (const donut::app::BaseCamera* ActiveCamera = Camera.GetActiveUserCamera())
		{
			CameraData.Position = ActiveCamera->GetPosition();
			CameraData.Forward = dm::normalize(ActiveCamera->GetDir());
			CameraData.Up = dm::normalize(ActiveCamera->GetUp());
		}

		// Preserve the camera roll when recording and replaying poses.
		dm::float3 Right = dm::cross(CameraData.Forward, CameraData.Up);
		if (dm::length(Right) < 1e-4f)
			Right = dm::float3(1.f, 0.f, 0.f);
		else
			Right = dm::normalize(Right);

		CameraData.Right = Right;
		CameraData.Up = dm::cross(Right, CameraData.Forward);

		if (Camera.IsThirdPersonActive())
			TargetPosition = Camera.GetThirdPersonCamera().GetTargetPosition();

		bUpdatedOnce = true;
	}

	bool FCameraController::KeyboardUpdate(int Key, int Scancode, int Action, int Mods)
	{
		return Camera.KeyboardUpdate(Key, Scancode, Action, Mods);
	}

	bool FCameraController::MousePosUpdate(double Xpos, double Ypos)
	{
		return Camera.MousePosUpdate(Xpos, Ypos);
	}

	bool FCameraController::MouseButtonUpdate(int Button, int Action, int Mods)
	{
		return Camera.MouseButtonUpdate(Button, Action, Mods);
	}

	bool FCameraController::MouseScrollUpdate(double Xoffset, double Yoffset)
	{
		return Camera.MouseScrollUpdate(Xoffset, Yoffset);
	}

	void FCameraController::SwitchToFirstPerson(bool bAnimate)
	{
		Camera.SwitchToFirstPerson(bAnimate);
		Camera.GetFirstPersonCamera().SetMoveSpeed(CameraPreset.MoveSpeed);
		bDiscontinuity = true;
	}

	void FCameraController::SwitchToThirdPerson(bool bAnimate)
	{
		Camera.SwitchToThirdPerson(bAnimate);
		bDiscontinuity = true;
	}

	bool FCameraController::ConsumeDiscontinuity()
	{
		const bool bWasDiscontinuous = bDiscontinuity;
		bDiscontinuity = false;
		return bWasDiscontinuous;
	}

	void FCameraController::SetJitter(const dm::float2& JitterInPixels)
	{
		CameraData.Jitter = JitterInPixels;
		View.SetPixelOffset(JitterInPixels);
		View.UpdateCache();
	}

	void FCameraController::ApplyPose(const FCameraPose& Pose)
	{
		Camera.SwitchToFirstPerson(false);
		Camera.GetFirstPersonCamera().LookAt(Pose.Position, Pose.Position + Pose.Direction, Pose.Up);
		CameraPreset.FovDegrees = dm::degrees(Pose.FovRadians);
		CameraPreset.ZNear = Pose.NearPlane;
		CameraPreset.ZFar = Pose.FarPlane;
	}
} // namespace Prism::Adapter
