// SPDX-License-Identifier: Apache-2.0
#include <cstdio>

void TestBasis();
void TestIdentifiers();
void TestTexturePaths();
void TestSkeleton();
void TestCanonicalize();

int
main()
{
    TestBasis();
    TestIdentifiers();
    TestTexturePaths();
    TestSkeleton();
    TestCanonicalize();
    std::puts("mmdModel unit tests passed");
    return 0;
}
