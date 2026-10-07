#include "PassSupport.h"

namespace Prism::Gpu
{
	FShaderPass::~FShaderPass()
	{
		if (ShaderLibrary)
			ShaderLibrary->Unregister(this);
	}
	void FShaderPass::Attach(nvrhi::IDevice* InDevice, FShaderLibrary& InShaderLibrary)
	{
		if (ShaderLibrary)
			ShaderLibrary->Unregister(this);
		Device = InDevice;
		ShaderLibrary = &InShaderLibrary;
		BindingCache = std::make_unique<donut::engine::BindingCache>(Device);
		InShaderLibrary.Register(this);
	}
	nvrhi::BindingSetHandle FShaderPass::GetOrCreateBindingSet(const nvrhi::BindingSetDesc& Desc,
															   nvrhi::IBindingLayout* Layout)
	{
		return BindingCache->GetOrCreateBindingSet(Desc, Layout);
	}
	void FShaderPass::ClearBindings()
	{
		if (BindingCache)
			BindingCache->Clear();
	}
} // namespace Prism::Gpu
