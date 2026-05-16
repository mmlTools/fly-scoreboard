#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QKeySequence>
#include <QToolButton>

#include "fly_score_state.hpp"

class QPushButton;
class QSpinBox;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QVBoxLayout;
class QLabel;
class QShortcut;
class QJsonObject;
class FlyScoreWebSocketServer;

struct FlyCustomFieldUi {
	QWidget *row = nullptr;
	QCheckBox *visibleCheck = nullptr;
	QLabel *labelLbl = nullptr;
	QSpinBox *homeSpin = nullptr;
	QSpinBox *awaySpin = nullptr;
	QToolButton *minusHome = nullptr;
	QToolButton *plusHome = nullptr;
	QToolButton *minusAway = nullptr;
	QToolButton *plusAway = nullptr;
};

struct FlySingleStatUi {
	QWidget *row = nullptr;
	QCheckBox *visibleCheck = nullptr;
	QLabel *labelLbl = nullptr;
	QSpinBox *valueSpin = nullptr;
	QToolButton *minusBtn = nullptr;
	QToolButton *plusBtn = nullptr;
};

struct FlyTimerUi {
	QWidget *row = nullptr;
	QLabel *labelLbl = nullptr;
	QLineEdit *timeEdit = nullptr;
	QPushButton *startStop = nullptr;
	QPushButton *reset = nullptr;
	QCheckBox *visibleCheck = nullptr;
};

struct FlyHotkeyBinding;

class FlyScoreDock : public QWidget {
	Q_OBJECT
public:
	explicit FlyScoreDock(QWidget *parent = nullptr);
	~FlyScoreDock() override;
	bool init();

	void refreshBrowserSourceCombo(bool preserveSelection = true);
	QString selectedBrowserSourceName() const;

public slots:
	void bumpCustomFieldHome(int index, int delta);
	void bumpCustomFieldAway(int index, int delta);
	void toggleCustomFieldVisible(int index);
	void bumpSingleStat(int index, int delta);
	void toggleSingleStatVisible(int index);
	void toggleTimerRunning(int index);
	void toggleSwap();
	void toggleScoreboardVisible();
	void openHotkeysDialog();
	void ensureResourcesDefaults();

private slots:
	void onClearTeamsAndReset();

	void onOpenCustomFieldsDialog();
	void onOpenTimersDialog();
	void onOpenTeamsDialog();

	void onSetResourcesPath();
	void onOpenResourcesFolder();
	void onSetTemplatesRoot();

private:
	void loadState();
	void saveState();
	void refreshUiFromState(bool onlyTimeIfRunning = false);
	void clearAllCustomFieldRows();
	void loadCustomFieldControlsFromState();
	void syncCustomFieldControlsToState();
	void clearAllSingleStatRows();
	void loadSingleStatControlsFromState();
	void syncSingleStatControlsToState();
	void clearAllTimerRows();
	void loadTimerControlsFromState();
	QList<FlyHotkeyBinding> buildDefaultHotkeyBindings() const;
	QList<FlyHotkeyBinding> buildMergedHotkeyBindings() const;
	void applyHotkeyBindings(const QList<FlyHotkeyBinding> &bindings);
	void clearAllShortcuts();
	void updateBrowserSourceToCurrentResources();
	void refreshTemplateCombo(bool preserveSelection = true);
	QString selectedTemplateName() const;
	QString selectedTemplatePath() const;
	void loadTemplateByPath(const QString &path);
	void broadcastCurrentState();
	void updateWebSocketStatus();
	void handleRemoteCommand(const QJsonObject &command);
	QWidget *widgetCarousel_ = nullptr;
	QPushButton *toggleCarouselBtn_ = nullptr;
	void toggleWidgetCarouselVisible();
	void refreshWidgetCarouselToggleUi();

private:
	QString dataDir_;
	FlyState st_;
	QCheckBox *swapSides_ = nullptr;
	QCheckBox *showScoreboard_ = nullptr;
	QPushButton *teamsBtn_ = nullptr;
	QPushButton *editFieldsBtn_ = nullptr;
	QPushButton *editTimersBtn_ = nullptr;
	QVBoxLayout *customFieldsLayout_ = nullptr;
	QList<FlyCustomFieldUi> customFields_;
	QVBoxLayout *singleStatsLayout_ = nullptr;
	QList<FlySingleStatUi> singleStats_;
	QVBoxLayout *timersLayout_ = nullptr;
	QList<FlyTimerUi> timers_;
	void *obsSignalHandler_ = nullptr;
	bool obsSignalsConnected_ = false;
	QComboBox *browserSourceCombo_ = nullptr;
	QComboBox *templateCombo_ = nullptr;
	QLabel *webSocketStatus_ = nullptr;
	QPushButton *setResourcesPathBtn_ = nullptr;
	QPushButton *openResourcesFolderBtn_ = nullptr;
	QPushButton *setTemplatesRootBtn_ = nullptr;
	FlyScoreWebSocketServer *webSocketServer_ = nullptr;
	QList<FlyHotkeyBinding> hotkeyBindings_;
	QList<QShortcut *> shortcuts_;
};

void fly_create_dock();
void fly_destroy_dock();
FlyScoreDock *fly_get_dock();
