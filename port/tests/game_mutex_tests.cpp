// The game's mutex name must be the one the launcher looks for (launcher/src/launch.rs).
#include "GameMutex.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    std::string here = __FILE__;
    std::string launch = here.substr(0, here.find_last_of("\\/")) + "/../../launcher/src/launch.rs";
    std::ifstream in(launch);
    if (!in) { std::printf("not found: %s\n", launch.c_str()); return 1; }
    std::stringstream text;
    text << in.rdbuf();
    std::string src = text.str();
    std::string key = "GAME_MUTEX: &str = r\"";
    size_t at = src.find(key);
    assert(at != std::string::npos);
    std::string expected = src.substr(at + key.size(), src.find('"', at + key.size()) - at - key.size());

    std::wstring game(kGameMutex);
    std::string narrow(game.begin(), game.end());
    std::printf("launcher: %s\ngame:     %s\n", expected.c_str(), narrow.c_str());
    assert(narrow == expected);
    std::puts("game_mutex_tests: all passed");
    return 0;
}
