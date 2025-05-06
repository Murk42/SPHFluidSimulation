#pragma once
#include "OpenCLDebug.h"

namespace Blaze
{
	template<auto _Name, typename _Type>
	struct KernelArgument
	{
		static constexpr Constexpr::FixedString Name = _Name;
		using Type = _Type;
	};

	template<Constexpr::FixedString _KernelName, typename ... _Args>
	struct Kernel
	{
	public:
		using Args = TemplateGroup<_Args...>;
		static constexpr Constexpr::FixedString KernelName = _KernelName;

		Kernel() : kernel(NULL) {}
		~Kernel()
		{
			if (kernel != NULL)
				CL_CALL(clReleaseKernel(kernel));
		}

		void Create(cl_program program)
		{
			CL_CALL(kernel = clCreateKernel(program, KernelName, &ret));

			uint argCount;
			CL_CALL(clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(uint), &argCount, nullptr));

			if (argCount != sizeof...(_Args))
			{
				Debug::Logger::LogFatal("SPH Library", "Kernel argument count doesn't match!");
				CL_CALL(clReleaseKernel(kernel));
				return;
			}

			uintMem index;
			bool validArguments = (CheckKernelArg<typename _Args::template Type, _Args::Name>(index++) && ...);

			if (!validArguments)
			{
				CL_CALL(clReleaseKernel(kernel));
				return;
			}
		}
		void Enqueue(cl_command_queue commandQueue, size_t globalWorkOffset, size_t globalWorkSize, size_t localWorkSize, ArrayView<cl_event> waitEvents, cl_event* signalEvent, const typename _Args::template Type& ... args)
		{
			uintMem argIndex = 0;
			(SetKernelArg<typename _Args::template Type>(argIndex++, args), ...);
			CL_CALL(clEnqueueNDRangeKernel(commandQueue, kernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, waitEvents.Count(), waitEvents.Ptr(), signalEvent));
		}
	private:
		cl_kernel kernel;

		template<typename T>
		static StringView GetOpenCLTypeName();
		template<> static StringView GetOpenCLTypeName<uint32>() { return "uint"; }
		template<> static StringView GetOpenCLTypeName<int32>() { return "int"; }
		template<> static StringView GetOpenCLTypeName<uint64>() { return "ulong"; }
		template<> static StringView GetOpenCLTypeName<int64>() { return "long"; }
		template<> static StringView GetOpenCLTypeName<float>() { return "float"; }
		template<> static StringView GetOpenCLTypeName<double>() { return "double"; }

		template<typename T>
		inline void SetKernelArg(uintMem index, const T& value)
		{
			CL_CALL(clSetKernelArg(kernel, index, sizeof(T), &value));
		}
		template<typename T, Constexpr::FixedString Name>
		inline bool CheckKernelArg(uintMem i)
		{
			uintMem paramRetSize;

			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_NAME, 0, nullptr, &paramRetSize));
			String argumentName{ paramRetSize - 1 };
			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_NAME, paramRetSize, argumentName.Ptr(), nullptr));

			if (argumentName != Name)
			{
				Debug::Logger::LogFatal("SPH Library", "Kernel " + KernelName + " argument name doesn't match! In the kernel it's \"" + argumentName + "\" but the template value is \"" + Name + "\"");
				return false;
			}

			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, 0, nullptr, &paramRetSize));
			String argumentType{ paramRetSize - 1 };
			CL_CALL(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, paramRetSize, argumentType.Ptr(), nullptr));

			if constexpr (std::same_as<T, cl_mem>)
			{
				bool found = false;

				for (auto& ch : argumentType)
					if (ch == '*')
					{
						found = true;
						break;
					}

				if (!found)
				{
					Debug::Logger::LogFatal("SPH Library", "Kernel " + KernelName + " argument type doesn't match! In the kernel it's not a pointer type (\"" + argumentType + "\") but the template type is \"cl_mem\"");
					return false;
				}
			}
			else
			{
				if (GetOpenCLTypeName<T>() != argumentType)
				{
					Debug::Logger::LogFatal("SPH Library", "Kernel " + KernelName + " argument type doesn't match! In the kernel it's \"" + argumentType + "\" but the template type is \"" + typeid(T).name() + "\"");
					return false;
				}
			}

			return true;
		}
	};
}