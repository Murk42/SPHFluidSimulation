#include "pch.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"


ProfilingScene::ProfilingScene(OpenCLContext& clContext, cl_command_queue clQueue, Graphics::OpenGL::RenderWindow_OpenGL& window) :
	clContext(clContext), graphicsContext(window.GetGraphicsContext()), window(window),
	SPHSystemGPU(clContext.context, clContext.device, clQueue),
	SPHSystemCPU(std::thread::hardware_concurrency()),
	GPUDynamicParticlesBufferManager(clContext.context, clContext.device, clQueue),
	GPUStaticParticlesBufferManager(clContext.context, clContext.device, clQueue),
	texturedRectRenderer(graphicsContext)
{
	fontManager->AddFontFace("default", resourceManager.LoadResource<UI::FontFace>("default", "assets/fonts/Hack-Regular.ttf", 0));
	fontManager->CreateFontAtlas("default", { 16, 32, 64 }, antialiasedTextRenderer);
	UISystem.SetScreen<ProfilingUI>(resourceManager, *this);

	SPHSystems.AddBack(SimulationData{ SPHSystemGPU, GPUDynamicParticlesBufferManager, GPUStaticParticlesBufferManager });
	SPHSystems.AddBack(SimulationData{ SPHSystemCPU, CPUDynamicParticlesBufferManager, CPUStaticParticlesBufferManager });	
} 
ProfilingScene::~ProfilingScene()
{
}
void ProfilingScene::Update()
{
	UISystem.GetScreen()->Update();

	if (profiling && !SPHSystems.Empty() && !profiles.Empty())
	{
		SPHSystems[systemIndex].system.Update(profiles[profileIndex].simulationStepTime, profiles[profileIndex].stepsPerUpdate);

		++currentUpdate;

		static_cast<ProfilingUI*>(UISystem.GetScreen())->SetProfilingPercent((float)currentUpdate / (profiles[profileIndex].simulationDuration / profiles[profileIndex].simulationStepTime / profiles[profileIndex].stepsPerUpdate));

		UpdateProfilingState();
	}
}
void ProfilingScene::Render(const Graphics::RenderContext& renderContext)
{
	window.ClearRenderBuffers();
	UISystem.Render();
	window.Present();
}
void ProfilingScene::OnEvent(const Input::GenericInputEvent& event)
{
}
void ProfilingScene::LoadProfiles()
{
	//File jsonFile{ "assets/simulationProfiles/systemProfilingProfiles.json", FileAccessPermission::Read };
	//std::string jsonFileString;
	//jsonFileString.resize(jsonFile.GetSize());
	//jsonFile.Read(jsonFileString.data(), jsonFileString.size());
	//
	//try
	//{
	//	JSON fileJSON = nlohmann::json::parse(jsonFileString);
	//
	//	auto& profilesJSON = fileJSON["profiles"];
	//	profiles.ReserveExactly(profilesJSON.size());
	//	for (auto& profileJSON : profilesJSON)
	//	{
	//		auto& profile = *profiles.AddBack();
	//
	//		profile.name = ConvertString(profileJSON["profileName"]);
	//		profile.outputFilePath = (Path)ConvertString(profileJSON["outputFilePath"]);
	//		profile.simulationDuration = profileJSON["simulationDuration"];
	//		profile.simulationStepTime = profileJSON["simulationStepTime"];
	//		profile.stepsPerUpdate = profileJSON["stepsPerUpdate"];
	//
	//		auto sp = (std::string)profileJSON["systemParameters"];
	//		profile.systemInitParameters.ParseJSON(StringView(sp.data(), sp.size()));
	//	}
	//}
	//catch (const std::exception& exc)
	//{
	//	Debug::Logger::LogWarning("Client", "Failed to parse profiling parameters file with message: \n" + StringView(exc.what(), strlen(exc.what())));
	//	profiles.Clear();
	//}
}
void ProfilingScene::StartProfiling()
{
	LoadProfiles();

	if (profiles.Empty())
	{
		Debug::Logger::LogWarning("Client", "No profiles loaded");
		static_cast<ProfilingUI*>(UISystem.GetScreen())->ProfilingStopped();
		return;
	}

	for (auto& profile : profiles)
		FileSystem::DeleteFile(profile.outputFilePath);

	profiling = true;
	currentUpdate = 0;
	systemIndex = 0;
	profileIndex = 0;

	if (!profiles.Empty())
	{
		NewProfileStarted();
		if (!SPHSystems.Empty())
			NewSystemStarted();
	}

}
void ProfilingScene::UpdateProfilingState()
{
	if (!profiling)
		return;

	if (profiles.Empty() || SPHSystems.Empty())
	{
		StopProfiling();
		return;
	}

	if (currentUpdate >= profiles[profileIndex].simulationDuration / profiles[profileIndex].simulationStepTime / profiles[profileIndex].stepsPerUpdate)
	{
		SystemFinished();

		static_cast<ProfilingUI*>(UISystem.GetScreen())->SetProfilingPercent(0);

		currentUpdate = 0;

		if (systemIndex + 1 >= SPHSystems.Count())
		{
			ProfileFinished();

			if (profileIndex + 1 >= profiles.Count())
				StopProfiling();
			else
			{
				SPHSystems[systemIndex].system.Clear();
				SPHSystems[systemIndex].dynamicParticleBufferSet.Clear();
				SPHSystems[systemIndex].staticParticleBufferSet.Clear();

				systemIndex = 0;

				++profileIndex;
				NewProfileStarted();
				NewSystemStarted();
			}
		}
		else
		{
			SPHSystems[systemIndex].system.Clear();
			SPHSystems[systemIndex].dynamicParticleBufferSet.Clear();
			SPHSystems[systemIndex].staticParticleBufferSet.Clear();

			++systemIndex;
			NewSystemStarted();
		}
	}
}
void ProfilingScene::StopProfiling()
{
	for (auto& system : SPHSystems)
	{
		system.system.Clear();
		system.dynamicParticleBufferSet.Clear();
		system.staticParticleBufferSet.Clear();
	}

	profiling = false;
	static_cast<ProfilingUI*>(UISystem.GetScreen())->ProfilingStopped();
}
void ProfilingScene::NewProfileStarted()
{
	static_cast<ProfilingUI*>(UISystem.GetScreen())->LogProfiling("Started profiling profile named \"" + profiles[profileIndex].name + "\".\n");
}
void ProfilingScene::NewSystemStarted()
{
	//SPHSystems[systemIndex].system.Initialize(profiles[profileIndex].systemInitParameters, SPHSystems[systemIndex].particleBufferSet, );
	//uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\". "
	//	"With " + StringParsing::Convert(SPHSystems[systemIndex].system.GetDynamicParticleCount()) + " dynamic and "
	//	+ StringParsing::Convert(SPHSystems[systemIndex].system.GetStaticParticleCount()) + " static particles\n");
}
void ProfilingScene::ProfileFinished()
{
	auto& file = profiles[profileIndex].outputFile;
	file.Open(profiles[profileIndex].outputFilePath, FileAccessPermission::Write);

	static_cast<ProfilingUI*>(UISystem.GetScreen())->LogProfiling("Finished profiling profile named \"" + profiles[profileIndex].name + "\"\n");

	String output;

	if (SPHSystems.Empty())
		return;

	output +=
		"Started profiling profile named \"" + profiles[profileIndex].name + "\".\n"
		"   Number of dynamic particles: " + String::Parse(SPHSystems[systemIndex].dynamicParticleBufferSet.GetParticleCount()) + "\n"
		"   Number of static particles:  " + String::Parse(SPHSystems[systemIndex].staticParticleBufferSet.GetParticleCount()) + "\n"		"   Simulation duration:         " + String::Parse(profiles[profileIndex].simulationDuration) + "s\n"
		"   Simulation step time:        " + String::Parse(profiles[profileIndex].simulationStepTime) + "s\n"
		"   Steps per update:            " + String::Parse(profiles[profileIndex].stepsPerUpdate) + "\n"
		"\n";

	for (auto& system : SPHSystems)
		output += Format("{14} ",  system.system.SystemImplementationName());

	output += "\n";

	//for (uintMem i = 0; i < SPHSystems[systemIndex].profilingData.Count(); ++i)
	//{
	//	for (auto& system : SPHSystems)
	//	{
	//		auto& profilingData = system.profilingData[i];
	//
	//		char buffer[256];
	//		sprintf_s(buffer, "%12.3fms \0", profilingData.timePerStep_s * 1000);
	//		output += StringView(buffer, strlen(buffer));
	//	}
	//
	//	output += "\n";
	//}
	//
	//output += "\n";
	//for (auto& system : SPHSystems)
	//{
	//	if (!system.profilingData.Empty() && system.profilingData.First().implementationSpecific.Empty())
	//		continue;
	//
	//	output += system.system.SystemImplementationName() + " specific measurements\n";
	//
	//	for (auto it = system.profilingData.First().implementationSpecific.FirstIterator(); it != system.profilingData.First().implementationSpecific.BehindIterator(); ++it)
	//		output += String(*it.GetKey()).Resize(29) + " ";
	//
	//	output += "\n";
	//
	//	for (auto& profilingData : system.profilingData)
	//	{
	//		for (auto it = profilingData.implementationSpecific.FirstIterator(); it != profilingData.implementationSpecific.BehindIterator(); ++it)
	//			if (const float* value = it.GetValue<float>())
	//			{
	//				char buffer[256];
	//				sprintf_s(buffer, "%27.3fms \0", *value * 1000);
	//				output += StringView(buffer, strlen(buffer));
	//			}
	//
	//		output += "\n";
	//	}
	//
	//}

	file.Write(output.Ptr(), output.Count());

	profiles[profileIndex].outputFile.Close();

	//for (auto& SPHSystem : SPHSystems)
	//	SPHSystem.profilingData.Clear();
}
void ProfilingScene::SystemFinished()
{
}
