#pragma once
// 设置窗口：常规设置 + 区域预设管理 + 信任设备管理（PC-02/PC-03/PC-04）
#include <QDialog>
#include "../core/Config.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QListWidget;
class QComboBox;

namespace screencap::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(core::Config& config, QWidget* parent = nullptr);

private slots:
    void browseSaveDir();
    void addPreset();
    void removePreset();
    void removeDevice();
    void saveAndClose();

private:
    void reloadPresetList();
    void reloadDeviceList();
    void buildUi();

    core::Config& config_;

    // 常规设置
    QLineEdit* nameEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QLineEdit* saveDirEdit_ = nullptr;
    QCheckBox* saveChk_ = nullptr;
    QSpinBox* qualitySpin_ = nullptr;

    // 区域预设
    QListWidget* presetList_ = nullptr;
    QLineEdit* presetName_ = nullptr;
    QSpinBox* presetX_ = nullptr;
    QSpinBox* presetY_ = nullptr;
    QSpinBox* presetW_ = nullptr;
    QSpinBox* presetH_ = nullptr;
    QComboBox* presetMonitor_ = nullptr;

    // 信任设备
    QListWidget* deviceList_ = nullptr;
};

} // namespace screencap::ui
