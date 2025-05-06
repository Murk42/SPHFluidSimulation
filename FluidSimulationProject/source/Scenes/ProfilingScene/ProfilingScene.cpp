#include "pch.h"
#include "Scenes/ProfilingScene/ProfilingScene.h"
#include "JSONParsing.h"

#include "SPH/System/SystemCPU.h"
#include "SPH/System/SystemGPU.h"

ProfilingScene::ProfilingScene(OpenCLContext& clContext, cl_command_queue clQueue, RenderingSystem& renderingSystem) :
	clContext(clContext), renderingSystem(renderingSystem), window(renderingSystem.GetWindow()),
	SPHSystemGPU(clContext.context, clContext.device, clQueue, renderingSystem.GetGraphicsContext()), SPHSystemCPU(std::thread::hardware_concurrency()),
	GPUDynamicParticlesBufferManager(clContext.context, clContext.device, clQueue),
	GPUStaticParticlesBufferManager(clContext.context, clContext.device, clQueue),
	uiScreen(*this, &window)
{
	//Setup UI	
	uiScreen.SetWindow(&window);
	renderingSystem.SetScreen(&uiScreen);
	UIInputManager.SetScreen(&uiScreen);	


	SPHSystems.AddBack(SPHSystemData{ SPHSystemGPU, GPUDynamicParticlesBufferManager, GPUStaticParticlesBufferManager });
	SPHSystems.AddBack(SPHSystemData{ SPHSystemCPU, CPUDynamicParticlesBufferManager, CPUStaticParticlesBufferManager });
}
ProfilingScene::~ProfilingScene()
{
	renderingSystem.SetScreen(nullptr);	

}
void ProfilingScene::Update()
{	
	renderingSystem.Render({});

	if (profiling && !SPHSystems.Empty() && !profiles.Empty())
	{		
		SPHSystems[systemIndex].system.Update(profiles[profileIndex].simulationStepTime, profiles[profileIndex].stepsPerUpdate);
		
		++currentUpdate;		
					
		uiScreen.SetProfilingPercent((float)currentUpdate / (profiles[profileIndex].simulationDuration / profiles[profileIndex].simulationStepTime / profiles[profileIndex].stepsPerUpdate));		

		UpdateProfilingState();
	}
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

		uiScreen.SetProfilingPercent(0);

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
	uiScreen.ProfilingStopped();
}
void ProfilingScene::NewProfileStarted()
{		
	uiScreen.LogProfiling("Started profiling profile named \"" + profiles[profileIndex].name + "\".\n");
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

	uiScreen.LogProfiling("Finished profiling profile named \"" + profiles[profileIndex].name + "\"\n");	

	String output;

	if (SPHSystems.Empty())
		return;

	output +=
		"Started profiling profile named \"" + profiles[profileIndex].name + "\".\n"
		"   Number of dynamic particles: " + StringParsing::Convert(SPHSystems[systemIndex].dynamicParticleBufferSet.GetBufferSize() / sizeof(SPH::DynamicParticle)) + "\n"
		"   Number of static particles:  " + StringParsing::Convert(SPHSystems[systemIndex].staticParticleBufferSet.GetBufferSize() / sizeof(SPH::StaticParticle)) + "\n"		"   Simulation duration:         " + StringParsing::Convert(profiles[profileIndex].simulationDuration) + "s\n"
		"   Simulation step time:        " + StringParsing::Convert(profiles[profileIndex].simulationStepTime) + "s\n"
		"   Steps per update:            " + StringParsing::Convert(profiles[profileIndex].stepsPerUpdate) + "\n"		
		"\n";

	for (auto& system : SPHSystems)
		output += String(system.system.SystemImplementationName()).Resize(14) + " ";

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
