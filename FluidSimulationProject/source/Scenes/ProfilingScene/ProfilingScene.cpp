#include "pch.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"
#include "JSONParsing.h"

#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

ProfilingScene::ProfilingScene(OpenCLContext& clContext, cl::CommandQueue& clQueue, ThreadPool& threadPool, RenderingSystem& renderingSystem) :
	clContext(clContext), renderingSystem(renderingSystem), threadPool(threadPool), window(renderingSystem.GetWindow()),
	SPHSystemGPU(clContext, clQueue, renderingSystem.GetGraphicsContext()), SPHSystemCPU(threadPool),
	GPUParticleBufferSet(clContext, clQueue)
{		
	threadPool.AllocateThreads(std::thread::hardware_concurrency());

	//Setup UI	
	uiScreen.SetWindow(&window);
	renderingSystem.SetScreen(&uiScreen);
	UIInputManager.SetScreen(&uiScreen);	


	SPHSystems.AddBack(SPHSystemData{ SPHSystemGPU, GPUParticleBufferSet });
	SPHSystems.AddBack(SPHSystemData{ SPHSystemCPU, CPUParticleBufferSet });

	for (auto& systemData : SPHSystems)
		systemData.system.EnableProfiling(true);

	SetupEvents();
}

ProfilingScene::~ProfilingScene()
{
	renderingSystem.SetScreen(nullptr);	

}

void ProfilingScene::Update()
{	
	renderingSystem.Render();	

	if (profiling && !SPHSystems.Empty() && !profiles.Empty())
	{		
		SPHSystems[systemIndex].system.Update(profiles[profileIndex].simulationStepTime, profiles[profileIndex].stepsPerUpdate);
		
		auto profilingData = SPHSystems[systemIndex].system.GetProfilingData();
		++currentUpdate;		
		
		WriteToOutputFile(StringParsing::Convert(profilingData.timePerStep_s) + "s\n");

		uiScreen.SetProfilingPercent((float)currentUpdate / (profiles[profileIndex].simulationDuration / profiles[profileIndex].simulationStepTime / profiles[profileIndex].stepsPerUpdate));		

		UpdateProfilingState();
	}
}

void ProfilingScene::LoadProfiles()
{
	File jsonFile{ "assets/simulationProfiles/systemProfilingProfiles.json", FileAccessPermission::Read };
	std::string jsonFileString;
	jsonFileString.resize(jsonFile.GetSize());
	jsonFile.Read(jsonFileString.data(), jsonFileString.size());

	try 
	{
		JSON fileJSON = nlohmann::json::parse(jsonFileString);	

		auto& profilesJSON = fileJSON["profiles"];
		profiles.ReserveExactly(profilesJSON.size());
		for (auto& profileJSON : profilesJSON)
		{
			auto& profile = *profiles.AddBack();

			profile.name = ConvertString(profileJSON["profileName"]);
			profile.outputFilePath = (Path)ConvertString(profileJSON["outputFilePath"]);
			profile.simulationDuration = profileJSON["simulationDuration"];
			profile.simulationStepTime = profileJSON["simulationStepTime"];
			profile.stepsPerUpdate = profileJSON["stepsPerUpdate"];

			profile.systemInitParameters.ParseJSON(profileJSON["systemParameters"]);
		}
	}
	catch (const std::exception& exc)
	{
		Debug::Logger::LogWarning("Client", "Failed to parse profiling parameters file with message: \n" + StringView(exc.what(), strlen(exc.what())));
		profiles.Clear();
	}
}

void ProfilingScene::SetupEvents()
{	
	startProfilingButtonPressedEventHandler.SetFunction([&](auto event) {
		StartProfiling();				
		});
	uiScreen.starProfilingButton.pressedEventDispatcher.AddHandler(startProfilingButtonPressedEventHandler);
}

void ProfilingScene::WriteToOutputFile(StringView s)
{
	if (outputFile.IsOpen())
	{		
		outputFile.Write(s.Ptr(), s.Count());
	}
}

void ProfilingScene::StartProfiling()
{
	LoadProfiles();

	if (profiles.Empty())
	{
		Debug::Logger::LogWarning("Client", "No profiles loaded");
		uiScreen.ProfilingStopped();
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
		outputFile.Open(profiles[profileIndex].outputFilePath, FileAccessPermission::Write, { .openOption = FileOpenOptions::OpenAlways });


		if (!SPHSystems.Empty())		
			SPHSystems[systemIndex].system.Initialize(profiles[profileIndex].systemInitParameters, SPHSystems[systemIndex].particleBufferSet);

		String outputString = 
			"Started profiling profile named \"" + profiles[profileIndex].name + "\".\n"
			"   Number of dynamic particles: " + StringParsing::Convert(SPHSystems[systemIndex].system.GetDynamicParticleCount()) + "\n" +
			"   Number of static particles: " + StringParsing::Convert(SPHSystems[systemIndex].system.GetStaticParticleCount()) + "\n";
		uiScreen.LogProfiling(outputString);
		WriteToOutputFile(outputString);

		if (!SPHSystems.Empty())
		{
			uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\"\n");
			WriteToOutputFile("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\"\n");
		}
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
		currentUpdate = 0;

		SPHSystems[systemIndex].system.Clear();

		++systemIndex;

		if (systemIndex >= SPHSystems.Count())
		{
			uiScreen.LogProfiling("Finished profiling profile named \"" + profiles[profileIndex].name + "\"\n");
			WriteToOutputFile("Finished profiling profile named \"" + profiles[profileIndex].name + "\"\n");

			systemIndex = 0;
			++profileIndex;

			if (profileIndex >= profiles.Count())			
				StopProfiling();
			else
			{
				outputFile.Open(profiles[profileIndex].outputFilePath, FileAccessPermission::Write, { .openOption = FileOpenOptions::OpenAlways });
				SPHSystems[systemIndex].system.Initialize(profiles[profileIndex].systemInitParameters, SPHSystems[systemIndex].particleBufferSet);

				String outputString =
					"Started profiling profile named \"" + profiles[profileIndex].name + "\".\n"
					"   Number of dynamic particles: " + StringParsing::Convert(SPHSystems[systemIndex].system.GetDynamicParticleCount()) + "\n" +
					"   Number of static particles: " + StringParsing::Convert(SPHSystems[systemIndex].system.GetStaticParticleCount()) + "\n";
				uiScreen.LogProfiling(outputString);
				WriteToOutputFile(outputString);
				uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\"\n");
				WriteToOutputFile("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\"\n");
			}
		}		
		else
		{
			uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\"\n");
			WriteToOutputFile("Running system implementation \"" + SPHSystems[systemIndex].system.SystemImplementationName() + "\"\n");
			SPHSystems[systemIndex].system.Initialize(profiles[profileIndex].systemInitParameters, SPHSystems[systemIndex].particleBufferSet);
		}

	}
}

void ProfilingScene::StopProfiling()
{
	profiling = false;
	outputFile.Close();
	uiScreen.ProfilingStopped();
}
