#include "ReplayTrack.h"
#include <fstream>
#include <cmath>

namespace Prism::Host
{
	namespace
	{
		Json::Value WriteVector(dm::float3 V)
		{
			Json::Value J(Json::arrayValue);
			J.append(V.x);
			J.append(V.y);
			J.append(V.z);
			return J;
		}
		bool ReadVector(const Json::Value& J, dm::float3& V)
		{
			if (!J.isArray() || J.size() != 3)
				return false;
			for (unsigned I = 0; I < 3; ++I)
			{
				if (!J[I].isNumeric() || !std::isfinite(J[I].asDouble()))
					return false;
				V[I] = J[I].asFloat();
			}
			return std::isfinite(V.x) && std::isfinite(V.y) && std::isfinite(V.z);
		}
	} // namespace
	FStatus FReplayTrack::Save(const std::filesystem::path& Path) const
	{
		if (Samples.empty())
			return FStatus::Error(EErrorCode::InvalidArgument, "no replay samples to save");
		Json::Value Root;
		Root["version"] = 1;
		Root["fixedDelta"] = FixedDelta;
		Root["seed"] = Seed;
		Root["samples"] = Json::Value(Json::arrayValue);
		for (const auto& S : Samples)
		{
			Json::Value J;
			J["position"] = WriteVector(S.Camera.Position);
			J["direction"] = WriteVector(S.Camera.Direction);
			J["up"] = WriteVector(S.Camera.Up);
			J["fov"] = S.Camera.FovRadians;
			J["near"] = S.Camera.NearPlane;
			J["far"] = S.Camera.FarPlane;
			J["parameters"] = S.Parameters;
			Root["samples"].append(J);
		}
		std::ofstream Out(Path);
		Out << Root;
		return Out.good() ? FStatus::Ok() : FStatus::Error(EErrorCode::InvalidArgument, "cannot write replay file");
	}
	FStatus FReplayTrack::Load(const std::filesystem::path& Path)
	{
		std::ifstream In(Path);
		Json::Value Root;
		Json::CharReaderBuilder Reader;
		std::string Errors;
		if (!In || !Json::parseFromStream(Reader, In, &Root, &Errors))
			return FStatus::Error(EErrorCode::InvalidArgument, "cannot parse replay: " + Errors);
		FReplayTrack Candidate;
		if (!Root.isObject() || !Root["version"].isInt() || Root["version"].asInt() != 1 ||
			!Root["fixedDelta"].isNumeric() || !Root["seed"].isUInt())
			return FStatus::Error(EErrorCode::InvalidArgument, "invalid replay header");
		Candidate.FixedDelta = Root["fixedDelta"].asDouble();
		Candidate.Seed = Root["seed"].asUInt();
		const auto& List = Root["samples"];
		if (!std::isfinite(Candidate.FixedDelta) || Candidate.FixedDelta <= 0 || Candidate.FixedDelta > 1 ||
			!List.isArray() || List.empty() || List.size() > 1000000)
			return FStatus::Error(EErrorCode::InvalidArgument, "invalid replay duration or sample count");
		for (const auto& J : List)
		{
			FReplaySample S;
			if (!J.isObject() || !ReadVector(J["position"], S.Camera.Position) ||
				!ReadVector(J["direction"], S.Camera.Direction) || !ReadVector(J["up"], S.Camera.Up) ||
				!J["fov"].isNumeric() || !J["near"].isNumeric() || !J["far"].isNumeric())
				return FStatus::Error(EErrorCode::InvalidArgument, "invalid replay camera");
			S.Camera.FovRadians = J["fov"].asFloat();
			S.Camera.NearPlane = J["near"].asFloat();
			S.Camera.FarPlane = J["far"].asFloat();
			if (!(S.Camera.FovRadians > 0 && S.Camera.FovRadians < 3.14f && S.Camera.NearPlane > 0 &&
				  S.Camera.FarPlane > S.Camera.NearPlane && std::isfinite(S.Camera.FarPlane)) ||
				dm::length(dm::cross(S.Camera.Direction, S.Camera.Up)) < 1e-5f)
				return FStatus::Error(EErrorCode::InvalidArgument, "degenerate replay camera");
			S.Camera.Direction = dm::normalize(S.Camera.Direction);
			S.Camera.Up = dm::normalize(S.Camera.Up);
			S.Parameters = J["parameters"];
			Candidate.Samples.push_back(std::move(S));
		}
		*this = std::move(Candidate);
		return FStatus::Ok();
	}
} // namespace Prism::Host
