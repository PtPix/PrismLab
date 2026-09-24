#pragma once

// Framework types: error reporting without exceptions and without C++23 std::expected.
//
// An experiment reports failure with a Status carrying the module name and a reason, so that a failed
// experiment is not silently replaced by a different algorithm.

#include <string>
#include <utility>

namespace prism
{
    enum class ErrorCode
    {
        Ok = 0,
        InvalidArgument,
        NotInitialized,
        ResourceMissing,
        FormatMismatch,
        ExtentMismatch,
        Unsupported,
        ShaderCompileFailed,
        PipelineCreationFailed,
        DeviceError,
        Internal,
    };

    inline const char* ToString(ErrorCode code)
    {
        switch (code)
        {
        case ErrorCode::Ok:                    return "ok";
        case ErrorCode::InvalidArgument:       return "invalid argument";
        case ErrorCode::NotInitialized:        return "not initialized";
        case ErrorCode::ResourceMissing:       return "resource missing";
        case ErrorCode::FormatMismatch:        return "format mismatch";
        case ErrorCode::ExtentMismatch:        return "extent mismatch";
        case ErrorCode::Unsupported:           return "unsupported";
        case ErrorCode::ShaderCompileFailed:   return "shader compile failed";
        case ErrorCode::PipelineCreationFailed:return "pipeline creation failed";
        case ErrorCode::DeviceError:           return "device error";
        case ErrorCode::Internal:              return "internal error";
        default:                               return "unknown";
        }
    }

    // [[nodiscard]]: an ignored failure would silently continue with a broken pipeline or resource.
    class [[nodiscard]] Status
    {
    public:
        Status() = default;

        static Status Ok() { return Status(); }

        static Status Error(ErrorCode code, std::string message)
        {
            Status status;
            status.m_Code = code;
            status.m_Message = std::move(message);
            return status;
        }

        [[nodiscard]] bool IsOk() const { return m_Code == ErrorCode::Ok; }
        [[nodiscard]] bool IsError() const { return m_Code != ErrorCode::Ok; }
        [[nodiscard]] ErrorCode GetCode() const { return m_Code; }
        [[nodiscard]] const std::string& GetMessage() const { return m_Message; }

        // "invalid argument: extent must be positive"
        [[nodiscard]] std::string ToStringWithCode() const
        {
            if (IsOk())
                return "ok";

            return std::string(prism::ToString(m_Code)) + ": " + m_Message;
        }

        explicit operator bool() const { return IsOk(); }

    private:
        ErrorCode m_Code = ErrorCode::Ok;
        std::string m_Message;
    };

    template <typename T>
    class [[nodiscard]] Result
    {
    public:
        Result(T value)
            : m_Value(std::move(value))
            , m_Status(Status::Ok())
        {}

        Result(Status status)
            : m_Status(std::move(status))
        {}

        [[nodiscard]] bool IsOk() const { return m_Status.IsOk(); }
        [[nodiscard]] const Status& GetStatus() const { return m_Status; }
        [[nodiscard]] T& Value() { return m_Value; }
        [[nodiscard]] const T& Value() const { return m_Value; }
        T&& TakeValue() { return std::move(m_Value); }

        explicit operator bool() const { return IsOk(); }

    private:
        T m_Value{};
        Status m_Status;
    };

    // Helper to attach context to an error status of a nested call.
    inline Status WithContext(const Status& status, const std::string& context)
    {
        if (status.IsOk())
            return status;

        return Status::Error(status.GetCode(), context + ": " + status.GetMessage());
    }
}
