#include "PresetBuilderDialog.h"
#include "BrowseTab.h"
#include "CreatePresetTab.h"
#include "IngestTab.h"
#include "ManagePresetsTab.h"
#include "PresetBuilderCtx.h"

#include <QCloseEvent>
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

    tabs_ = new QTabWidget(this);

    ingestTab_  = new IngestTab(*ctx_, this);
    browseTab_  = new BrowseTab(*ctx_, this);
    createTab_  = new CreatePresetTab(*ctx_, this);
    manageTab_ = new ManagePresetsTab(*ctx_, this);

    tabs_->addTab(ingestTab_,  "Ingest");
    tabs_->addTab(browseTab_,  "Browse / Tag");
    tabs_->addTab(createTab_,  "Create Preset");
    tabs_->addTab(manageTab_, "Manage Presets");

    vbox->addWidget(tabs_);

    // Cross-tab wiring
    connect(ingestTab_,  &IngestTab::libraryChanged,
            browseTab_,  &BrowseTab::refresh);
    connect(ingestTab_,  &IngestTab::libraryChanged,
            createTab_,  &CreatePresetTab::refreshTrackList);
    connect(browseTab_,  &BrowseTab::libraryChanged,
            createTab_,  &CreatePresetTab::refreshTrackList);
    connect(createTab_,  &CreatePresetTab::presetExported,
            this,        &PresetBuilderDialog::onPresetExported);
    connect(createTab_,  &CreatePresetTab::presetExported,
            manageTab_,  &ManagePresetsTab::refresh);
    connect(ingestTab_,  &IngestTab::ingesting,
            this,        &PresetBuilderDialog::onIngesting);

    // Initial population
    browseTab_->refresh();
    createTab_->refreshTrackList();
}

void PresetBuilderDialog::onPresetExported() {
    emit presetExported();
}

void PresetBuilderDialog::onIngesting(bool active) {
    tabs_->setTabEnabled(1, !active);  // Browse / Tag
    tabs_->setTabEnabled(2, !active);  // Create Preset
    tabs_->setTabEnabled(3, !active);  // Manage Presets
}

void PresetBuilderDialog::closeEvent(QCloseEvent* e) {
    // Wait for any running ingest to complete before closing,
    // to avoid use-after-free on ctx_ and the repositories.
    if (ingestTab_) ingestTab_->waitForWorker();
    QDialog::closeEvent(e);
}

} // namespace gui
