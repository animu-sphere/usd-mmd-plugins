// SPDX-License-Identifier: Apache-2.0
#include <cstdio>

void RunSampleTests();
void RunEvaluateTests();

int
main()
{
    RunSampleTests();
    RunEvaluateTests();
    std::puts("mmdControl_unit: all passed");
    return 0;
}
