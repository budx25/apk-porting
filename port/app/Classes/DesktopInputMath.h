#ifndef __DESKTOP_INPUT_MATH_H__
#define __DESKTOP_INPUT_MATH_H__

// Touch paths that turn desktop input (wheel, arrow keys, Ctrl+wheel) into the drags and
// pinches the game understands. Engine-free; cocos coordinates (y points up).
#include <utility>
#include <vector>

struct Pt { float x, y; };
enum class Axis { Vertical, Horizontal };

// Start point plus `steps` points. Wheel down (delta < 0) drags up on vertical lists and left on
// horizontal ones, so content moves the way a desktop user expects. 120 delta = one notch.
std::vector<Pt> wheelDragPath(Pt start, int wheelDelta, Axis axis, float pxPerNotch, int steps);

// A page swipe through the centre: 40% of the width. right == true (next page) drags left.
std::vector<Pt> arrowSwipePath(Pt center, bool right, float width, int steps);

// Two fingers starting 100 px apart around `center`; delta > 0 moves them apart (zoom in).
std::pair<std::vector<Pt>, std::vector<Pt>> pinchPaths(Pt center, int wheelDelta, float pxPerNotch, int steps);

#endif
