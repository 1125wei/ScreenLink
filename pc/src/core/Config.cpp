#include "Config.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDateTime>

namespace screencap::core {

Config& Config::instance()
{
    static Config inst;
    return inst;
}

QString Config::configDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // AppDataLocation 需 QCoreApplication 设置 app 名；兜底用 APPDATA/ScreenLink
    QString dir = base.isEmpty() ? QStringLiteral("%1/ScreenLink").arg(QDir::homePath())
                                 : base;
    QDir d(dir);
    if (!d.exists())
        d.mkpath(QStringLiteral("."));
    return dir;
}

QString Config::configFilePath() const
{
    return QDir(configDir()).filePath(QStringLiteral("config.json"));
}

bool Config::load()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;
    fromJson(doc.object());
    return true;
}

bool Config::save()
{
    QSaveFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return f.commit();
}

bool Config::findPreset(const QString& name, Preset* out) const
{
    for (const auto& p : presets) {
        if (p.name == name) {
            if (out)
                *out = p;
            return true;
        }
    }
    return false;
}

QJsonObject Config::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("listen_port")] = listenPort;
    obj[QStringLiteral("save_dir")] = saveDir;
    obj[QStringLiteral("save_enabled")] = saveEnabled;
    obj[QStringLiteral("quality_max")] = qualityMax;
    obj[QStringLiteral("autostart")] = autostart;
    obj[QStringLiteral("server_name")] = serverName;

    QJsonArray pa;
    for (const auto& p : presets) {
        QJsonObject po;
        po[QStringLiteral("name")] = p.name;
        po[QStringLiteral("monitor")] = p.monitor;
        po[QStringLiteral("x")] = p.x;
        po[QStringLiteral("y")] = p.y;
        po[QStringLiteral("w")] = p.w;
        po[QStringLiteral("h")] = p.h;
        pa.append(po);
    }
    obj[QStringLiteral("presets")] = pa;

    QJsonArray td;
    for (const auto& t : trustedDevices) {
        QJsonObject to;
        to[QStringLiteral("device_id")] = t.deviceId;
        to[QStringLiteral("name")] = t.name;
        to[QStringLiteral("paired_at")] = t.pairedAt;
        td.append(to);
    }
    obj[QStringLiteral("trusted_devices")] = td;
    return obj;
}

void Config::fromJson(const QJsonObject& obj)
{
    listenPort = static_cast<quint16>(obj.value(QStringLiteral("listen_port")).toInt(8848));
    saveDir = obj.value(QStringLiteral("save_dir")).toString();
    saveEnabled = obj.value(QStringLiteral("save_enabled")).toBool(true);
    qualityMax = obj.value(QStringLiteral("quality_max")).toInt(95);
    autostart = obj.value(QStringLiteral("autostart")).toBool(false);
    serverName = obj.value(QStringLiteral("server_name")).toString(QStringLiteral("我的电脑"));

    presets.clear();
    for (const auto& v : obj.value(QStringLiteral("presets")).toArray()) {
        const QJsonObject po = v.toObject();
        Preset p;
        p.name = po.value(QStringLiteral("name")).toString();
        p.monitor = po.value(QStringLiteral("monitor")).toInt(0);
        p.x = po.value(QStringLiteral("x")).toInt(0);
        p.y = po.value(QStringLiteral("y")).toInt(0);
        p.w = po.value(QStringLiteral("w")).toInt(-1);
        p.h = po.value(QStringLiteral("h")).toInt(-1);
        presets.append(p);
    }

    trustedDevices.clear();
    for (const auto& v : obj.value(QStringLiteral("trusted_devices")).toArray()) {
        const QJsonObject to = v.toObject();
        TrustedDevice t;
        t.deviceId = to.value(QStringLiteral("device_id")).toString();
        t.name = to.value(QStringLiteral("name")).toString();
        t.pairedAt = to.value(QStringLiteral("paired_at")).toString();
        trustedDevices.append(t);
    }
}

} // namespace screencap::core
