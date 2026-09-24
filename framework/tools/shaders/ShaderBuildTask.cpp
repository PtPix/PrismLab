#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "ShaderBuildTask.h"
#include <fstream>
#include <iterator>

namespace prism::host
{
    namespace
    {
        std::wstring Quote(const std::wstring& text)
        {
            std::wstring result = L"\"";
            size_t slashes = 0;
            for (wchar_t c : text)
            {
                if (c == L'\\') { ++slashes; continue; }
                result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
                slashes = 0; result += c;
            }
            result.append(slashes * 2, L'\\');
            return result + L'\"';
        }
    }
    ShaderBuildTask::~ShaderBuildTask()
    {
        if (m_Job) CloseHandle(m_Job);
        if (m_Process) CloseHandle(m_Process);
    }
    bool ShaderBuildTask::Start(const std::filesystem::path& script, const std::filesystem::path& output)
    {
        if (Running()) return false;
        m_ExitCode = 1; m_Log.clear();
        std::error_code ec;
        std::filesystem::create_directories(output, ec);
        if (ec || !std::filesystem::exists(script)) { m_Log = "Reload script/output unavailable. Build the sample first."; return false; }
        m_LogPath = output / "compile.log";
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE log = CreateFileW(m_LogPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log == INVALID_HANDLE_VALUE) { m_Log = "Cannot open shader build log."; return false; }
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = log; startup.hStdError = log; startup.hStdInput = input;
        PROCESS_INFORMATION process{};
        const std::filesystem::path cmake = PRISM_CMAKE_PATH;
        std::wstring command = Quote(cmake.wstring()) + L" " + Quote(L"-DPRISM_RELOAD_OUTPUT=" + output.wstring()) + L" -P " + Quote(script.wstring());
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        const bool configured = job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit, sizeof(limit));
        const bool created = configured && CreateProcessW(cmake.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, script.parent_path().c_str(), &startup, &process);
        CloseHandle(log); if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        if (!created) { if (job) CloseHandle(job); m_Log = "Cannot launch shader build process."; return false; }
        if (!AssignProcessToJobObject(job, process.hProcess))
        {
            TerminateProcess(process.hProcess, 1); CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job);
            m_Log = "Cannot attach shader build process to its job."; return false;
        }
        ResumeThread(process.hThread); CloseHandle(process.hThread);
        m_Process = process.hProcess; m_Job = job;
        return true;
    }
    bool ShaderBuildTask::Poll()
    {
        if (!m_Process || WaitForSingleObject(m_Process, 0) != WAIT_OBJECT_0) return false;
        GetExitCodeProcess(m_Process, &m_ExitCode);
        CloseHandle(m_Process); m_Process = nullptr;
        CloseHandle(m_Job); m_Job = nullptr;
        std::ifstream stream(m_LogPath);
        m_Log.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        return true;
    }
}
