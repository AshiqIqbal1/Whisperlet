#include "cpufeatures.h"

namespace {

// CPUID leaf 1, ECX
constexpr std::uint32_t kFma = 1u << 12;
constexpr std::uint32_t kSse42 = 1u << 20;
constexpr std::uint32_t kOsXsave = 1u << 27;
constexpr std::uint32_t kAvx = 1u << 28;
constexpr std::uint32_t kF16c = 1u << 29;

// CPUID leaf 7, EBX
constexpr std::uint32_t kAvx2 = 1u << 5;
constexpr std::uint32_t kBmi2 = 1u << 8;

// XCR0: the OS saves SSE (bit 1) and AVX (bit 2) register state across
// context switches. Without it AVX instructions fault even on a CPU that
// has them.
constexpr std::uint64_t kXcr0SseAvx = 0x6;

} // namespace

std::vector<const char *> missingCpuFeatures(const CpuIdInfo &info)
{
    const bool osSavesAvx =
        (info.leaf1Ecx & kOsXsave) && (info.xcr0 & kXcr0SseAvx) == kXcr0SseAvx;
    const std::uint32_t leaf7Ebx = info.maxLeaf >= 7 ? info.leaf7Ebx : 0;

    std::vector<const char *> missing;
    if (!(info.leaf1Ecx & kSse42))
        missing.push_back("SSE4.2");
    if (!(info.leaf1Ecx & kAvx) || !osSavesAvx)
        missing.push_back("AVX");
    if (!(leaf7Ebx & kAvx2) || !osSavesAvx)
        missing.push_back("AVX2");
    if (!(info.leaf1Ecx & kFma) || !osSavesAvx)
        missing.push_back("FMA");
    if (!(info.leaf1Ecx & kF16c) || !osSavesAvx)
        missing.push_back("F16C");
    if (!(leaf7Ebx & kBmi2))
        missing.push_back("BMI2");
    return missing;
}
