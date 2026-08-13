// ScreenLink 电脑端入口：托盘常驻后台运行
#include "ui/App.h"
#include <QApplication>
#include <QLockFile>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QMessageBox>
#include <QIcon>

// GUI 程序无控制台：qDebug 写文件，便于诊断（%APPDATA%/ScreenLink/log.txt）
static void logToFile(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile f(dir + QStringLiteral("/log.txt"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz "))
           << msg << Qt::endl;
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ScreenLink"));
    app.setQuitOnLastWindowClosed(false); // 关窗口不退出，托盘常驻
    app.setWindowIcon(QIcon(QStringLiteral(":/app_icon.ico"))); // 应用/任务栏/托盘统一图标
    qInstallMessageHandler(logToFile);

    // 单实例保护：重复启动直接提示退出（避免端口冲突）
    QLockFile lockFile(QDir::temp().filePath(QStringLiteral("screenlink.lock")));
    lockFile.setStaleLockTime(0); // 不自动接管陈旧锁（防止误判，进程崩溃后锁文件由 OS 释放）
    if (!lockFile.tryLock(100)) {
        QMessageBox::information(nullptr, QStringLiteral("ScreenLink"),
                                 QStringLiteral("ScreenLink 已在运行（托盘图标处查看）。"));
        return 0;
    }

    screencap::ui::App slApp;
    if (!slApp.init())
        return 1;
    return app.exec();
}
