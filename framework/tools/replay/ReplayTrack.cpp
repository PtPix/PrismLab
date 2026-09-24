#include "ReplayTrack.h"
#include <fstream>
#include <cmath>

namespace prism::host
{
    namespace
    {
        Json::Value WriteVector(dm::float3 v)
        { Json::Value j(Json::arrayValue); j.append(v.x); j.append(v.y); j.append(v.z); return j; }
        bool ReadVector(const Json::Value& j, dm::float3& v)
        {
            if (!j.isArray() || j.size() != 3) return false;
            for (unsigned i = 0; i < 3; ++i)
            { if (!j[i].isNumeric() || !std::isfinite(j[i].asDouble())) return false; v[i] = j[i].asFloat(); }
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
    }
    Status ReplayTrack::Save(const std::filesystem::path& path) const
    {
        if (samples.empty()) return Status::Error(ErrorCode::InvalidArgument, "no replay samples to save");
        Json::Value root; root["version"] = 1; root["fixedDelta"] = fixedDelta; root["seed"] = seed;
        root["samples"] = Json::Value(Json::arrayValue);
        for (const auto& s : samples)
        {
            Json::Value j;
            j["position"] = WriteVector(s.camera.position); j["direction"] = WriteVector(s.camera.direction);
            j["up"] = WriteVector(s.camera.up); j["fov"] = s.camera.fovRadians;
            j["near"] = s.camera.nearPlane; j["far"] = s.camera.farPlane;
            j["parameters"] = s.parameters; root["samples"].append(j);
        }
        std::ofstream out(path); out << root;
        return out.good() ? Status::Ok() : Status::Error(ErrorCode::InvalidArgument, "cannot write replay file");
    }
    Status ReplayTrack::Load(const std::filesystem::path& path)
    {
        std::ifstream in(path); Json::Value root; Json::CharReaderBuilder reader; std::string errors;
        if (!in || !Json::parseFromStream(reader, in, &root, &errors))
            return Status::Error(ErrorCode::InvalidArgument, "cannot parse replay: " + errors);
        ReplayTrack candidate;
        if (!root.isObject() || !root["version"].isInt() || root["version"].asInt() != 1 || !root["fixedDelta"].isNumeric() || !root["seed"].isUInt())
            return Status::Error(ErrorCode::InvalidArgument, "invalid replay header");
        candidate.fixedDelta = root["fixedDelta"].asDouble(); candidate.seed = root["seed"].asUInt();
        const auto& list = root["samples"];
        if (!std::isfinite(candidate.fixedDelta) || candidate.fixedDelta <= 0 || candidate.fixedDelta > 1 ||
            !list.isArray() || list.empty() || list.size() > 1000000)
            return Status::Error(ErrorCode::InvalidArgument, "invalid replay duration or sample count");
        for (const auto& j : list)
        {
            ReplaySample s;
            if (!j.isObject() || !ReadVector(j["position"], s.camera.position) || !ReadVector(j["direction"], s.camera.direction) ||
                !ReadVector(j["up"], s.camera.up) || !j["fov"].isNumeric() || !j["near"].isNumeric() || !j["far"].isNumeric())
                return Status::Error(ErrorCode::InvalidArgument, "invalid replay camera");
            s.camera.fovRadians = j["fov"].asFloat(); s.camera.nearPlane = j["near"].asFloat(); s.camera.farPlane = j["far"].asFloat();
            if (!(s.camera.fovRadians > 0 && s.camera.fovRadians < 3.14f && s.camera.nearPlane > 0 &&
                s.camera.farPlane > s.camera.nearPlane && std::isfinite(s.camera.farPlane)) ||
                dm::length(dm::cross(s.camera.direction, s.camera.up)) < 1e-5f)
                return Status::Error(ErrorCode::InvalidArgument, "degenerate replay camera");
            s.camera.direction = dm::normalize(s.camera.direction); s.camera.up = dm::normalize(s.camera.up);
            s.parameters = j["parameters"]; candidate.samples.push_back(std::move(s));
        }
        *this = std::move(candidate); return Status::Ok();
    }
}
