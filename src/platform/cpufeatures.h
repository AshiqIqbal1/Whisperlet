#pragma once

#include <cstdint>
#include <vector>

// The Windows build compiles ggml's CPU kernels for a fixed x86-64 baseline
// (the AVX/AVX2/FMA/F16C flags pinned in CMakeLists.txt, plus SSE4.2 and
// BMI2, which ggml-cpu enables by default when GGML_NATIVE is OFF and which
// MSVC's /arch:AVX2 may also emit). On a CPU without those
// instructions the first ggml call dies with an illegal instruction and no
// message, so main() checks for them up front and explains instead (#69).
//
// Keep this list in step with what ggml-cpu is compiled with on Windows.

// The raw CPUID/XGETBV values the check needs, split out from reading them
// so the decision is testable on any machine.
struct CpuIdInfo {
    std::uint32_t maxLeaf = 0;  // CPUID leaf 0, EAX
    std::uint32_t leaf1Ecx = 0; // CPUID leaf 1, ECX
    std::uint32_t leaf7Ebx = 0; // CPUID leaf 7 subleaf 0, EBX (0 if maxLeaf < 7)
    std::uint64_t xcr0 = 0;     // XGETBV(0) (0 if the OS has not enabled XSAVE)
};

// Names of the required features the CPU (or the OS, for AVX state saving)
// lacks, in a fixed order. Empty when the build can run.
std::vector<const char *> missingCpuFeatures(const CpuIdInfo &info);

#ifdef _WIN32
// Reads CPUID/XGETBV on the running machine.
CpuIdInfo readCpuIdInfo();
#endif
