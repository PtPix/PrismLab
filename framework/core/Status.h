#pragma once

// Framework types: error reporting without exceptions and without C++23 std::expected.
//
// An experiment reports failure with a Status carrying the module name and a reason, so that a failed
// experiment is not silently replaced by a different algorithm.

#include <string>
#include <utility>

namespace Prism
{
	enum class EErrorCode
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

	inline const char* ToString(EErrorCode Code)
	{
		switch (Code)
		{
			case EErrorCode::Ok:
				return "ok";
			case EErrorCode::InvalidArgument:
				return "invalid argument";
			case EErrorCode::NotInitialized:
				return "not initialized";
			case EErrorCode::ResourceMissing:
				return "resource missing";
			case EErrorCode::FormatMismatch:
				return "format mismatch";
			case EErrorCode::ExtentMismatch:
				return "extent mismatch";
			case EErrorCode::Unsupported:
				return "unsupported";
			case EErrorCode::ShaderCompileFailed:
				return "shader compile failed";
			case EErrorCode::PipelineCreationFailed:
				return "pipeline creation failed";
			case EErrorCode::DeviceError:
				return "device error";
			case EErrorCode::Internal:
				return "internal error";
			default:
				return "unknown";
		}
	}

	// [[nodiscard]]: an ignored failure would silently continue with a broken pipeline or resource.
	class [[nodiscard]] FStatus
	{
	  public:
		FStatus() = default;

		static FStatus Ok()
		{
			return FStatus();
		}

		static FStatus Error(EErrorCode Code, std::string Message)
		{
			FStatus Status;
			Status.Code = Code;
			Status.Message = std::move(Message);
			return Status;
		}

		[[nodiscard]] bool IsOk() const
		{
			return Code == EErrorCode::Ok;
		}
		[[nodiscard]] bool IsError() const
		{
			return Code != EErrorCode::Ok;
		}
		[[nodiscard]] EErrorCode GetCode() const
		{
			return Code;
		}
		[[nodiscard]] const std::string& GetMessage() const
		{
			return Message;
		}

		// "invalid argument: extent must be positive"
		[[nodiscard]] std::string ToStringWithCode() const
		{
			if (IsOk())
				return "ok";

			return std::string(Prism::ToString(Code)) + ": " + Message;
		}

		explicit operator bool() const
		{
			return IsOk();
		}

	  private:
		EErrorCode Code = EErrorCode::Ok;
		std::string Message;
	};

	template <typename InValueType> class [[nodiscard]] TResult
	{
	  public:
		TResult(InValueType InValue) : StoredValue(std::move(InValue)), ResultStatus(FStatus::Ok())
		{
		}

		TResult(FStatus InStatus) : ResultStatus(std::move(InStatus))
		{
		}

		[[nodiscard]] bool IsOk() const
		{
			return ResultStatus.IsOk();
		}
		[[nodiscard]] const FStatus& GetStatus() const
		{
			return ResultStatus;
		}
		[[nodiscard]] InValueType& GetValue()
		{
			return StoredValue;
		}
		[[nodiscard]] const InValueType& GetValue() const
		{
			return StoredValue;
		}
		InValueType&& TakeValue()
		{
			return std::move(StoredValue);
		}

		explicit operator bool() const
		{
			return IsOk();
		}

	  private:
		InValueType StoredValue{};
		FStatus ResultStatus;
	};

	// Helper to attach context to an error status of a nested call.
	inline FStatus WithContext(const FStatus& Status, const std::string& Context)
	{
		if (Status.IsOk())
			return Status;

		return FStatus::Error(Status.GetCode(), Context + ": " + Status.GetMessage());
	}
} // namespace Prism
