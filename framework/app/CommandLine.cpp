#include "CommandLine.h"

#include <donut/core/log.h>

#include <cstdlib>

namespace Prism::Host
{
	namespace
	{
		bool Equals(const std::string& Value, const char* Name)
		{
			return Value == Name;
		}

		bool StartsWith(const std::string& Value, const char* Prefix)
		{
			const size_t Length = std::strlen(Prefix);
			return Value.size() >= Length && Value.compare(0, Length, Prefix) == 0;
		}
	} // namespace

	FCommandLine ParseCommandLine(int Argc, char** Argv)
	{
		FCommandLine CommandLine;

		for (int Index = 1; Index < Argc; ++Index)
		{
			const std::string Argument = Argv[Index];
			const bool bHasValue = (Index + 1) < Argc;

			if (Equals(Argument, "--config") && bHasValue)
			{
				CommandLine.ConfigPath = Argv[++Index];
			}
			else if (Equals(Argument, "--scene") && bHasValue)
			{
				CommandLine.SceneSource = Argv[++Index];
			}
			else if (Equals(Argument, "--asset") && bHasValue)
			{
				CommandLine.SceneAsset = Argv[++Index];
			}
			else if (Equals(Argument, "--capture") && bHasValue)
			{
				CommandLine.CapturePath = Argv[++Index];
			}
			else if (Equals(Argument, "--capture-frame") && bHasValue)
			{
				CommandLine.CaptureFrame = uint32_t(std::strtoul(Argv[++Index], nullptr, 10));
			}
			else if (Equals(Argument, "--write-reference") && bHasValue)
			{
				CommandLine.WriteReferencePath = Argv[++Index];
			}
			else if (Equals(Argument, "--reference") && bHasValue)
			{
				CommandLine.ReferencePath = Argv[++Index];
			}
			else if (Equals(Argument, "--tolerance") && bHasValue)
			{
				CommandLine.Tolerance = float(std::strtod(Argv[++Index], nullptr));
			}
			else if (Equals(Argument, "--bench"))
			{
				CommandLine.BenchFrames = 120;
			}
			else if (StartsWith(Argument, "--bench="))
			{
				const uint32_t Frames = uint32_t(std::strtoul(Argument.c_str() + std::strlen("--bench="), nullptr, 10));
				CommandLine.BenchFrames = (Frames > 0) ? Frames : 120;
			}
			else if (Equals(Argument, "--bench-warmup"))
			{
				CommandLine.BenchWarmup = 30;
			}
			else if (StartsWith(Argument, "--bench-warmup="))
			{
				CommandLine.BenchWarmup =
					uint32_t(std::strtoul(Argument.c_str() + std::strlen("--bench-warmup="), nullptr, 10));
			}
			else if (Equals(Argument, "--metrics") && bHasValue)
			{
				CommandLine.MetricsPath = Argv[++Index];
			}
			else if (Equals(Argument, "--debug-view") && bHasValue)
			{
				CommandLine.DebugView = int(std::strtol(Argv[++Index], nullptr, 10));
			}
			else if (Equals(Argument, "--width") && bHasValue)
			{
				CommandLine.Width = uint32_t(std::strtoul(Argv[++Index], nullptr, 10));
			}
			else if (Equals(Argument, "--height") && bHasValue)
			{
				CommandLine.Height = uint32_t(std::strtoul(Argv[++Index], nullptr, 10));
			}
			else if (Equals(Argument, "--no-vsync"))
			{
				CommandLine.bDisableVsync = true;
			}
			else if (Equals(Argument, "--no-timing"))
			{
				CommandLine.bDisableGpuTiming = true;
			}
			else if (Equals(Argument, "--help") || Equals(Argument, "-h") || Equals(Argument, "/?"))
			{
				CommandLine.bShowHelp = true;
			}
			else
			{
				CommandLine.ExtraArguments.push_back(Argument);
			}
		}

		return CommandLine;
	}

	std::string GetCommandLineUsage()
	{
		return "Usage: <experiment executable> [options]\n"
			   "  --config <path>        use a specific JSON config file\n"
			   "  --scene <source>       override the scene source: procedural | gltf\n"
			   "  --asset <path>         override the scene asset (scene .json, .gltf or .glb)\n"
			   "  --capture <path>       save a screenshot and exit (see --capture-frame)\n"
			   "  --capture-frame <n>    frame index to capture, default 2\n"
			   "  --write-reference <path>  write the captured frame as a float reference (.f32) and exit\n"
			   "  --reference <path>     compare the captured frame against a float reference; non-zero exit on "
			   "mismatch\n"
			   "  --tolerance <v>        tolerance for --reference, default 0.01\n"
			   "  --bench[=N]            measure N frames (default 120) after warmup, write the metrics CSV, exit\n"
			   "  --bench-warmup[=N]     warmup frames for --bench, default 30\n"
			   "  --metrics <path>       metrics CSV path (used with --bench)\n"
			   "  --debug-view <n>       show the n-th published intermediate result (0 = experiment output)\n"
			   "  --width <n>            window width\n"
			   "  --height <n>           window height\n"
			   "  --no-vsync             disable vertical sync\n"
			   "  --no-timing            disable GPU timestamp queries\n"
			   "  --help                 print this message\n";
	}
} // namespace Prism::Host
