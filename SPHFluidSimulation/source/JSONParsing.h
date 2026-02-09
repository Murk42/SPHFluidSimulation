#pragma once
#include "json.hpp"

namespace JSON
{
	static nlohmann::json FromStream(ReadStream& stream)
	{
		std::string jsonString;
		jsonString.resize(stream.GetSize());
		stream.Read(jsonString.data(), jsonString.size());

		try
		{
			return nlohmann::json::parse(jsonString);
		}
		catch (nlohmann::json::parse_error& ex)
		{
			Debug::Logger::LogError("SPH Library", "Failed to parse JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));
			throw;
		}
	}
	static nlohmann::json FromString(StringView string)
	{
		try
		{
			return nlohmann::json::parse(std::string_view(string.Ptr(), string.Count()));
		}
		catch (nlohmann::json::parse_error& ex)
		{
			Debug::Logger::LogError("SPH Library", "Failed to parse JSON with message: \n" + StringView(ex.what(), strlen(ex.what())));
			return { };
		}
	}
	static String AsString(const nlohmann::json& json)
	{
		std::string s = json.dump();
		return String(s.data(), s.size());
	}

	template<typename T>
	static T Expect(const nlohmann::json& json);

	template<typename T>
	static T Expect(const nlohmann::json& json, StringView name)
	{
		auto it = json.find(name.Ptr());

		if (it == json.end())
		{
			Debug::Logger::LogError("SPH Library", Format("Expected a entry in a JSON named \"{}\"", name));
			throw;
		}

		return Expect<T>(*it);
	}
	template<>
	static const nlohmann::json& Expect<const nlohmann::json&>(const nlohmann::json& json)
	{
		return json;
	}
	template<>
	static float Expect<float>(const nlohmann::json& json)
	{
		if (!json.is_number())
		{
			Debug::Logger::LogError("SPH Library", "Failed to convert JSON entry. Expected it to be a float");
			throw;
		}

		return (float)json;
	}
	template<>
	static String Expect<String>(const nlohmann::json& json)
	{
		if (!json.is_string())
		{
			Debug::Logger::LogError("SPH Library", "Failed to convert JSON entry. Expected it to be a string");
			throw;
		}

		std::string s = json;

		return String(s.data(), s.size());
	}
	template<>
	static Vec3f Expect<Vec3f>(const nlohmann::json& json)
	{
		if (!json.is_array() || json.size() != 3 || !json.at(0).is_number() || !json.at(1).is_number() || !json.at(2).is_number())
		{
			Debug::Logger::LogError("SPH Library", "Failed to convert JSON entry. Expected it to be an array of 3 floats");
			throw;
		}

		return { json[0], json[1], json[2] };
	}
	template<>
	static uint32 Expect<uint32>(const nlohmann::json& json)
	{
		if (!json.is_number_unsigned())
		{
			Debug::Logger::LogError("SPH Library", "Failed to convert JSON entry. Expected it to be an unsigned integer");
			throw;
		}

		return json;
	}

	static bool HasEntry(const nlohmann::json & json, StringView name)
	{
		auto it = json.find(name.Ptr());

		if (it != json.end())
			return true;
		return false;
	}
}