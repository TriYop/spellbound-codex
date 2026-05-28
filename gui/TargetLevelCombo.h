#pragma once

#include "mastertweak/target_level.hpp"

#include <QComboBox>
#include <optional>

namespace gui {

// QComboBox listing the 8 built-in TargetLevelProfiles plus an "Auto" entry.
// Emits targetChanged(nullopt) for Auto, targetChanged(profile) otherwise.
class TargetLevelCombo : public QComboBox {
    Q_OBJECT
public:
    explicit TargetLevelCombo(QWidget* parent = nullptr);
    std::optional<mt::TargetLevelProfile> currentTarget() const;

signals:
    void targetChanged(std::optional<mt::TargetLevelProfile> profile);

private slots:
    void onIndexChanged(int index);
};

} // namespace gui
