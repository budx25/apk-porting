// Wheel / trackpad scrolling as one continuous synthetic touch (cocos coordinates: y points up).
#include "DesktopInputMath.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b) { return std::abs(a - b) < 0.01f; }

// Runs frames until the gesture ends; returns the number of frames that ran.
static int runToEnd(WheelDrag& w, WheelDrag::Touch* last)
{
    for (int i = 1; i < 1000; i++)
    {
        WheelDrag::Touch t = w.frame();
        if (t.phase == WheelDrag::End) { *last = t; return i; }
    }
    assert(!"gesture never ended");
    return 0;
}

int main()
{
    // Directions: wheel down drags up (content scrolls down); Shift+wheel down and a sideways
    // wheel/trackpad swipe to the right drag left (next page). 120 delta = 120 px.
    {
        Pt down = wheelOffset(-120, false, false);
        assert(near(down.x, 0) && near(down.y, 120));
        Pt shiftDown = wheelOffset(-120, false, true);
        assert(near(shiftDown.x, -120) && near(shiftDown.y, 0));
        Pt right = wheelOffset(120, true, false);
        assert(near(right.x, -120) && near(right.y, 0));
    }

    // Nothing happens without wheel input.
    {
        WheelDrag w;
        assert(w.frame().phase == WheelDrag::None);
    }

    // One mouse notch down on a vertical list: finger down at the cursor, drags up 120 px in
    // steps of at most kMaxStep, then lifts after the wheel goes quiet.
    {
        WheelDrag w;
        w.scroll({ 500, 300 }, 0, 120);
        WheelDrag::Touch t = w.frame();
        assert(t.phase == WheelDrag::Begin && !t.pinch && near(t.a.x, 500) && near(t.a.y, 300));
        t = w.frame();
        assert(t.phase == WheelDrag::Move && near(t.a.y, 300 + WheelDrag::kMaxStep));
        WheelDrag::Touch end;
        runToEnd(w, &end);
        assert(near(end.a.x, 500) && near(end.a.y, 420));
    }

    // A trackpad sends many small deltas: they join into one drag (one Begin, one End), and the
    // finger follows their sum.
    {
        WheelDrag w;
        int begins = 0, ends = 0;
        WheelDrag::Touch t;
        for (int i = 0; i < 20; i++)
        {
            w.scroll({ 500, 300 }, -8, 0);   // sideways, 8 px per event, one event per frame
            t = w.frame();
            begins += t.phase == WheelDrag::Begin;
            ends += t.phase == WheelDrag::End;
        }
        assert(begins == 1 && ends == 0);
        runToEnd(w, &t);
        assert(near(t.a.x, 500 - 160) && near(t.a.y, 300));
    }

    // The finger lifts only after kIdleFrames frames without input.
    {
        WheelDrag w;
        w.scroll({ 500, 300 }, 0, 5);
        WheelDrag::Touch t;
        int frames = runToEnd(w, &t);
        assert(frames == 1 + 1 + WheelDrag::kIdleFrames);   // Begin, the 5 px move, then idle frames
        assert(w.frame().phase == WheelDrag::None);         // and a new scroll starts fresh
        w.scroll({ 10, 10 }, 0, 5);
        t = w.frame();
        assert(t.phase == WheelDrag::Begin && near(t.a.x, 10));
    }

    // Pinch: two fingers 100 px apart around the cursor; a positive spread moves them apart.
    {
        WheelDrag w;
        w.pinch({ 568, 320 }, 60);
        WheelDrag::Touch t = w.frame();
        assert(t.phase == WheelDrag::Begin && t.pinch && near(t.b.x - t.a.x, 100));
        runToEnd(w, &t);
        assert(near(t.b.x - t.a.x, 160) && near((t.a.x + t.b.x) / 2, 568));
    }

    // A scroll that arrives during a pinch (or the reverse) is dropped, not mixed in.
    {
        WheelDrag w;
        w.pinch({ 568, 320 }, -40);
        w.frame();
        w.scroll({ 568, 320 }, 0, 120);
        WheelDrag::Touch t;
        runToEnd(w, &t);
        assert(t.pinch && near(t.b.x - t.a.x, 60) && near(t.a.y, 320));
    }

    std::puts("input_math_tests: all passed");
    return 0;
}
