#pragma once
#include <QWidget>
#include <QString>
#include <QVector>

#include "fly_score_state.hpp"

class QPushButton;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QVBoxLayout;

struct FlyCustomFieldUi {
    QWidget    *row          = nullptr;
    QLineEdit  *labelEdit    = nullptr;
    QSpinBox   *homeSpin     = nullptr;
    QSpinBox   *awaySpin     = nullptr;
    QCheckBox  *visibleCheck = nullptr;
    QPushButton *removeBtn   = nullptr;
};

class FlyScoreDock : public QWidget {
    Q_OBJECT
public:
    explicit FlyScoreDock(QWidget *parent = nullptr);
    bool init();

signals:
    void requestSave();

public slots:
    void bumpHomeScore(int delta);
    void bumpAwayScore(int delta);
    void bumpHomeRounds(int delta);
    void bumpAwayRounds(int delta);
    void toggleSwap();
    void toggleShow();

private slots:
    void onStartStop();
    void onReset();
    void onTimeEdited();
    void onApply();
    void onClearTeamsAndReset();

    // Custom fields
    void onAddCustomField();

    // Teams dialog
    void onOpenTeamsDialog();

private:
    void loadState();
    void saveState();
    void refreshUiFromState(bool onlyTimeIfRunning = false);

    // Custom fields helpers (wired to FlyState)
    void loadCustomFieldsFromState();
    void saveCustomFieldsToState();
    FlyCustomFieldUi addCustomFieldRow(const QString &label,
                                       int home,
                                       int away,
                                       bool visible);
    void clearAllCustomFieldRows();

private:
    QString  dataDir_;
    FlyState st_;

    QLineEdit  *time_label_ = nullptr;
    QComboBox  *mode_       = nullptr;

    QLineEdit  *time_       = nullptr;
    QPushButton *startStop_ = nullptr;
    QPushButton *reset_     = nullptr;

    QSpinBox *homeScore_  = nullptr;
    QSpinBox *awayScore_  = nullptr;
    QSpinBox *homeRounds_ = nullptr;
    QSpinBox *awayRounds_ = nullptr;

    QCheckBox *swapSides_      = nullptr;
    QCheckBox *showScoreboard_ = nullptr;
    QCheckBox *showRounds_     = nullptr;

    QPushButton *applyBtn_ = nullptr;
    QPushButton *teamsBtn_ = nullptr;

    // Custom stats fields (inside the dock)
    QVBoxLayout *customFieldsLayout_ = nullptr;
    QPushButton *addFieldBtn_        = nullptr;
    QVector<FlyCustomFieldUi> customFields_;
};

void fly_create_dock();
void fly_destroy_dock();

FlyScoreDock *fly_get_dock();
