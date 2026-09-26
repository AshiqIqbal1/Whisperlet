#include "cpufeatures.h"

#include <cstdio>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::string joined(const std::vector<const char *> &names)
{
    std::string out;
    for (const char *name : names) {
        if (!out.empty())
            out += ",";
        out += name;
    }
    return out;
}

void expectMissing(const CpuIdInfo &info, const char *expected, const char *what)
{
    const std::string got = joined(missingCpuFeatures(info));
    if (got != expected) {
        std::fprintf(stderr, "FAIL: %s: expected missing \"%s\", got \"%s\"\n", what,
                     expected, got.c_str());
        ++failures;
    }
}

// Values as reported by a Haswell Core i7-4770 under Windows 10.
CpuIdInfo haswell()
{
    CpuIdInfo info;
    info.maxLeaf = 0xd;
    info.leaf1Ecx = 0x7ffafbff;
    info.leaf7Ebx = 0x000027ab;
    info.xcr0 = 0x7;
    return info;
}

// Values as reported by a Gemini Lake Celeron N4020 (2019): SSE4.2 but no
// AVX, FMA, F16C, AVX2 or BMI2. The class of CPU #69 crashed on.
CpuIdInfo celeronN4020()
{
    CpuIdInfo info;
    info.maxLeaf = 0x18;
    info.leaf1Ecx = 0x4ff8ebbf;
    info.leaf7Ebx = 0x2294e283;
    info.xcr0 = 0x3;
    return info;
}

void supportedCpuPasses()
{
    check(missingCpuFeatures(haswell()).empty(), "Haswell has every required feature");
}

void budgetCpuIsRefused()
{
    expectMissing(celeronN4020(), "AVX,AVX2,FMA,F16C,BMI2", "Celeron N4020");
}

// The CPU has AVX but the OS never enabled saving the YMM registers, so AVX
// instructions would still fault.
void osWithoutAvxStateIsRefused()
{
    CpuIdInfo noXsave = haswell();
    noXsave.leaf1Ecx &= ~(1u << 27);
    noXsave.xcr0 = 0;
    expectMissing(noXsave, "AVX,AVX2,FMA,F16C", "OSXSAVE off");

    CpuIdInfo noYmm = haswell();
    noYmm.xcr0 = 0x3;
    expectMissing(noYmm, "AVX,AVX2,FMA,F16C", "XCR0 without AVX state");
}

// Leaf 7 is only meaningful when CPUID reports it exists.
void leaf7IgnoredWhenAbsent()
{
    CpuIdInfo info = haswell();
    info.maxLeaf = 6;
    expectMissing(info, "AVX2,BMI2", "max leaf below 7");
}

// Sandy/Ivy Bridge: AVX (and F16C on Ivy) but no AVX2/FMA/BMI2.
void avxOnlyCpuIsRefused()
{
    CpuIdInfo ivyBridge = haswell();
    ivyBridge.leaf1Ecx &= ~(1u << 12); // no FMA
    ivyBridge.leaf7Ebx = 0x00000281;   // no AVX2, no BMI2
    expectMissing(ivyBridge, "AVX2,FMA,BMI2", "Ivy Bridge");
}

} // namespace

int main()
{
    supportedCpuPasses();
    budgetCpuIsRefused();
    osWithoutAvxStateIsRefused();
    leaf7IgnoredWhenAbsent();
    avxOnlyCpuIsRefused();

    if (failures == 0)
        std::printf("cpufeatures_test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
