#include "DesktopInput.h"
#include "ScriptingCore.h"
#include <windowsx.h>

USING_NS_CC;

static const int kTouchA = 7, kTouchB = 8;  // ids no real mouse touch uses (the mouse is id 0)

static DesktopInput* s_instance = NULL;
static bool s_startFullscreen = false;

// F11: borderless window covering the current monitor, and back. The engine's WM_SIZE
// handling (patch 0011) rescales the game to the new client size.
static bool s_fullscreen = false;
static LONG s_savedStyle = 0;
static RECT s_savedRect = { 0, 0, 0, 0 };

static void toggleFullscreen(HWND hwnd)
{
    if (!s_fullscreen)
    {
        s_savedStyle = GetWindowLong(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &s_savedRect);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLong(hwnd, GWL_STYLE, (s_savedStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER)) | WS_POPUP);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        SetWindowLong(hwnd, GWL_STYLE, s_savedStyle);
        SetWindowPos(hwnd, NULL, s_savedRect.left, s_savedRect.top, s_savedRect.right - s_savedRect.left,
                     s_savedRect.bottom - s_savedRect.top, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    s_fullscreen = !s_fullscreen;
}

void DesktopInput::setStartFullscreen(bool on) { s_startFullscreen = on; }

void DesktopInput::install()
{
    if (s_instance) return;
    s_instance = new DesktopInput();
    CCDirector::sharedDirector()->getScheduler()->scheduleUpdateForTarget(s_instance, 0, false);
    CCEGLView::sharedOpenGLView()->setWndProc(&DesktopInput::onWindowMessage);
    if (s_startFullscreen) toggleFullscreen(CCEGLView::sharedOpenGLView()->getHWnd());
}

LRESULT DesktopInput::onWindowMessage(UINT message, WPARAM wParam, LPARAM lParam, BOOL* processed)
{
    if (!s_instance || (GetKeyState(VK_LBUTTON) & 0x8000)) return 0;   // a real drag is in progress
    CCEGLView* view = CCEGLView::sharedOpenGLView();

    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
    {
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(view->getHWnd(), &p);
        float zoom = view->getFrameZoomFactor();
        Pt at = { p.x / zoom, -p.y / zoom };   // window y points down; WheelDrag works in y-up
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        WORD keys = GET_KEYSTATE_WPARAM(wParam);
        if (message == WM_MOUSEWHEEL && (keys & MK_CONTROL))
            s_instance->m_drag.pinch(at, delta / 2.f);
        else
        {
            Pt d = wheelOffset(delta, message == WM_MOUSEHWHEEL, (keys & MK_SHIFT) != 0);
            s_instance->m_drag.scroll(at, d.x, d.y);
        }
        *processed = TRUE;
    }
    else if (message == WM_KEYDOWN && wParam == VK_F11)
    {
        toggleFullscreen(view->getHWnd());
        *processed = TRUE;
    }
    else if (message == WM_KEYDOWN && wParam == VK_RETURN)
    {
        // shim.js decides what Enter means (confirm a one-button dialog); text fields get Enter themselves
        jsval ignored;
        ScriptingCore::getInstance()->evalString("if (pc.onEnter) pc.onEnter();", &ignored);
        *processed = TRUE;
    }
    return 0;
}

void DesktopInput::update(float)
{
    WheelDrag::Touch t = m_drag.frame();
    if (t.phase == WheelDrag::None) return;
    int ids[2] = { kTouchA, kTouchB };
    float xs[2] = { t.a.x, t.b.x }, ys[2] = { -t.a.y, -t.b.y };
    int n = t.pinch ? 2 : 1;

    CCEGLView* view = CCEGLView::sharedOpenGLView();
    if (t.phase == WheelDrag::Begin) view->handleTouchesBegin(n, ids, xs, ys);
    else if (t.phase == WheelDrag::Move) view->handleTouchesMove(n, ids, xs, ys);
    else view->handleTouchesEnd(n, ids, xs, ys);
}
