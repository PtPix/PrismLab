#include <filesystem>
bool RunReplayTests(const std::filesystem::path& directory);
int main(int argc, char** argv)
{
    auto directory = std::filesystem::absolute(argv[0]).parent_path() / "cpu-test-output";
    std::filesystem::create_directories(directory);
    (void)argc;
    return RunReplayTests(directory) ? 0 : 1;
}
