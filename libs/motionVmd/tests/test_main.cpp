// SPDX-License-Identifier: Apache-2.0
#include <cstdio>

void TestCp932();
void TestReader();
void TestMotion();

int
main()
{
    TestCp932();
    TestReader();
    TestMotion();
    std::puts("motionVmd unit tests passed");
    return 0;
}
