#pragma once
#include "SPH/ParticleBufferSet/CPUParticleBufferSet.h"
#include "SPH/System/System.h"

namespace SPH
{
	class OfflineCPUParticleBufferSet :
		public CPUParticleBufferSet
	{
	public:
		OfflineCPUParticleBufferSet();

		void Initialize(uintMem dynamicParticleBufferCount, ArrayView<DynamicParticle> dynamicParticles) override;
		void Clear() override;
		void Advance() override;

		CPUParticleReadBufferHandle& GetReadBufferHandle() override;
		CPUParticleWriteBufferHandle& GetWriteBufferHandle() override;		

		uintMem GetDynamicParticleCount() override;
	private:
		class Buffer :
			public CPUParticleReadBufferHandle,
			public CPUParticleWriteBufferHandle
		{			
			Array<DynamicParticle> dynamicParticles;

			CPUSync readSync;
			CPUSync writeSync;

			std::mutex stateMutex;
			std::condition_variable stateCV;

			uintMem dynamicParticleCount;
		public:
			Buffer();

			void Initialize(const DynamicParticle* dynamicParticlePtr, uintMem dynamicParticleCount);

			CPUSync& GetReadSync() override;
			CPUSync& GetWriteSync() override;			

			const DynamicParticle* GetReadBuffer() override { return dynamicParticles.Ptr(); }
			DynamicParticle* GetWriteBuffer() override { return dynamicParticles.Ptr(); }
		};

		Array<Buffer> buffers;
		uintMem currentBuffer;

		uintMem dynamicParticleCount;
	};
}