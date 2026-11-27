#include "DesktopInputMath.h"

static std::vector<Pt> line(Pt from, float dx, float dy, int steps)
{
    std::vector<Pt> path;
    for (int i = 0; i <= steps; i++)
    {
        float t = steps > 0 ? (float)i / steps : 1.f;
        path.push_back({ from.x + dx * t, from.y + dy * t });
    }
    return path;
}

std::vector<Pt> wheelDragPath(Pt start, int wheelDelta, Axis axis, float pxPerNotch, int steps)
{
    float distance = wheelDelta / 120.f * pxPerNotch;
    return axis == Axis::Vertical ? line(start, 0, -distance, steps) : line(start, distance, 0, steps);
}

std::vector<Pt> arrowSwipePath(Pt center, bool right, float width, int steps)
{
    float distance = 0.4f * width;
    Pt start = { center.x + (right ? distance : -distance) / 2, center.y };
    return line(start, right ? -distance : distance, 0, steps);
}

std::pair<std::vector<Pt>, std::vector<Pt>> pinchPaths(Pt center, int wheelDelta, float pxPerNotch, int steps)
{
    float spread = wheelDelta / 120.f * pxPerNotch / 2;
    return { line({ center.x - 50, center.y }, -spread, 0, steps), line({ center.x + 50, center.y }, spread, 0, steps) };
}
