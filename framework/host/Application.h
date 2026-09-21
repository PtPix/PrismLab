#pragma once

// Host layer: process-level assembly.
//
// RunApplication() builds the device, the shader mounts, the shared services, the scene and the two
// render passes, then runs the message loop and tears everything down in the order the graphics API
// requires. A lab executable is three lines: see host/Entry.cpp.

#include "Lab.h"

namespace renderlab::host
{
    int RunApplication(std::unique_ptr<Lab> lab, int argc, char** argv);

    // 由 Entry.cpp 使用：调用实验的 CreateLab() 工厂。
    int Run(int argc, char** argv);
}
