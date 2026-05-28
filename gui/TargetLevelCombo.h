#pragma once

#include "mastertweak/target_level.hpp"

#include <QComboBox>
#include <optional>

namespace gui {

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
