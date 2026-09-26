#include "cpufeatures.h"

#include <immintrin.h> // _xgetbv
#include <intrin.h>    // __cpuid, __cpuidex

// This file is compiled without any /arch flag, so none of it can use the
// instructions it is checking for.
CpuIdInfo readCpuIdInfo()
{
    CpuIdInfo info;
    int regs[4] = {};

    __cpuid(regs, 0);
    info.maxLeaf = static_cast<std::uint32_t>(regs[0]);

    __cpuid(regs, 1);
    info.leaf1Ecx = static_cast<std::uint32_t>(regs[2]);

    if (info.maxLeaf >= 7) {
        __cpuidex(regs, 7, 0);
        info.leaf7Ebx = static_cast<std::uint32_t>(regs[1]);
    }

    // XGETBV itself is an illegal instruction unless the OS enabled XSAVE.
    if (info.leaf1Ecx & (1u << 27))
        info.xcr0 = _xgetbv(0);

    return info;
}
