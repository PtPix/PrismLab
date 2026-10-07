#pragma once

// Donut facilities: the shared camera.
//
// The host owns one controller; experiments never create a camera, never build a projection matrix and
// never touch the Donut camera classes. They receive CameraData (contracts) from the frame context.

#include <framework/render/data/CameraPreset.h>

#include <framework/render/data/CameraData.h>
#include <framework/render/data/CameraPose.h>
#include <framework/core/Types.h>

#include <donut/app/Camera.h>
#include <donut/engine/View.h>

namespace Prism::Adapter
{
	class FCameraController
	{
	  public:
		void Initialize(const FCameraPreset& InPreset);

		// 每帧调用一次：更新相机动画、投影矩阵、视图缓存，并把结果写入 CameraData。
		void Update(float DeltaTimeSeconds, const FExtent2D& InRenderSize, bool bAnimate = true);

		bool KeyboardUpdate(int Key, int Scancode, int Action, int Mods);
		bool MousePosUpdate(double Xpos, double Ypos);
		bool MouseButtonUpdate(int Button, int Action, int Mods);
		bool MouseScrollUpdate(double Xoffset, double Yoffset);

		void SwitchToFirstPerson(bool bAnimate);
		void SwitchToThirdPerson(bool bAnimate);
		[[nodiscard]] bool IsFirstPerson() const
		{
			return Camera.IsFirstPersonActive();
		}

		[[nodiscard]] donut::engine::PlanarView& GetView()
		{
			return View;
		}
		[[nodiscard]] const donut::engine::PlanarView& GetView() const
		{
			return View;
		}

		[[nodiscard]] const Prism::FCameraData& GetCameraData() const
		{
			return CameraData;
		}
		[[nodiscard]] dm::float3 GetPosition() const
		{
			return CameraData.Position;
		}
		[[nodiscard]] dm::float3 GetDirection() const
		{
			return CameraData.Forward;
		}
		[[nodiscard]] float GetDistance() const
		{
			return dm::length(CameraData.Position - TargetPosition);
		}

		// 相机不连续（切换视角、瞬移）时置位；宿主据此请求历史重置。
		[[nodiscard]] bool ConsumeDiscontinuity();

		void SetJitter(const dm::float2& JitterInPixels);
		void ApplyPose(const FCameraPose& Pose);

	  private:
		donut::app::SwitchableCamera Camera;
		donut::engine::PlanarView View;

		Prism::FCameraData CameraData;
		FCameraPreset CameraPreset;

		dm::float3 TargetPosition = dm::float3(0.f);
		dm::uint2 RenderSize = dm::uint2(1);
		bool bUpdatedOnce = false;
		bool bDiscontinuity = false;
	};
} // namespace Prism::Adapter
