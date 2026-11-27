#include "DesktopInput.h"
#include "ScriptingCore.h"
#include <windowsx.h>

USING_NS_CC;

static const int kSteps = 6;              // points per gesture after the first (one per frame)
static const float kPxPerNotch = 120.f;   // drag distance per wheel notch, in frame pixels
static const int kTouchA = 7, kTouchB = 8;  // ids no real mouse touch uses (the mouse is id 0)

static DesktopInput* s_instance = NULL;

void DesktopInput::install()
{
    if (s_instance) return;
    s_instance = new DesktopInput();
    CCDirector::sharedDirector()->getScheduler()->scheduleUpdateForTarget(s_instance, 0, false);
    CCEGLView::sharedOpenGLView()->setWndProc(&DesktopInput::onWindowMessage);
}

// Window y points down while the path math uses y up: negate y on the way in and out.
static Pt toUp(float x, float y) { return { x, -y }; }

LRESULT DesktopInput::onWindowMessage(UINT message, WPARAM wParam, LPARAM lParam, BOOL* processed)
{
    if (!s_instance || (GetKeyState(VK_LBUTTON) & 0x8000)) return 0;   // a real drag is in progress
    CCEGLView* view = CCEGLView::sharedOpenGLView();

    if (message == WM_MOUSEWHEEL)
    {
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(view->getHWnd(), &p);
        float zoom = view->getFrameZoomFactor();
        Pt at = toUp(p.x / zoom, p.y / zoom);
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        WORD keys = GET_KEYSTATE_WPARAM(wParam);
        Gesture g;
        if (keys & MK_CONTROL)
        {
            std::pair<std::vector<Pt>, std::vector<Pt>> fingers = pinchPaths(at, delta, kPxPerNotch / 2, kSteps);
            g.a = fingers.first;
            g.b = fingers.second;
        }
        else
        {
            g.a = wheelDragPath(at, delta, (keys & MK_SHIFT) ? Axis::Horizontal : Axis::Vertical, kPxPerNotch, kSteps);
        }
        s_instance->queue(g);
        *processed = TRUE;
    }
    else if (message == WM_KEYDOWN && wParam == VK_RETURN)
    {
        // shim.js decides what Enter means (confirm a one-button dialog); text fields get Enter themselves
        jsval ignored;
        ScriptingCore::getInstance()->evalString("if (pc.onEnter) pc.onEnter();", &ignored);
        *processed = TRUE;
    }
    else if (message == WM_KEYDOWN && (wParam == VK_LEFT || wParam == VK_RIGHT))
    {
        CCSize frame = view->getFrameSize();
        Gesture g;
        g.a = arrowSwipePath(toUp(frame.width / 2, frame.height / 2), wParam == VK_RIGHT, frame.width, kSteps);
        s_instance->queue(g);
        *processed = TRUE;
    }
    return 0;
}

void DesktopInput::update(float)
{
    if (m_queue.empty()) return;
    Gesture& g = m_queue.front();
    int ids[2] = { kTouchA, kTouchB };
    float xs[2], ys[2];
    int n = g.b.empty() ? 1 : 2;
    xs[0] = g.a[g.next].x; ys[0] = -g.a[g.next].y;
    if (n == 2) { xs[1] = g.b[g.next].x; ys[1] = -g.b[g.next].y; }

    CCEGLView* view = CCEGLView::sharedOpenGLView();
    if (g.next == 0) view->handleTouchesBegin(n, ids, xs, ys);
    else if (g.next + 1 < g.a.size()) view->handleTouchesMove(n, ids, xs, ys);
    else view->handleTouchesEnd(n, ids, xs, ys);

    if (++g.next == g.a.size()) m_queue.pop_front();
}
