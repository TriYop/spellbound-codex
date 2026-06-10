#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QToolButton;
QT_END_NAMESPACE

namespace gui {

class RackUnit : public QWidget {
    Q_OBJECT
public:
    explicit RackUnit(const QString& unitId, const QString& title,
                      QWidget* body, QWidget* parent = nullptr);

    void setBypassed(bool bypassed);
    bool isBypassed() const;
    void setCollapsed(bool collapsed);
    bool isCollapsed() const;
    void setStats(const QString& text);
    void setBadge(const QString& text, bool isAdvised = true);

signals:
    void bypassChanged(bool bypassed);
    void collapseChanged(bool collapsed);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void applyCollapseState();

    QString       unitId_;
    QWidget*      body_      = nullptr;
    QWidget*      header_    = nullptr;
    QPushButton*  ledBtn_    = nullptr;
    QToolButton*  colBtn_    = nullptr;
    QLabel*       statsLbl_  = nullptr;
    QLabel*       badgeLbl_  = nullptr;
    bool          bypassed_  = false;
    bool          collapsed_ = false;
};

} // namespace gui
