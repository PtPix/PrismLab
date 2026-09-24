#pragma once
#include <framework/render/RenderServices.h>
#include <framework/render/data/ColorSpace.h>
namespace prism::gpu
{
    struct DisplayInput
    {
        nvrhi::ITexture* sceneColor = nullptr;
        ColorSpace colorSpace = ColorSpace::SceneLinear;
        nvrhi::IFramebuffer* outputTarget = nullptr;
        Extent2D outputSize;
        float deltaTimeSeconds = 0.f;
        uint64_t frameIndex = 0;
    };
    // The sample owns display algorithms and their UI. Targets may be offscreen.
    class IDisplayChain
    {
    public:
        virtual ~IDisplayChain() = default;
        virtual Status Initialize(RenderServices& services) = 0;
        virtual Status Record(RenderServices& services, nvrhi::ICommandList* commands, const DisplayInput& input) = 0;
        virtual void OnOutputResized(RenderServices& services, Extent2D size) = 0;
    };
}
