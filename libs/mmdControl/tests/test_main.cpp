// SPDX-License-Identifier: Apache-2.0
#include <cstdio>

void RunSampleTests();
void RunEvaluateTests();
void RunUvMorphTests();
void RunMaterialMorphTests();

int
main()
{
    RunSampleTests();
    RunEvaluateTests();
    RunUvMorphTests();
    RunMaterialMorphTests();
    std::puts("mmdControl_unit: all passed");
    return 0;
}
