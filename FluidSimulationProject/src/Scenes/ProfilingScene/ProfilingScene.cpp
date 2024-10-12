#include "pch.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"
#include "JSONParsing.h"

#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

ProfilingScene::ProfilingScene(OpenCLContext& CLContext, RenderingSystem& renderingSystem) :
	CLContext(CLContext), renderingSystem(renderingSystem), window(renderingSystem.GetWindow())
{		
	threadPool.AllocateThreads(std::thread::hardware_concurrency());

	//Setup UI	
	uiScreen.SetWindow(&window);
	renderingSystem.SetScreen(&uiScreen);
	UIInputManager.SetScreen(&uiScreen);	

	SPHSystems.AddBack(std::make_unique<SPH::SystemGPU>(CLContext, renderingSystem.GetGraphicsContext()));
	SPHSystems.AddBack(std::make_unique<SPH::SystemCPU>(threadPool));

	for (auto& SPHSystem : SPHSystems)
		SPHSystem->EnableProfiling(true);

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
		std::shared_ptr<SPH::System> system = SPHSystems[systemIndex];						

		system->Update(profiles[profileIndex].simulationStepTime, profiles[profileIndex].stepsPerUpdate);
		
		auto profilingData = system->GetProfilingData();
		++currentUpdate;		
		
		WriteToOutputFile(StringParsing::Convert(profilingData.timePerStep_ns) + "ns\n");

		uiScreen.SetProfilingPercent((float)currentUpdate / (profiles[profileIndex].simulationDuration / profiles[profileIndex].simulationStepTime / profiles[profileIndex].stepsPerUpdate));		

		UpdateProfilingState();
	}
}

void ProfilingScene::LoadProfiles()
{
	File jsonFile{ "simulationProfiles/systemProfilingProfiles.json", FileAccessPermission::Read };
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
			SPHSystems[systemIndex]->Initialize(profiles[profileIndex].systemInitParameters);					

		String outputString = 
			"Started profiling profile named \"" + profiles[profileIndex].name + "\".\n"
			"   Number of dynamic particles: " + StringParsing::Convert(SPHSystems[systemIndex]->GetDynamicParticleCount()) + "\n" +
			"   Number of static particles: " + StringParsing::Convert(SPHSystems[systemIndex]->GetStaticParticleCount()) + "\n";
		uiScreen.LogProfiling(outputString);
		WriteToOutputFile(outputString);

		if (!SPHSystems.Empty())
		{
			uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex]->SystemImplementationName() + "\"\n");
			WriteToOutputFile("Running system implementation \"" + SPHSystems[systemIndex]->SystemImplementationName() + "\"\n");
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

		SPHSystems[systemIndex]->Clear();

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
				SPHSystems[systemIndex]->Initialize(profiles[profileIndex].systemInitParameters);

				String outputString =
					"Started profiling profile named \"" + profiles[profileIndex].name + "\".\n"
					"   Number of dynamic particles: " + StringParsing::Convert(SPHSystems[systemIndex]->GetDynamicParticleCount()) + "\n" +
					"   Number of static particles: " + StringParsing::Convert(SPHSystems[systemIndex]->GetStaticParticleCount()) + "\n";
				uiScreen.LogProfiling(outputString);
				WriteToOutputFile(outputString);
				uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex]->SystemImplementationName() + "\"\n");
				WriteToOutputFile("Running system implementation \"" + SPHSystems[systemIndex]->SystemImplementationName() + "\"\n");
			}
		}		
		else
		{
			uiScreen.LogProfiling("Running system implementation \"" + SPHSystems[systemIndex]->SystemImplementationName() + "\"\n");
			WriteToOutputFile("Running system implementation \"" + SPHSystems[systemIndex]->SystemImplementationName() + "\"\n");
			SPHSystems[systemIndex]->Initialize(profiles[profileIndex].systemInitParameters);
		}

	}
}

void ProfilingScene::StopProfiling()
{
	profiling = false;
	outputFile.Close();
	uiScreen.ProfilingStopped();
}
