#include "CommandLine.h"

#include <donut/core/log.h>

#include <cstdlib>

namespace prism::host
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
            else if (Equals(argument, "--write-reference") && hasValue)
            {
                commandLine.writeReferencePath = argv[++index];
            }
            else if (Equals(argument, "--reference") && hasValue)
            {
                commandLine.referencePath = argv[++index];
            }
            else if (Equals(argument, "--tolerance") && hasValue)
            {
                commandLine.tolerance = float(std::strtod(argv[++index], nullptr));
            }
            else if (Equals(argument, "--bench"))
            {
                commandLine.benchFrames = 120;
            }
            else if (StartsWith(argument, "--bench="))
            {
                const uint32_t frames = uint32_t(std::strtoul(argument.c_str() + std::strlen("--bench="), nullptr, 10));
                commandLine.benchFrames = (frames > 0) ? frames : 120;
            }
            else if (Equals(argument, "--bench-warmup"))
            {
                commandLine.benchWarmup = 30;
            }
            else if (StartsWith(argument, "--bench-warmup="))
            {
                commandLine.benchWarmup = uint32_t(std::strtoul(argument.c_str() + std::strlen("--bench-warmup="), nullptr, 10));
            }
            else if (Equals(argument, "--metrics") && hasValue)
            {
                commandLine.metricsPath = argv[++index];
            }
            else if (Equals(argument, "--debug-view") && hasValue)
            {
                commandLine.debugView = int(std::strtol(argv[++index], nullptr, 10));
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
            "Usage: <experiment executable> [options]\n"
            "  --config <path>        use a specific JSON config file\n"
            "  --scene <source>       override the scene source: procedural | gltf\n"
            "  --asset <path>         override the scene asset (scene .json, .gltf or .glb)\n"
            "  --smoke-test[=N]       render N frames (default 3) and exit\n"
            "  --capture <path>       save a screenshot and exit (see --capture-frame)\n"
            "  --capture-frame <n>    frame index to capture, default 2\n"
            "  --write-reference <path>  write the captured frame as a float reference (.f32) and exit\n"
            "  --reference <path>     compare the captured frame against a float reference; non-zero exit on mismatch\n"
            "  --tolerance <v>        tolerance for --reference, default 0.01\n"
            "  --bench[=N]            measure N frames (default 120) after warmup, write the metrics CSV, exit\n"
            "  --bench-warmup[=N]     warmup frames for --bench, default 30\n"
            "  --metrics <path>       metrics CSV path (used with --bench or --smoke-test)\n"
            "  --debug-view <n>       show the n-th published intermediate result (0 = experiment output)\n"
            "  --width <n>            window width\n"
            "  --height <n>           window height\n"
            "  --no-vsync             disable vertical sync\n"
            "  --no-timing            disable GPU timestamp queries\n"
            "  --help                 print this message\n";
    }
}
