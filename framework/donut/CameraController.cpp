#include "CameraController.h"

#include <donut/core/log.h>

#include <algorithm>

namespace prism::adapter
{
    void CameraController::Initialize(const HostCameraPreset& preset)
    {
        m_Preset = preset;

        m_Camera.GetFirstPersonCamera().LookAt(preset.position, preset.target);
        m_Camera.GetFirstPersonCamera().SetMoveSpeed(preset.moveSpeed);

        const float orbitDistance = std::max(dm::length(preset.position - preset.target), 0.5f);
        m_Camera.GetThirdPersonCamera().SetTargetPosition(preset.target);
        m_Camera.GetThirdPersonCamera().SetDistance(orbitDistance);

        m_Camera.SwitchToFirstPerson(false);

        m_TargetPosition = preset.target;
        m_Discontinuity = true;
    }

    void CameraController::Update(float deltaTimeSeconds, const Extent2D& renderSize)
    {
        m_Camera.Animate(deltaTimeSeconds);

        m_RenderSize = dm::uint2(std::max(renderSize.width, 1u), std::max(renderSize.height, 1u));

        // 上一帧的矩阵先落到 previous，再计算当前帧，供重投影使用。
        m_CameraData.previous = m_CameraData.current;
        m_CameraData.hasPrevious = m_UpdatedOnce;
        m_CameraData.previousJitter = m_CameraData.jitter;

        const float aspectRatio = float(m_RenderSize.x) / float(m_RenderSize.y);
        m_CameraData.verticalFovRadians = dm::radians(m_Preset.fovDegrees);
        m_CameraData.aspectRatio = aspectRatio;
        m_CameraData.zNearMeters = m_Preset.zNear;
        m_CameraData.zFarMeters = m_Preset.zFar;

        const dm::float4x4 projection = dm::perspProjD3DStyle(
            m_CameraData.verticalFovRadians,
            aspectRatio,
            m_CameraData.zNearMeters,
            m_CameraData.zFarMeters);

        m_View.SetViewport(nvrhi::Viewport(float(m_RenderSize.x), float(m_RenderSize.y)));
        m_View.SetMatrices(m_Camera.GetWorldToViewMatrix(), projection);
        m_View.UpdateCache();

        // 环绕相机需要视口与投影矩阵才能把拖拽增量转换成旋转
        m_Camera.GetThirdPersonCamera().SetView(m_View);

        m_CameraData.current = prism::ViewMatrices::Build(m_View.GetViewMatrix(), m_View.GetProjectionMatrix(false));

        if (const donut::app::BaseCamera* camera = m_Camera.GetActiveUserCamera())
        {
            m_CameraData.position = camera->GetPosition();
            m_CameraData.forward = dm::normalize(camera->GetDir());
        }

        // 右手世界系：right = forward x worldUp，up = right x forward
        const dm::float3 worldUp = dm::float3(0.f, 1.f, 0.f);
        dm::float3 right = dm::cross(m_CameraData.forward, worldUp);
        if (dm::length(right) < 1e-4f)
            right = dm::float3(1.f, 0.f, 0.f);
        else
            right = dm::normalize(right);

        m_CameraData.right = right;
        m_CameraData.up = dm::cross(right, m_CameraData.forward);

        if (m_Camera.IsThirdPersonActive())
            m_TargetPosition = m_Camera.GetThirdPersonCamera().GetTargetPosition();

        m_UpdatedOnce = true;
    }

    bool CameraController::KeyboardUpdate(int key, int scancode, int action, int mods)
    {
        return m_Camera.KeyboardUpdate(key, scancode, action, mods);
    }

    bool CameraController::MousePosUpdate(double xpos, double ypos)
    {
        return m_Camera.MousePosUpdate(xpos, ypos);
    }

    bool CameraController::MouseButtonUpdate(int button, int action, int mods)
    {
        return m_Camera.MouseButtonUpdate(button, action, mods);
    }

    bool CameraController::MouseScrollUpdate(double xoffset, double yoffset)
    {
        return m_Camera.MouseScrollUpdate(xoffset, yoffset);
    }

    void CameraController::SwitchToFirstPerson(bool animate)
    {
        m_Camera.SwitchToFirstPerson(animate);
        m_Camera.GetFirstPersonCamera().SetMoveSpeed(m_Preset.moveSpeed);
        m_Discontinuity = true;
    }

    void CameraController::SwitchToThirdPerson(bool animate)
    {
        m_Camera.SwitchToThirdPerson(animate);
        m_Discontinuity = true;
    }

    bool CameraController::ConsumeDiscontinuity()
    {
        const bool discontinuity = m_Discontinuity;
        m_Discontinuity = false;
        return discontinuity;
    }

    void CameraController::SetJitter(const dm::float2& jitterInPixels)
    {
        m_CameraData.jitter = jitterInPixels;
        m_View.SetPixelOffset(jitterInPixels);
        m_View.UpdateCache();
    }
}
