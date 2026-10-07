#include <filesystem>
bool RunReplayTests(const std::filesystem::path& Directory);
int main(int argc, char** argv)
{
	auto Directory = std::filesystem::absolute(argv[0]).parent_path() / "cpu-test-output";
	std::filesystem::create_directories(Directory);
	(void)argc;
	return RunReplayTests(Directory) ? 0 : 1;
}
