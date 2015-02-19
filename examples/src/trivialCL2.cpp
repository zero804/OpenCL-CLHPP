#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <CL/cl2.hpp>
#include <iostream>
#include <vector>

const int numElements = 32;

int main(void)
{
    // Filter for a 2.0 platform and set it as the default
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cl::Platform plat;
    for (auto &p : platforms) {
        std::string platver = p.getInfo<CL_PLATFORM_VERSION>();
        if (platver.find("OpenCL 2.") != std::string::npos) {
            plat = p;
        }
    }
    if (plat() == 0)  {
        std::cout << "No OpenCL 2.0 platform found.";
        return -1;
    }

    cl::Platform newP = cl::Platform::setDefault(plat);
    if (newP != plat) {
        std::cout << "Error setting default platform.";
        return -1;
    }
    cl::Program vectorAddProgram(
        std::string(
        "kernel void vectorAdd(global const int *inputA, global const int *inputB, global int *output, int val, write_only pipe int outPipe){"
        "  output[get_global_id(0)] = inputA[get_global_id(0)] + inputB[get_global_id(0)] + val;"
        "  write_pipe(outPipe, &val);"
        "  queue_t default_queue = get_default_queue(); "
        "  ndrange_t ndrange = ndrange_1D(get_global_size(0), get_global_size(0)); "
        "  enqueue_kernel(default_queue, CLK_ENQUEUE_FLAGS_WAIT_KERNEL, ndrange, "
        "    ^{"
        "      output[get_global_size(0)+get_global_id(0)] = inputA[get_global_size(0)+get_global_id(0)] + inputB[get_global_size(0)+get_global_id(0)];"
        "    });"
        "}")       
        , false);
    try {
        vectorAddProgram.build("-cl-std=CL2.0");
    }
    catch (...) {
        std::string bl = vectorAddProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl::Device::getDefault());
        std::cerr << bl << std::endl;
    }

	auto vectorAddKernel =
		cl::KernelFunctor<
		cl::Buffer&,
		cl::Buffer&,
		cl::Buffer&,
        int,
        cl::Pipe&
		>(vectorAddProgram, "vectorAdd");

    std::vector<int> inputA(numElements, 1);
    std::vector<int> inputB(numElements, 2);
    std::vector<int> output(numElements, 0xdeadbeef);
    cl::Buffer inputABuffer(begin(inputA), end(inputA), true);
    cl::Buffer inputBBuffer(begin(inputB), end(inputB), true);
    cl::Buffer outputBuffer(begin(output), end(output), false);
    cl::Pipe aPipe(sizeof(cl_int), numElements / 2);
    // Unfortunately, there is no way to check for a default or know if a kernel needs one
    // so the user has to create one
    // We can't preemptively do so on device creation because they cannot then replace it
    cl::DeviceCommandQueue deviceQueue = cl::DeviceCommandQueue::makeDefault(
        cl::Context::getDefault(), cl::Device::getDefault());
	
    vectorAddKernel(
        cl::EnqueueArgs(
            cl::NDRange(numElements/2),
            cl::NDRange(numElements/2)),
        inputABuffer,
        inputBBuffer,
        outputBuffer,
        3,
        aPipe);

	cl_int error;
	vectorAddKernel(
		cl::EnqueueArgs(
		cl::NDRange(numElements/2),
		cl::NDRange(numElements/2)),
		inputABuffer,
		inputBBuffer,
		outputBuffer,
        3,
        aPipe,
		error);

    cl::copy(outputBuffer, begin(output), end(output));

    cl::Device d = cl::Device::getDefault();
    std::cout << "Max pipe args: " << d.getInfo<CL_DEVICE_MAX_PIPE_ARGS>() << "\n";
    std::cout << "Max pipe active reservations: " << d.getInfo<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS>() << "\n";
    std::cout << "Max pipe packet size: " << d.getInfo<CL_DEVICE_PIPE_MAX_PACKET_SIZE>() << "\n";

    std::cout << "Output:\n";
    for (int i = 1; i < numElements; ++i) {
        std::cout << "\t" << output[i] << "\n";
    }
    std::cout << "\n\n";

}
