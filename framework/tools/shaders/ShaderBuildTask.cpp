#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "ShaderBuildTask.h"
#include <fstream>
#include <iterator>

namespace Prism::Host
{
	namespace
	{
		std::wstring Quote(const std::wstring& Text)
		{
			std::wstring Result = L"\"";
			size_t Slashes = 0;
			for (wchar_t C : Text)
			{
				if (C == L'\\')
				{
					++Slashes;
					continue;
				}
				Result.append(C == L'\"' ? Slashes * 2 + 1 : Slashes, L'\\');
				Slashes = 0;
				Result += C;
			}
			Result.append(Slashes * 2, L'\\');
			return Result + L'\"';
		}
	} // namespace
	FShaderBuildTask::~FShaderBuildTask()
	{
		if (JobHandle)
			CloseHandle(JobHandle);
		if (ProcessHandle)
			CloseHandle(ProcessHandle);
	}
	bool FShaderBuildTask::Start(const std::filesystem::path& InScript, const std::filesystem::path& InOutput)
	{
		if (Running())
			return false;
		ExitCode = 1;
		BuildLog.clear();
		std::error_code Ec;
		std::filesystem::create_directories(InOutput, Ec);
		if (Ec || !std::filesystem::exists(InScript))
		{
			BuildLog = "Reload script/output unavailable. Build the sample first.";
			return false;
		}
		LogPath = InOutput / "compile.log";
		SECURITY_ATTRIBUTES Security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
		HANDLE LogFile = CreateFileW(LogPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &Security,
									 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (LogFile == INVALID_HANDLE_VALUE)
		{
			BuildLog = "Cannot open shader build log.";
			return false;
		}
		HANDLE Input =
			CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &Security, OPEN_EXISTING, 0, nullptr);
		STARTUPINFOW Startup{};
		Startup.cb = sizeof(Startup);
		Startup.dwFlags = STARTF_USESTDHANDLES;
		Startup.hStdOutput = LogFile;
		Startup.hStdError = LogFile;
		Startup.hStdInput = Input;
		PROCESS_INFORMATION ProcessInformation{};
		const std::filesystem::path Cmake = PRISM_CMAKE_PATH;
		std::wstring Command = Quote(Cmake.wstring()) + L" " + Quote(L"-DPRISM_RELOAD_OUTPUT=" + InOutput.wstring()) +
							   L" -P " + Quote(InScript.wstring());
		HANDLE NewJobHandle = CreateJobObjectW(nullptr, nullptr);
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limit{};
		Limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		const bool bConfigured =
			NewJobHandle &&
			SetInformationJobObject(NewJobHandle, JobObjectExtendedLimitInformation, &Limit, sizeof(Limit));
		const bool bCreated =
			bConfigured &&
			CreateProcessW(Cmake.c_str(), Command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
						   nullptr, InScript.parent_path().c_str(), &Startup, &ProcessInformation);
		CloseHandle(LogFile);
		if (Input != INVALID_HANDLE_VALUE)
			CloseHandle(Input);
		if (!bCreated)
		{
			if (NewJobHandle)
				CloseHandle(NewJobHandle);
			BuildLog = "Cannot launch shader build process.";
			return false;
		}
		if (!AssignProcessToJobObject(NewJobHandle, ProcessInformation.hProcess))
		{
			TerminateProcess(ProcessInformation.hProcess, 1);
			CloseHandle(ProcessInformation.hThread);
			CloseHandle(ProcessInformation.hProcess);
			CloseHandle(NewJobHandle);
			BuildLog = "Cannot attach shader build process to its job.";
			return false;
		}
		ResumeThread(ProcessInformation.hThread);
		CloseHandle(ProcessInformation.hThread);
		ProcessHandle = ProcessInformation.hProcess;
		JobHandle = NewJobHandle;
		return true;
	}
	bool FShaderBuildTask::Poll()
	{
		if (!ProcessHandle || WaitForSingleObject(ProcessHandle, 0) != WAIT_OBJECT_0)
			return false;
		GetExitCodeProcess(ProcessHandle, &ExitCode);
		CloseHandle(ProcessHandle);
		ProcessHandle = nullptr;
		CloseHandle(JobHandle);
		JobHandle = nullptr;
		std::ifstream Stream(LogPath);
		BuildLog.assign(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
		return true;
	}
} // namespace Prism::Host
