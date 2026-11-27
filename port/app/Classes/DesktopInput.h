#ifndef __DESKTOP_INPUT_H__
#define __DESKTOP_INPUT_H__

#include "cocos2d.h"
#include "DesktopInputMath.h"

// Desktop input for a touch game: the mouse wheel and trackpad two-finger swipes scroll and turn
// pages (Shift+wheel = sideways), Ctrl+wheel (and trackpad pinch) pinches. Each scroll becomes one
// synthetic touch played over the following frames, so scroll views and page views see a real
// drag. F11 toggles fullscreen; Enter confirms one-button prompts (shim.js). Esc is the engine's
// back key.
class DesktopInput : public cocos2d::CCObject
{
public:
    static void install();
    static void setStartFullscreen(bool on);   // --fullscreen; applied by install()
    virtual void update(float dt);

private:
    static LRESULT onWindowMessage(UINT message, WPARAM wParam, LPARAM lParam, BOOL* processed);
    WheelDrag m_drag;
};

#endif
