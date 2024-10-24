#include "pch.h"

#include <strstream>
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

CLIENT_API void Setup()
{	
	JSON defaultProfile = ReadJSON("defaultProfile.json");

	JSON profiles;

	try
	{
		for (uintMem i = 0; i < 3; ++i)
		{
			for (uintMem j = 1; j <= 10; ++j)
			{
				uintMem particleCount = j * pow(10, i) * 1000;

				JSON profile = defaultProfile;

				float side = pow((float)particleCount / 8.0f, 1.0f / 3.0f);
				float offset = -side / 2;

				profile["outputFilePath"] = "output" + std::to_string(particleCount) + "p.txt";
				profile["profileName"] = std::to_string(particleCount);

				profile["systemParameters"]["dynamicParticleGenerationParameters"]["spawnVolumeSize"] = std::array<float, 3>({ side, side, side });
				profile["systemParameters"]["dynamicParticleGenerationParameters"]["spawnVolumeOffset"] = std::array<float, 3>({ offset, offset, offset });
				profile["systemParameters"]["staticParticleGenerationParameters"]["spawnVolumeSize"] = std::array<float, 3>({ 60, 60, 60 });
				profile["systemParameters"]["staticParticleGenerationParameters"]["spawnVolumeOffset"] = std::array<float, 3>({ -30, -30, -30 });

				profiles.push_back(profile);
			}
		}

		JSON out;
		out.push_back({ { "profiles", profiles } });

		WriteJSON("../FluidSimulationProject/assets/simulationProfiles/systemProfilingProfiles.json", out);
	}
	catch (const std::exception& ex)
	{
		Debug::Logger::LogError("JSON", "Error creating profiles. Message: \n" + StringView(ex.what(), strlen(ex.what())));
	}	
}