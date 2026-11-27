#ifndef __DESKTOP_INPUT_H__
#define __DESKTOP_INPUT_H__

#include "cocos2d.h"
#include "DesktopInputMath.h"
#include <deque>

// Desktop input for a touch game: mouse wheel scrolls (Shift = sideways), Ctrl+wheel pinches,
// Left/Right arrows swipe pages. Each becomes a synthetic touch gesture, played one point per
// frame so scroll views and page views see a real drag. Esc is handled by the engine (back key).
class DesktopInput : public cocos2d::CCObject
{
public:
    static void install();
    virtual void update(float dt);

private:
    struct Gesture
    {
        std::vector<Pt> a, b;   // b is empty for one-finger gestures
        size_t next = 0;
    };
    void queue(Gesture g) { m_queue.push_back(g); }
    static LRESULT onWindowMessage(UINT message, WPARAM wParam, LPARAM lParam, BOOL* processed);

    std::deque<Gesture> m_queue;
};

#endif
