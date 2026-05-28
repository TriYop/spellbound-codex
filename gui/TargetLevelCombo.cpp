#include "TargetLevelCombo.h"

namespace gui {

TargetLevelCombo::TargetLevelCombo(QWidget* parent) : QComboBox(parent) {
    addItem("Auto (from preset)");
    for (const auto& p : mt::kTargetLevelProfiles) {
        const QString label = QString("%1 — %2 LUFS / %3 dBTP")
            .arg(QString::fromStdString(p.name))
            .arg(static_cast<double>(p.lufs),         0, 'f', 1)
            .arg(static_cast<double>(p.peakCeiling),  0, 'f', 1);
        addItem(label);
    }
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TargetLevelCombo::onIndexChanged);
}

std::optional<mt::TargetLevelProfile> TargetLevelCombo::currentTarget() const {
    const int idx = currentIndex();
    if (idx <= 0) return std::nullopt;
    return mt::kTargetLevelProfiles[static_cast<size_t>(idx - 1)];
}

void TargetLevelCombo::onIndexChanged(int index) {
    if (index <= 0)
        emit targetChanged(std::nullopt);
    else
        emit targetChanged(mt::kTargetLevelProfiles[static_cast<size_t>(index - 1)]);
}

} // namespace gui
