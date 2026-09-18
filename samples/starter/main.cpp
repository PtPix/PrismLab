/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

// Adapted from Donut-Samples basic_triangle for PrismLab.
#include <donut/app/ApplicationBase.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <donut/engine/ShaderFactory.h>
#include <nvrhi/utils.h>
#include <GLFW/glfw3.h>
#include <cstring>
#include <memory>

using namespace donut;
constexpr const char* WindowTitle = "PrismLab | Starter";

class Starter final : public app::IRenderPass
{
public:
    Starter(app::DeviceManager* manager, bool smokeTest)
        : IRenderPass(manager), m_smokeTest(smokeTest) {}

    bool Init()
    {
        const auto shaderPath = app::GetDirectoryWithExecutable()
            / "shaders/starter" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());
        auto fs = std::make_shared<vfs::NativeFileSystem>();
        engine::ShaderFactory shaders(GetDevice(), fs, shaderPath);
        m_vs = shaders.CreateShader("triangle.hlsl", "main_vs", nullptr, nvrhi::ShaderType::Vertex);
        m_ps = shaders.CreateShader("triangle.hlsl", "main_ps", nullptr, nvrhi::ShaderType::Pixel);
        m_commands = GetDevice()->createCommandList();
        return m_vs && m_ps && m_commands;
    }

    void BackBufferResizing() override { m_pipeline = nullptr; }
    void Animate(float) override
    {
        GetDeviceManager()->SetInformativeWindowTitle(WindowTitle);
    }

    void Render(nvrhi::IFramebuffer* framebuffer) override
    {
        if (!m_pipeline)
        {
            nvrhi::GraphicsPipelineDesc desc;
            desc.VS = m_vs;
            desc.PS = m_ps;
            desc.primType = nvrhi::PrimitiveType::TriangleList;
            desc.renderState.depthStencilState.depthTestEnable = false;
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            m_pipeline = GetDevice()->createGraphicsPipeline(desc, framebuffer->getFramebufferInfo());
            if (!m_pipeline)
            {
                m_failed = true;
                log::error("Cannot create the starter graphics pipeline.");
                glfwSetWindowShouldClose(GetDeviceManager()->GetWindow(), GLFW_TRUE);
                return;
            }
        }

        m_commands->open();
        nvrhi::utils::ClearColorAttachment(m_commands, framebuffer, 0,
            nvrhi::Color(0.025f, 0.035f, 0.055f, 1.f));
        nvrhi::GraphicsState state;
        state.pipeline = m_pipeline;
        state.framebuffer = framebuffer;
        state.viewport.addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        m_commands->setGraphicsState(state);
        nvrhi::DrawArguments draw;
        draw.vertexCount = 3;
        m_commands->draw(draw);
        m_commands->close();
        GetDevice()->executeCommandList(m_commands);

        if (m_smokeTest && ++m_frames >= 3)
            glfwSetWindowShouldClose(GetDeviceManager()->GetWindow(), GLFW_TRUE);
    }

    bool Failed() const { return m_failed; }

private:
    nvrhi::ShaderHandle m_vs, m_ps;
    nvrhi::GraphicsPipelineHandle m_pipeline;
    nvrhi::CommandListHandle m_commands;
    bool m_smokeTest = false;
    bool m_failed = false;
    unsigned m_frames = 0;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    bool smokeTest = false;
    for (int i = 1; i < __argc; ++i)
        smokeTest |= std::strcmp(__argv[i], "--smoke-test") == 0;

    std::unique_ptr<app::DeviceManager> manager(
        app::DeviceManager::Create(nvrhi::GraphicsAPI::D3D12));
    if (!manager)
        return 1;

    app::DeviceCreationParameters params;
    params.backBufferWidth = 1280;
    params.backBufferHeight = 720;
    params.vsyncEnabled = true;
#ifdef _DEBUG
    params.enableDebugRuntime = true;
    params.enableNvrhiValidationLayer = true;
#endif
    if (!manager->CreateWindowDeviceAndSwapChain(params, WindowTitle))
    {
        log::error("Cannot initialize D3D12.");
        manager->Shutdown();
        return 1;
    }

    int result = 0;
    {
        Starter sample(manager.get(), smokeTest);
        if (sample.Init())
        {
            manager->AddRenderPassToBack(&sample);
            manager->RunMessageLoop();
            manager->RemoveRenderPass(&sample);
            manager->GetDevice()->waitForIdle();
            result = sample.Failed() ? 1 : 0;
        }
        else
        {
            log::error("Starter initialization failed. Build the shaders before running.");
            result = 1;
        }
    }
    manager->Shutdown();
    return result;
}
