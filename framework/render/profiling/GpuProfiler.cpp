#include "GpuProfiler.h"

#include <donut/core/log.h>

#include <algorithm>

namespace Prism::Gpu
{
	FGpuProfiler::FGpuProfiler(nvrhi::IDevice* InDevice, uint32_t InMaxScopes)
		: Device(InDevice), MaxScopes(InMaxScopes)
	{
	}

	FGpuProfiler::FScope* FGpuProfiler::FindOrCreateScope(const char* Name)
	{
		if (!Name)
			return nullptr;

		for (FScope& Scope : Scopes)
		{
			if (Scope.Name == Name)
				return &Scope;
		}

		if (Scopes.size() >= MaxScopes)
		{
			donut::log::warning("GpuProfiler: scope '%s' exceeds the limit of %u scopes.", Name, MaxScopes);
			return nullptr;
		}

		FScope Scope;
		Scope.Name = Name;
		Scope.Queries.reserve(KFramesInFlight);

		for (uint32_t I = 0; I < KFramesInFlight; ++I)
			Scope.Queries.push_back(Device->createTimerQuery());

		Scopes.push_back(std::move(Scope));

		FScopeTiming Timing;
		Timing.Name = Name;
		Timings.push_back(std::move(Timing));

		return &Scopes.back();
	}

	void FGpuProfiler::BeginFrame(nvrhi::ICommandList* Commands)
	{
		(void)Commands;
		FrameSlot = uint32_t(FrameCount % KFramesInFlight);
		for (size_t I = 0; I < Scopes.size(); ++I)
		{
			auto& Scope = Scopes[I];
			for (uint32_t Slot = 0; Slot < KFramesInFlight; ++Slot)
			{
				if (!Scope.bPending[Slot] || !Device->pollTimerQuery(Scope.Queries[Slot]))
					continue;
				const float Ms = Device->getTimerQueryTime(Scope.Queries[Slot]) * 1000.f;
				Device->resetTimerQuery(Scope.Queries[Slot]);
				Scope.bPending[Slot] = false;
				auto& Timing = Timings[I];
				if (Scope.Frames[Slot] < MinResultFrame || (Timing.bValid && Scope.Frames[Slot] <= Timing.FrameIndex))
					continue;
				Timing.Milliseconds = Ms;
				Timing.SmoothedMilliseconds = Timing.bValid ? Timing.SmoothedMilliseconds * 0.9f + Ms * 0.1f : Ms;
				Timing.FrameIndex = Scope.Frames[Slot];
				Timing.bValid = true;
			}
		}
		bFrameOpen = true;
	}

	void FGpuProfiler::EndFrame()
	{
		assert(Stack.empty());
		bFrameOpen = false;
		++FrameCount;
	}

	void FGpuProfiler::BeginScope(nvrhi::ICommandList* Commands, const char* Name)
	{
		Stack.push_back(-1);
		if (!bEnabled || !bFrameOpen || !Commands)
			return;
		auto* Scope = FindOrCreateScope(Name);
		if (!Scope || Scope->bActive || Scope->bPending[FrameSlot] || Scope->LastRecordedFrame == FrameCount)
			return;
		Stack.back() = int(Scope - Scopes.data());
		Commands->beginTimerQuery(Scope->Queries[FrameSlot]);
		Scope->bActive = true;
		Scope->LastRecordedFrame = FrameCount;
		Scope->Frames[FrameSlot] = FrameCount;
	}

	void FGpuProfiler::EndScope(nvrhi::ICommandList* Commands)
	{
		if (Stack.empty())
			return;
		int Index = Stack.back();
		Stack.pop_back();
		if (Index < 0 || !Commands)
			return;
		auto& Scope = Scopes[size_t(Index)];
		Commands->endTimerQuery(Scope.Queries[FrameSlot]);
		Scope.bActive = false;
		Scope.bPending[FrameSlot] = true;
	}

	float FGpuProfiler::GetTotalMilliseconds() const
	{
		for (const auto& Timing : Timings)
			if (Timing.Name == "Frame" && Timing.bValid)
				return Timing.Milliseconds;
		return 0.f;
	}

	void FGpuProfiler::SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	void FGpuProfiler::Reset()
	{
		MinResultFrame = FrameCount;
		for (auto& Timing : Timings)
			Timing.bValid = false;
	}
} // namespace Prism::Gpu
