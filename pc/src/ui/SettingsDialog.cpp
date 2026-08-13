#include "SettingsDialog.h"
#include "../capture/CaptureEngine.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>

namespace screencap::ui {

SettingsDialog::SettingsDialog(core::Config& config, QWidget* parent)
    : QDialog(parent)
    , config_(config)
{
    setWindowTitle(QStringLiteral("ScreenLink 设置"));
    setMinimumWidth(520);
    buildUi();
    reloadPresetList();
    reloadDeviceList();
}

void SettingsDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    // ---- 常规设置 ----
    auto* general = new QGroupBox(QStringLiteral("常规"), this);
    auto* form = new QFormLayout(general);
    nameEdit_ = new QLineEdit(config_.serverName, general);
    portSpin_ = new QSpinBox(general);
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(config_.listenPort);
    saveDirEdit_ = new QLineEdit(config_.saveDir, general);
    auto* browseBtn = new QPushButton(QStringLiteral("浏览…"), general);
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(saveDirEdit_, 1);
    dirRow->addWidget(browseBtn);
    saveChk_ = new QCheckBox(QStringLiteral("截图后保存到本地目录"), general);
    saveChk_->setChecked(config_.saveEnabled);
    qualitySpin_ = new QSpinBox(general);
    qualitySpin_->setRange(10, 100);
    qualitySpin_->setValue(config_.qualityMax);
    qualitySpin_->setSuffix(QStringLiteral(" %"));

    form->addRow(QStringLiteral("服务器名称"), nameEdit_);
    form->addRow(QStringLiteral("监听端口"), portSpin_);
    form->addRow(QStringLiteral("保存目录"), dirRow);
    form->addRow(QString(), saveChk_);
    form->addRow(QStringLiteral("JPEG 质量上限"), qualitySpin_);
    layout->addWidget(general);

    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseSaveDir);

    // ---- 区域预设 ----
    auto* presetBox = new QGroupBox(QStringLiteral("截图区域预设"), this);
    auto* presetLayout = new QVBoxLayout(presetBox);
    presetList_ = new QListWidget(presetBox);
    presetLayout->addWidget(presetList_);

    auto* pf = new QFormLayout;
    presetName_ = new QLineEdit(presetBox);
    presetName_->setPlaceholderText(QStringLiteral("如：微信区"));
    presetMonitor_ = new QComboBox(presetBox);
    const auto screens = capture::CaptureEngine::screens();
    for (int i = 0; i < screens.size(); ++i)
        presetMonitor_->addItem(QStringLiteral("显示器 %1").arg(i), i);
    if (presetMonitor_->count() == 0)
        presetMonitor_->addItem(QStringLiteral("显示器 0"), 0);
    presetX_ = new QSpinBox(presetBox); presetX_->setRange(-10000, 10000);
    presetY_ = new QSpinBox(presetBox); presetY_->setRange(-10000, 10000);
    presetW_ = new QSpinBox(presetBox); presetW_->setRange(-1, 20000); presetW_->setValue(-1);
    presetH_ = new QSpinBox(presetBox); presetH_->setRange(-1, 20000); presetH_->setValue(-1);
    pf->addRow(QStringLiteral("名称"), presetName_);
    pf->addRow(QStringLiteral("显示器"), presetMonitor_);
    pf->addRow(QStringLiteral("X / Y"), presetX_);
    auto* whRow = new QHBoxLayout;
    whRow->addWidget(presetW_, 1);
    whRow->addWidget(presetH_, 1);
    pf->addRow(QStringLiteral("宽 / 高（-1=全屏）"), whRow);
    presetLayout->addLayout(pf);

    auto* presetBtnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton(QStringLiteral("添加预设"), presetBox);
    auto* delBtn = new QPushButton(QStringLiteral("删除选中"), presetBox);
    presetBtnRow->addWidget(addBtn);
    presetBtnRow->addWidget(delBtn);
    presetLayout->addLayout(presetBtnRow);
    layout->addWidget(presetBox);

    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addPreset);
    connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::removePreset);

    // ---- 信任设备 ----
    auto* deviceBox = new QGroupBox(QStringLiteral("已配对设备"), this);
    auto* devLayout = new QVBoxLayout(deviceBox);
    deviceList_ = new QListWidget(deviceBox);
    devLayout->addWidget(deviceList_);
    auto* devBtnRow = new QHBoxLayout;
    auto* delDevBtn = new QPushButton(QStringLiteral("删除选中设备"), deviceBox);
    devBtnRow->addStretch();
    devBtnRow->addWidget(delDevBtn);
    devLayout->addLayout(devBtnRow);
    layout->addWidget(deviceBox);
    connect(delDevBtn, &QPushButton::clicked, this, &SettingsDialog::removeDevice);

    // ---- 底部 ----
    auto* bottom = new QHBoxLayout;
    auto* saveBtn = new QPushButton(QStringLiteral("保存"), this);
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    bottom->addStretch();
    bottom->addWidget(saveBtn);
    bottom->addWidget(cancelBtn);
    layout->addLayout(bottom);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveAndClose);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::browseSaveDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择截图保存目录"), saveDirEdit_->text());
    if (!dir.isEmpty())
        saveDirEdit_->setText(QDir::toNativeSeparators(dir));
}

void SettingsDialog::addPreset()
{
    const QString name = presetName_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ScreenLink"),
                             QStringLiteral("预设名称不能为空"));
        return;
    }
    if (name == QLatin1String("fullscreen")) {
        QMessageBox::warning(this, QStringLiteral("ScreenLink"),
                             QStringLiteral("\"fullscreen\" 为保留名称，请换一个"));
        return;
    }
    for (const auto& p : config_.presets) {
        if (p.name == name) {
            QMessageBox::warning(this, QStringLiteral("ScreenLink"),
                                 QStringLiteral("预设 \"%1\" 已存在").arg(name));
            return;
        }
    }
    core::Preset p;
    p.name = name;
    p.monitor = presetMonitor_->currentData().toInt();
    p.x = presetX_->value();
    p.y = presetY_->value();
    p.w = presetW_->value();
    p.h = presetH_->value();
    config_.presets.append(p);
    config_.save();
    reloadPresetList();
    presetName_->clear();
}

void SettingsDialog::removePreset()
{
    const int row = presetList_->currentRow();
    if (row < 0)
        return;
    config_.presets.removeAt(row);
    config_.save();
    reloadPresetList();
}

void SettingsDialog::removeDevice()
{
    const int row = deviceList_->currentRow();
    if (row < 0)
        return;
    config_.trustedDevices.removeAt(row);
    config_.save();
    reloadDeviceList();
}

void SettingsDialog::reloadPresetList()
{
    presetList_->clear();
    for (const auto& p : config_.presets) {
        presetList_->addItem(QStringLiteral("%1  [屏%2 %3,%4 %5x%6]")
                                 .arg(p.name)
                                 .arg(p.monitor)
                                 .arg(p.x)
                                 .arg(p.y)
                                 .arg(p.w < 0 ? QStringLiteral("全") : QString::number(p.w))
                                 .arg(p.h < 0 ? QStringLiteral("屏") : QString::number(p.h)));
    }
}

void SettingsDialog::reloadDeviceList()
{
    deviceList_->clear();
    for (const auto& t : config_.trustedDevices)
        deviceList_->addItem(QStringLiteral("%1  (%2)  配对于 %3")
                                 .arg(t.name, t.deviceId.left(8), t.pairedAt));
}

void SettingsDialog::saveAndClose()
{
    config_.serverName = nameEdit_->text().trimmed();
    config_.listenPort = static_cast<quint16>(portSpin_->value());
    config_.saveDir = saveDirEdit_->text().trimmed();
    config_.saveEnabled = saveChk_->isChecked();
    config_.qualityMax = qualitySpin_->value();
    config_.save();
    accept();
}

} // namespace screencap::ui
