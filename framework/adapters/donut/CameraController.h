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

namespace prism::adapter
{
    class CameraController
    {
    public:
        void Initialize(const CameraPreset& preset);

        // 每帧调用一次：更新相机动画、投影矩阵、视图缓存，并把结果写入 CameraData。
        void Update(float deltaTimeSeconds, const Extent2D& renderSize, bool animate = true);

        bool KeyboardUpdate(int key, int scancode, int action, int mods);
        bool MousePosUpdate(double xpos, double ypos);
        bool MouseButtonUpdate(int button, int action, int mods);
        bool MouseScrollUpdate(double xoffset, double yoffset);

        void SwitchToFirstPerson(bool animate);
        void SwitchToThirdPerson(bool animate);
        [[nodiscard]] bool IsFirstPerson() const { return m_Camera.IsFirstPersonActive(); }

        [[nodiscard]] donut::engine::PlanarView& GetView() { return m_View; }
        [[nodiscard]] const donut::engine::PlanarView& GetView() const { return m_View; }

        [[nodiscard]] const prism::CameraData& GetCameraData() const { return m_CameraData; }
        [[nodiscard]] dm::float3 GetPosition() const { return m_CameraData.position; }
        [[nodiscard]] dm::float3 GetDirection() const { return m_CameraData.forward; }
        [[nodiscard]] float GetDistance() const { return dm::length(m_CameraData.position - m_TargetPosition); }

        // 相机不连续（切换视角、瞬移）时置位；宿主据此请求历史重置。
        [[nodiscard]] bool ConsumeDiscontinuity();

        void SetJitter(const dm::float2& jitterInPixels);
        void ApplyPose(const CameraPose& pose);

    private:
        donut::app::SwitchableCamera m_Camera;
        donut::engine::PlanarView m_View;

        prism::CameraData m_CameraData;
        CameraPreset m_Preset;

        dm::float3 m_TargetPosition = dm::float3(0.f);
        dm::uint2 m_RenderSize = dm::uint2(1);
        bool m_UpdatedOnce = false;
        bool m_Discontinuity = false;
    };
}
