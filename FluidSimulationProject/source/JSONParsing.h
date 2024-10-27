#pragma once

static String ConvertString(const std::string& string)
{
	return String(string.data(), string.size());
}

static Vec3f ConvertVec3f(auto& json)
{
	return Vec3f(json[0], json[1], json[2]);
}