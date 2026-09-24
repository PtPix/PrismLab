// Host layer: the shared WinMain.
//
// An experiment executable only provides an Experiment subclass and the CreateExperiment() factory (host/Experiment.h); everything
// else - window, device, camera, scene, UI, frame loop - comes from prism_app.

#include "Application.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return prism::host::Run(__argc, __argv);
}
