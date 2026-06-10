#include "RackUnit.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace gui {

RackUnit::RackUnit(const QString& unitId, const QString& title,
                   QWidget* body, QWidget* parent)
    : QWidget(parent)
    , unitId_(unitId)
    , body_(body)
{
    body_->setParent(this);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── header ────────────────────────────────────────────────────────────────
    header_ = new QWidget(this);
    header_->setFixedHeight(36);
    header_->setStyleSheet("background: #1a1a1a; border-bottom: 1px solid #333;");
    header_->installEventFilter(this);

    auto* hbox = new QHBoxLayout(header_);
    hbox->setContentsMargins(6, 0, 6, 0);
    hbox->setSpacing(6);

    ledBtn_ = new QPushButton(header_);
    ledBtn_->setFixedSize(12, 12);
    ledBtn_->setStyleSheet(
        "QPushButton { background: #2d8a2d; border-radius: 6px; border: none; }"
        "QPushButton:pressed { background: #1a5c1a; }");
    hbox->addWidget(ledBtn_);

    colBtn_ = new QToolButton(header_);
    colBtn_->setFixedSize(16, 16);
    colBtn_->setStyleSheet(
        "QToolButton { background: transparent; color: #aaa; font-size: 9pt; border: none; }");
    hbox->addWidget(colBtn_);

    auto* titleLbl = new QLabel(title.toUpper(), header_);
    titleLbl->setStyleSheet("color: #ddd; font-size: 11pt; font-weight: bold;");
    hbox->addWidget(titleLbl);

    hbox->addStretch();

    statsLbl_ = new QLabel(header_);
    statsLbl_->setStyleSheet("color: #888; font-size: 9pt;");
    hbox->addWidget(statsLbl_);

    badgeLbl_ = new QLabel(header_);
    badgeLbl_->setStyleSheet(
        "color: white; background: #2d8a2d; border-radius: 3px; padding: 0 4px; font-size: 8pt;");
    badgeLbl_->setVisible(false);
    hbox->addWidget(badgeLbl_);

    outer->addWidget(header_);
    outer->addWidget(body_);

    // restore collapse state
    {
        QSettings s("MasterTweak", "MasterTweak");
        collapsed_ = s.value(QString("ChainPanel/%1/collapsed").arg(unitId_), false).toBool();
    }
    applyCollapseState();

    connect(ledBtn_, &QPushButton::clicked, this, [this]() { setBypassed(!bypassed_); });
    connect(colBtn_, &QToolButton::clicked, this, [this]() { setCollapsed(!collapsed_); });
}

bool RackUnit::eventFilter(QObject* obj, QEvent* event) {
    if (obj == header_ && event->type() == QEvent::MouseButtonDblClick) {
        const auto* me = static_cast<QMouseEvent*>(event);
        const QPoint pos = me->pos();
        if (!ledBtn_->geometry().contains(pos) && !colBtn_->geometry().contains(pos)) {
            setCollapsed(!collapsed_);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void RackUnit::setBypassed(bool bypassed) {
    if (bypassed_ == bypassed) return;
    bypassed_ = bypassed;
    const QString color = bypassed_ ? "#8a2d2d" : "#2d8a2d";
    const QString pressedColor = bypassed_ ? "#5c1a1a" : "#1a5c1a";
    ledBtn_->setStyleSheet(
        QString("QPushButton { background: %1; border-radius: 6px; border: none; }"
                "QPushButton:pressed { background: %2; }").arg(color, pressedColor));
    emit bypassChanged(bypassed_);
}

bool RackUnit::isBypassed()  const { return bypassed_;  }
bool RackUnit::isCollapsed() const { return collapsed_; }

void RackUnit::setCollapsed(bool collapsed) {
    if (collapsed_ == collapsed) return;
    collapsed_ = collapsed;
    applyCollapseState();
    QSettings s("MasterTweak", "MasterTweak");
    s.setValue(QString("ChainPanel/%1/collapsed").arg(unitId_), collapsed_);
    emit collapseChanged(collapsed_);
}

void RackUnit::setStats(const QString& text) {
    statsLbl_->setText(text);
}

void RackUnit::setBadge(const QString& text, bool isAdvised) {
    badgeLbl_->setText(text);
    badgeLbl_->setStyleSheet(
        QString("color: white; background: %1; border-radius: 3px; padding: 0 4px; font-size: 8pt;")
            .arg(isAdvised ? "#2d8a2d" : "#2d5c8a"));
    badgeLbl_->setVisible(!text.isEmpty());
}

void RackUnit::applyCollapseState() {
    body_->setVisible(!collapsed_);
    colBtn_->setText(collapsed_ ? QString::fromUtf8("\xe2\x96\xb6")   // ▶
                                 : QString::fromUtf8("\xe2\x96\xbc")); // ▼
}

} // namespace gui
