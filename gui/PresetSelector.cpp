#include "PresetSelector.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>

namespace gui {

PresetSelector::PresetSelector(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    combo_   = new QComboBox(this);
    openBtn_ = new QPushButton("Open…", this);
    openBtn_->setFixedWidth(70);

    layout->addWidget(combo_, 1);
    layout->addWidget(openBtn_);

    connect(combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PresetSelector::onComboChanged);
    connect(openBtn_, &QPushButton::clicked, this, &PresetSelector::onOpenFile);
}

void PresetSelector::populate(const std::string& executableDir) {
    presets_ = mt::enumeratePresets(executableDir);
    combo_->blockSignals(true);
    combo_->clear();
    if (presets_.empty()) {
        combo_->addItem("(no presets found)");
    } else {
        for (const auto& p : presets_)
            combo_->addItem(QString::fromStdString(p.name));
    }
    combo_->blockSignals(false);

    if (!presets_.empty())
        emit presetChanged(presets_.front());
}

std::optional<mt::PresetData> PresetSelector::currentPreset() const {
    const int idx = combo_->currentIndex();
    if (idx < 0 || static_cast<size_t>(idx) >= presets_.size())
        return std::nullopt;
    return presets_[static_cast<size_t>(idx)];
}

void PresetSelector::onComboChanged(int index) {
    if (index >= 0 && static_cast<size_t>(index) < presets_.size())
        emit presetChanged(presets_[static_cast<size_t>(index)]);
}

void PresetSelector::onOpenFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open MixAdvice Preset", QString(), "XML Presets (*.xml);;All Files (*)");
    if (path.isEmpty()) return;

    std::string err;
    auto p = mt::loadPreset(path.toStdString(), &err);
    if (!p) return;  // silently ignore bad XML (user will notice no change)

    presets_.push_back(*p);
    combo_->blockSignals(true);
    combo_->addItem(QString::fromStdString(p->name));
    combo_->setCurrentIndex(combo_->count() - 1);
    combo_->blockSignals(false);
    emit presetChanged(presets_.back());
}

} // namespace gui
