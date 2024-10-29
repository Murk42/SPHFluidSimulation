#include "pch.h"

#include <strstream>
#include <array>
#include "json.hpp"
using namespace Blaze;

using JSON = nlohmann::json;

JSON ReadJSON(Path path)
{
	try
	{
		File file{ path, FileAccessPermission::Read };

		std::string s;
		s.resize(file.GetSize());
		file.Read(s.data(), s.size());

		auto out = JSON::parse(s);
		return out;
	}
	catch (const std::exception& ex)
	{
		Debug::Logger::LogError("JSON", "Error reading JSON file. Message: \n" + StringView(ex.what(), strlen(ex.what())));
	}
	return {};
}

void WriteJSON(Path path, const JSON& json)
{
	try
	{
		File file{ path, FileAccessPermission::Write, FileOpenParameters {
			.openOption = FileOpenOptions::CreateAlways
		} };

		std::string s = nlohmann::to_string(json);

		file.Write(s.data(), s.size());

	}
	catch (const std::exception& ex)
	{
		Debug::Logger::LogError("JSON", "Error writing to JSON file. Message: \n" + StringView(ex.what(), strlen(ex.what())));
	}
}

JSON SetupParticleTest()
{
	JSON profiles;

	JSON defaultProfile = ReadJSON("defaultProfile.json");

	float sizes[]{ 60, 120, 240 };
	for (uintMem i = 0; i < 3; ++i)
	{		
		JSON profile = defaultProfile;		

		profile["outputFilePath"] = "outputs/" + (std::string)"increasingStaticParticleCount" + std::to_string(i) + ".txt";
		profile["profileName"] = "increasingStaticParticleCount" + std::to_string(i);

		profile["systemParameters"]["staticParticleGenerationParameters"]["spawnVolumeOffset"] = std::array<float, 3>({ -sizes[i] / 2, -1, -sizes[i] / 2 });
		profile["systemParameters"]["staticParticleGenerationParameters"]["spawnVolumeSize"] = std::array<float, 3>({ sizes[i], sizes[i], sizes[i] });

		profiles.push_back(profile);
	}

	return profiles;
}

CLIENT_API void Setup()
{		

	try
	{		
		JSON profiles = SetupParticleTest();

		JSON out;
		out.push_back({ { "profiles", profiles } });

		WriteJSON("../FluidSimulationProject/assets/simulationProfiles/increasingStaticParticleCountProfiles.json", out);
	}
	catch (const std::exception& ex)
	{
		Debug::Logger::LogError("JSON", "Error creating profiles. Message: \n" + StringView(ex.what(), strlen(ex.what())));
	}	
}