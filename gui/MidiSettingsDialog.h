#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

namespace gui {
class MidiController;
}

namespace gui {

class MidiSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit MidiSettingsDialog(MidiController* midi, QWidget* parent = nullptr);

private slots:
    void onApply();

private:
    void loadSettings();
    void saveSettings(const QString& inName, const QString& outName);

    MidiController* midi_;
    QComboBox*      inCombo_  = nullptr;
    QComboBox*      outCombo_ = nullptr;
};

} // namespace gui
