// Host layer: the shared WinMain.
//
// A lab executable only provides a Lab subclass and the CreateLab() factory (host/Lab.h); everything
// else - window, device, camera, scene, UI, frame loop - comes from rl_host.

#include "Application.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return renderlab::host::Run(__argc, __argv);
}
