#pragma once

#include "mastertweak/preset.hpp"

#include <QWidget>
#include <vector>

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
QT_END_NAMESPACE

namespace gui {

// Combo box that lists all available presets (bundled + ~/.config/MixAdvice/Presets/).
// Also provides an "Open…" button for loading a preset from an arbitrary XML file.
class PresetSelector : public QWidget {
    Q_OBJECT
public:
    explicit PresetSelector(QWidget* parent = nullptr);

    // Populate from executable directory (bundled) + user config dir.
    void populate(const std::string& executableDir);

    // Currently selected preset, or nullopt if none loaded.
    std::optional<mt::PresetData> currentPreset() const;

signals:
    void presetChanged(const mt::PresetData& preset);
    void manageRequested();

private slots:
    void onComboChanged(int index);
    void onOpenFile();

private:
    QComboBox*  combo_   = nullptr;
    QPushButton* openBtn_ = nullptr;
    QPushButton* manageBtn_ = nullptr;

    std::vector<mt::PresetData> presets_;
};

} // namespace gui
