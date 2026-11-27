#ifndef __PC_BINDINGS_H__
#define __PC_BINDINGS_H__

#include "jsapi.h"

// Script-visible `pc` object: what the Windows host offers the game's scripts.
//   pc.openURL(url)        -> bool   opens http/https links in the default browser
//   pc.readTextFile(path)  -> string|null   reads a file through the engine's search paths
//   pc.debug               -> bool   true when started with --debug
void register_pc_bindings(JSContext* cx, JSObject* global);
void pc_set_debug(bool on);

#endif
