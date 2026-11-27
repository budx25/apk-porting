// pc.readTextFile may only read files inside the game folder.
#include "SafePath.h"
#include <cassert>
#include <cstdio>

int main()
{
    assert(isSafeRelativePath("VERSION"));
    assert(isSafeRelativePath("UI/cfgAssets.json"));
    assert(isSafeRelativePath("UI\\cfgAssets.json"));

    assert(!isSafeRelativePath(""));
    assert(!isSafeRelativePath("C:\\Users\\me\\secret.txt"));
    assert(!isSafeRelativePath("C:/Users/me/secret.txt"));
    assert(!isSafeRelativePath("\\\\server\\share\\x"));
    assert(!isSafeRelativePath("/etc/passwd"));
    assert(!isSafeRelativePath("..\\..\\secret.txt"));
    assert(!isSafeRelativePath("UI/../../secret.txt"));
    assert(!isSafeRelativePath("UI/.."));

    std::puts("safe_path_tests: all passed");
    return 0;
}
