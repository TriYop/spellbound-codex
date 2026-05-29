#pragma once
#include <QDialog>
namespace gui {
class PresetBuilderDialog : public QDialog {
    Q_OBJECT
public:
    explicit PresetBuilderDialog(const std::string& executableDir, QWidget* parent = nullptr);
signals:
    void presetExported();
};
} // namespace gui
