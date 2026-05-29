#pragma once

#include <QDialog>
#include <memory>
#include <string>

namespace gui {

class PresetBuilderCtx;
class IngestTab;
class BrowseTab;
class CreatePresetTab;

class PresetBuilderDialog : public QDialog {
    Q_OBJECT
public:
    explicit PresetBuilderDialog(const std::string& executableDir,
                                 QWidget* parent = nullptr);
    ~PresetBuilderDialog() override;

signals:
    // Emitted when a preset is exported to ~/.config/MixAdvice/Presets/ so
    // PresetSelector can refresh.
    void presetExported();

private:
    void buildUi();
    void onPresetExported();

    std::string              execDir_;
    std::unique_ptr<PresetBuilderCtx> ctx_;

    IngestTab*       ingestTab_  = nullptr;
    BrowseTab*       browseTab_  = nullptr;
    CreatePresetTab* createTab_  = nullptr;
};

} // namespace gui
