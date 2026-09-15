// SPDX-License-Identifier: Apache-2.0
#include <cstdio>

void TestDiagnostic();
void TestText();
void TestReader();

int
main()
{
    TestDiagnostic();
    TestText();
    TestReader();
    std::puts("mmdPmx unit tests passed");
    return 0;
}
