#ifndef __DESKTOP_INPUT_MATH_H__
#define __DESKTOP_INPUT_MATH_H__

// Turns wheel and trackpad scrolling (and Ctrl+wheel pinch) into the touch drags the game
// understands. Engine-free; cocos coordinates (y points up).

struct Pt { float x, y; };

// The finger offset for one wheel message: wheel down drags up (content scrolls down), and
// Shift+wheel down or a sideways swipe to the right (WM_MOUSEHWHEEL > 0) drags left, to the next
// page. 120 delta (one notch) = 120 px.
Pt wheelOffset(int delta, bool horizontalWheel, bool shift);

// One continuous synthetic touch per scroll: the first input puts a finger down at the cursor,
// later input moves it (at most kMaxStep px per frame, so a notch still reads as a drag), and it
// lifts after kIdleFrames quiet frames. A trackpad's stream of small deltas thus becomes one real
// swipe that scroll views and page views can fling.
class WheelDrag
{
public:
    enum Phase { None, Begin, Move, End };
    struct Touch { Phase phase; bool pinch; Pt a, b; };   // b only for a pinch
    static const int kIdleFrames = 6;
    static constexpr float kMaxStep = 40.f;

    void scroll(Pt at, float dx, float dy);   // move the finger by (dx, dy) px
    void pinch(Pt at, float spread);          // change the distance between two fingers by spread px
    Touch frame();                            // once per frame: what to send

private:
    void start(Pt at, bool pinch);
    bool m_active = false, m_begun = false, m_pinch = false;
    Pt m_pos = { 0, 0 };          // finger (or the pinch centre)
    float m_spread = 0;           // distance between pinch fingers
    Pt m_pending = { 0, 0 };      // not yet sent; a pinch uses x
    int m_idle = 0;
};

#endif
