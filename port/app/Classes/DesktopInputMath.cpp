#include "DesktopInputMath.h"
#include <algorithm>

Pt wheelOffset(int delta, bool horizontalWheel, bool shift)
{
    if (horizontalWheel) return { (float)-delta, 0 };
    if (shift) return { (float)delta, 0 };
    return { 0, (float)-delta };
}

void WheelDrag::start(Pt at, bool pinch)
{
    m_active = true;
    m_begun = false;
    m_pinch = pinch;
    m_pos = at;
    m_spread = 100;
    m_pending = { 0, 0 };
}

void WheelDrag::scroll(Pt at, float dx, float dy)
{
    if (!m_active) start(at, false);
    if (m_pinch) return;   // mid-pinch: dropped
    m_pending.x += dx;
    m_pending.y += dy;
    m_idle = 0;
}

void WheelDrag::pinch(Pt at, float spread)
{
    if (!m_active) start(at, true);
    if (!m_pinch) return;   // mid-scroll: dropped
    m_pending.x += spread;
    m_idle = 0;
}

static float take(float& pending)
{
    float step = std::max(-WheelDrag::kMaxStep, std::min(WheelDrag::kMaxStep, pending));
    pending -= step;
    return step;
}

WheelDrag::Touch WheelDrag::frame()
{
    if (!m_active) return { None, false, { 0, 0 }, { 0, 0 } };
    Phase phase = Move;
    if (!m_begun)
        m_begun = true, phase = Begin;
    else if (m_pending.x != 0 || m_pending.y != 0)
    {
        if (m_pinch) m_spread = std::max(10.f, m_spread + take(m_pending.x));
        else m_pos = { m_pos.x + take(m_pending.x), m_pos.y + take(m_pending.y) };
    }
    else if (++m_idle >= kIdleFrames)
        m_active = false, phase = End;

    if (!m_pinch) return { phase, false, m_pos, { 0, 0 } };
    return { phase, true, { m_pos.x - m_spread / 2, m_pos.y }, { m_pos.x + m_spread / 2, m_pos.y } };
}
