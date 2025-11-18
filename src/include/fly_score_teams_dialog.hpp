#pragma once

#include <QDialog>
#include <QString>

#include "fly_score_state.hpp"

class QLineEdit;
class QToolButton;
class QPushButton;

class FlyTeamsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FlyTeamsDialog(const QString &dataDir,
                            FlyState &state,
                            QWidget *parent = nullptr);
    ~FlyTeamsDialog() override;

private slots:
    void onBrowseHomeLogo();
    void onBrowseAwayLogo();
    void onApply();

private:
    void syncUiFromState();
    void syncStateFromUi();

private:
    QString  dataDir_;
    FlyState &state_;

    // Home team
    QLineEdit   *homeTitle_  = nullptr;
    QLineEdit   *homeSub_    = nullptr;
    QLineEdit   *homeLogo_   = nullptr;
    QToolButton *homeBrowse_ = nullptr;

    // Guests team
    QLineEdit   *awayTitle_  = nullptr;
    QLineEdit   *awaySub_    = nullptr;
    QLineEdit   *awayLogo_   = nullptr;
    QToolButton *awayBrowse_ = nullptr;

    QPushButton *applyBtn_   = nullptr;
};
