#pragma once

// Host layer: process-level assembly.
//
// RunApplication() builds the device, the shader mounts, the shared services, the scene and the two
// render passes, then runs the message loop and tears everything down in the order the graphics API
// requires. An experiment executable is three lines: see host/Entry.cpp.

#include "Experiment.h"

namespace Prism::Host
{
	int RunApplication(std::unique_ptr<IExperiment> Experiment, int Argc, char** Argv);

	// 由 Entry.cpp 使用：调用实验的 CreateExperiment() 工厂。
	int Run(int Argc, char** Argv);
} // namespace Prism::Host
