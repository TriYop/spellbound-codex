#include "PresetBuilderDialog.h"
#include "BrowseTab.h"
#include "CreatePresetTab.h"
#include "IngestTab.h"
#include "PresetBuilderCtx.h"

#include <QDir>
#include <QTabWidget>
#include <QVBoxLayout>

namespace gui {

PresetBuilderDialog::PresetBuilderDialog(const std::string& executableDir,
                                         QWidget* parent)
    : QDialog(parent, Qt::Window)  // Qt::Window = resizable, non-modal
    , execDir_(executableDir)
{
    setWindowTitle("Preset Builder");
    resize(900, 640);

    // Ensure the MixAdvice config directory exists.
    const QString configDir = QDir::homePath() + "/.config/MixAdvice";
    QDir().mkpath(configDir);
    const std::string dbPath = (configDir + "/preset_builder.db").toStdString();

    ctx_ = std::make_unique<PresetBuilderCtx>(dbPath);

    buildUi();
}

PresetBuilderDialog::~PresetBuilderDialog() = default;

void PresetBuilderDialog::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);

    auto* tabs = new QTabWidget(this);

    ingestTab_  = new IngestTab(*ctx_, this);
    browseTab_  = new BrowseTab(*ctx_, this);
    createTab_  = new CreatePresetTab(*ctx_, this);

    tabs->addTab(ingestTab_,  "Ingest");
    tabs->addTab(browseTab_,  "Browse / Tag");
    tabs->addTab(createTab_,  "Create Preset");

    vbox->addWidget(tabs);

    // Cross-tab wiring
    connect(ingestTab_,  &IngestTab::libraryChanged,
            browseTab_,  &BrowseTab::refresh);
    connect(ingestTab_,  &IngestTab::libraryChanged,
            createTab_,  &CreatePresetTab::refreshTrackList);
    connect(browseTab_,  &BrowseTab::libraryChanged,
            createTab_,  &CreatePresetTab::refreshTrackList);
    connect(createTab_,  &CreatePresetTab::presetExported,
            this,        &PresetBuilderDialog::onPresetExported);

    // Initial population
    browseTab_->refresh();
    createTab_->refreshTrackList();
}

void PresetBuilderDialog::onPresetExported() {
    emit presetExported();
}

} // namespace gui
