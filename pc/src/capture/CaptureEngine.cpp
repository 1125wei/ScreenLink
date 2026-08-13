#include "CaptureEngine.h"
#include <QBuffer>
#include <QDebug>
#include <windows.h>

namespace screencap::capture {

QList<ScreenInfo> CaptureEngine::screens()
{
    QList<ScreenInfo> list;
    struct Ctx { QList<ScreenInfo>* list; } ctx{&list};
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hMon, HDC, LPRECT, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(hMon, &mi);
            ScreenInfo si;
            si.index = c->list->size();
            si.geometry = QRect(mi.rcMonitor.left, mi.rcMonitor.top,
                                mi.rcMonitor.right - mi.rcMonitor.left,
                                mi.rcMonitor.bottom - mi.rcMonitor.top);
            si.name = QString::fromWCharArray(mi.szDevice);
            c->list->append(si);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
    return list;
}

static QImage grabRect(const QRect& r)
{
    if (r.width() <= 0 || r.height() <= 0)
        return {};
    HDC screenDC = GetDC(nullptr);
    if (!screenDC)
        return {};
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bmp = CreateCompatibleBitmap(screenDC, r.width(), r.height());
    if (!bmp) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return {};
    }
    HGDIOBJ old = SelectObject(memDC, bmp);
    BitBlt(memDC, 0, 0, r.width(), r.height(), screenDC, r.x(), r.y(),
           SRCCOPY | CAPTUREBLT);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = r.width();
    bi.bmiHeader.biHeight = -r.height(); // 自顶向下
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    QImage img(r.width(), r.height(), QImage::Format_ARGB32);
    GetDIBits(memDC, bmp, 0, static_cast<UINT>(r.height()), img.bits(), &bi,
              DIB_RGB_COLORS);

    SelectObject(memDC, old);
    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return img;
}

QImage CaptureEngine::captureFullscreen(int monitorIndex)
{
    const auto all = screens();
    if (all.isEmpty())
        return {};
    if (monitorIndex < 0 || monitorIndex >= all.size()) {
        const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        return grabRect(QRect(vx, vy, vw, vh));
    }
    return grabRect(all.at(monitorIndex).geometry);
}

QImage CaptureEngine::captureRegion(const QRect& rect)
{
    return grabRect(rect);
}

QByteArray CaptureEngine::toJpeg(const QImage& img, int quality)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPEG", qBound(1, quality, 100));
    return bytes;
}

} // namespace screencap::capture
