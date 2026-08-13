#pragma once
// ScreenLink 截图引擎（GDI，Win10/11 全兼容，MVP）
// 隐私硬约束：仅在被调用时截图，无任何后台轮询采集
#include <QImage>
#include <QRect>
#include <QString>
#include <QList>

namespace screencap::capture {

struct ScreenInfo {
    int index = 0;
    QRect geometry;   // 虚拟桌面坐标（物理像素）
    QString name;     // 设备名（\\.\DISPLAY1）
};

class CaptureEngine {
public:
    // 枚举显示器（虚拟桌面坐标系）
    static QList<ScreenInfo> screens();

    // 全屏截图：monitorIndex >= 0 指定显示器；-1 为所有显示器（虚拟桌面）
    static QImage captureFullscreen(int monitorIndex = -1);

    // 区域截图：rect 为虚拟桌面坐标
    static QImage captureRegion(const QRect& rect);

    // JPEG 编码（quality 1-100，越界自动夹取）
    static QByteArray toJpeg(const QImage& img, int quality = 85);
};

} // namespace screencap::capture
