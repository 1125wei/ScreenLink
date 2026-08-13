#pragma once
// ScreenLink 配置管理（PROTOCOL.md §11）
// config.json 存于 %APPDATA%/ScreenLink/，原子写入（临时文件+rename）
#include <QString>
#include <QList>
#include <QJsonObject>

namespace screencap::core {

struct Preset {
    QString name;      // 预设名（手机端选择）
    int monitor = 0;   // 显示器编号
    int x = 0, y = 0;  // 区域左上角（虚拟桌面坐标）
    int w = -1, h = -1; // -1 表示全屏该显示器
};

struct TrustedDevice {
    QString deviceId;
    QString name;
    QString pairedAt;   // ISO 8601
};

class Config {
public:
    static Config& instance();

    bool load();
    bool save();  // 原子写

    // 数据目录 %APPDATA%/ScreenLink/
    static QString configDir();
    QString configFilePath() const;

    // 配置项
    quint16 listenPort = 8848;
    QString saveDir;         // 截图保存目录（空 = 不保存）
    bool saveEnabled = true;
    int qualityMax = 95;
    bool autostart = false;
    QString serverName = QStringLiteral("我的电脑");

    QList<Preset> presets;
    QList<TrustedDevice> trustedDevices;

    // 便捷：按名查预设（不存在返回 false）
    bool findPreset(const QString& name, Preset* out) const;

private:
    Config() = default;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

} // namespace screencap::core
