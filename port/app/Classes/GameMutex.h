#ifndef __GAME_MUTEX_H__
#define __GAME_MUTEX_H__

// Held while the game runs, so the launcher can tell (must match GAME_MUTEX in launcher/src/launch.rs).
static const wchar_t kGameMutex[] = L"Local\\NinjaHeroesPC.Game";

#endif
