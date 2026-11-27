#ifndef __SAFE_PATH_H__
#define __SAFE_PATH_H__

#include <string>

// True for a non-empty relative path that stays inside the folder it's resolved against:
// no drive letter or ':', no leading slash or backslash, no ".." component.
inline bool isSafeRelativePath(const std::string& path)
{
    if (path.empty() || path.find(':') != std::string::npos || path[0] == '/' || path[0] == '\\')
        return false;
    size_t start = 0;
    while (start <= path.size())
    {
        size_t end = path.find_first_of("/\\", start);
        if (end == std::string::npos) end = path.size();
        if (path.compare(start, end - start, "..") == 0 && end - start == 2)
            return false;
        start = end + 1;
    }
    return true;
}

#endif
