// SPDX-License-Identifier: Apache-2.0
#include <cstdio>

void TestDiagnostic();
void TestReader();

int
main()
{
    TestDiagnostic();
    TestReader();
    std::puts("mmdPmx unit tests passed");
    return 0;
}
