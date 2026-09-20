#include "CommandLine.h"

#include <donut/core/log.h>

#include <cstdlib>

namespace renderlab::host
{
    namespace
    {
        bool Equals(const std::string& value, const char* name)
        {
            return value == name;
        }

        bool StartsWith(const std::string& value, const char* prefix)
        {
            const size_t length = std::strlen(prefix);
            return value.size() >= length && value.compare(0, length, prefix) == 0;
        }
    }

    CommandLine ParseCommandLine(int argc, char** argv)
    {
        CommandLine commandLine;

        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            const bool hasValue = (index + 1) < argc;

            if (Equals(argument, "--config") && hasValue)
            {
                commandLine.configPath = argv[++index];
            }
            else if (Equals(argument, "--scene") && hasValue)
            {
                commandLine.sceneSource = argv[++index];
            }
            else if (Equals(argument, "--asset") && hasValue)
            {
                commandLine.sceneAsset = argv[++index];
            }
            else if (Equals(argument, "--smoke-test"))
            {
                commandLine.smokeTest = true;
                commandLine.smokeTestFrames = 3;
            }
            else if (StartsWith(argument, "--smoke-test="))
            {
                commandLine.smokeTest = true;
                const uint32_t frames = uint32_t(std::strtoul(argument.c_str() + std::strlen("--smoke-test="), nullptr, 10));
                commandLine.smokeTestFrames = (frames > 0) ? frames : 3;
            }
            else if (Equals(argument, "--capture") && hasValue)
            {
                commandLine.capturePath = argv[++index];
            }
            else if (Equals(argument, "--capture-frame") && hasValue)
            {
                commandLine.captureFrame = uint32_t(std::strtoul(argv[++index], nullptr, 10));
            }
            else if (Equals(argument, "--width") && hasValue)
            {
                commandLine.width = uint32_t(std::strtoul(argv[++index], nullptr, 10));
            }
            else if (Equals(argument, "--height") && hasValue)
            {
                commandLine.height = uint32_t(std::strtoul(argv[++index], nullptr, 10));
            }
            else if (Equals(argument, "--no-vsync"))
            {
                commandLine.disableVsync = true;
            }
            else if (Equals(argument, "--no-timing"))
            {
                commandLine.disableGpuTiming = true;
            }
            else if (Equals(argument, "--help") || Equals(argument, "-h") || Equals(argument, "/?"))
            {
                commandLine.showHelp = true;
            }
            else
            {
                commandLine.extraArguments.push_back(argument);
            }
        }

        return commandLine;
    }

    std::string GetCommandLineUsage()
    {
        return
            "Usage: <lab executable> [options]\n"
            "  --config <path>        use a specific JSON config file\n"
            "  --scene <source>       override the scene source: procedural | gltf\n"
            "  --asset <path>         override the scene asset (scene .json, .gltf or .glb)\n"
            "  --smoke-test[=N]       render N frames (default 3) and exit\n"
            "  --capture <path>       save a screenshot and exit (see --capture-frame)\n"
            "  --capture-frame <n>    frame index to capture, default 2\n"
            "  --width <n>            window width\n"
            "  --height <n>           window height\n"
            "  --no-vsync             disable vertical sync\n"
            "  --no-timing            disable GPU timestamp queries\n"
            "  --help                 print this message\n";
    }
}
