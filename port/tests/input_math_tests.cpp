// Synthetic touch paths for mouse wheel, arrow keys and pinch (cocos coordinates: y points up).
#include "DesktopInputMath.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    // Wheel down on a vertical list drags the finger up, one notch = pxPerNotch.
    auto p = wheelDragPath({500, 300}, -120, Axis::Vertical, 120.f, 4);
    assert(p.size() == 5 && p.front().x == 500 && p.front().y == 300 && p.back().x == 500 && p.back().y == 420);

    // Shift+wheel down on a horizontal list drags left, two notches.
    auto h = wheelDragPath({500, 300}, -240, Axis::Horizontal, 120.f, 4);
    assert(h.back().x == 260 && h.back().y == 300);

    // Wheel up reverses the direction.
    auto up = wheelDragPath({500, 300}, 120, Axis::Vertical, 120.f, 4);
    assert(up.back().y == 180);

    // Right arrow = next page = drag left by 40% of the width.
    auto a = arrowSwipePath({568, 320}, true, 1136.f, 6);
    assert(a.size() == 7 && a.back().x < a.front().x && std::abs((a.front().x - a.back().x) - 454.4f) < 0.01f);
    auto l = arrowSwipePath({568, 320}, false, 1136.f, 6);
    assert(l.back().x > l.front().x);

    // Ctrl+wheel up spreads two fingers apart (zoom in); down brings them together.
    auto [f1, f2] = pinchPaths({568, 320}, 120, 60.f, 4);
    assert(f1.size() == 5 && f2.size() == 5);
    assert(std::abs(f2.back().x - f1.back().x) > std::abs(f2.front().x - f1.front().x));
    auto [g1, g2] = pinchPaths({568, 320}, -120, 60.f, 4);
    assert(std::abs(g2.back().x - g1.back().x) < std::abs(g2.front().x - g1.front().x));

    std::puts("input_math_tests: all passed");
    return 0;
}
