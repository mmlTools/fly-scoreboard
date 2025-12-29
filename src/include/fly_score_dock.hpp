#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QKeySequence>

#include "fly_score_state.hpp"

class QPushButton;
class QSpinBox;
class QLineEdit;
class QCheckBox;
class QVBoxLayout;
class QLabel;
class QShortcut;

// UI bundle for a single custom field row in the dock
struct FlyCustomFieldUi {
	QWidget *row = nullptr;
	QCheckBox *visibleCheck = nullptr;
	QLabel *labelLbl = nullptr;
	QSpinBox *homeSpin = nullptr;
	QSpinBox *awaySpin = nullptr;
	QPushButton *minusHome = nullptr;
	QPushButton *plusHome = nullptr;
	QPushButton *minusAway = nullptr;
	QPushButton *plusAway = nullptr;
};

// UI bundle for a single timer row in the dock
struct FlyTimerUi {
	QWidget *row = nullptr;
	QLabel *labelLbl = nullptr;
	QLineEdit *timeEdit = nullptr;
	QPushButton *startStop = nullptr;
	QPushButton *reset = nullptr;
	QCheckBox *visibleCheck = nullptr;
};

// Forward-declared; defined in fly_score_hotkeys_dialog.hpp
struct FlyHotkeyBinding;

class FlyScoreDock : public QWidget {
	Q_OBJECT
public:
	explicit FlyScoreDock(QWidget *parent = nullptr);
	bool init();

public slots:
	// Match stats from hotkeys
	void bumpCustomFieldHome(int index, int delta);
	void bumpCustomFieldAway(int index, int delta);
	void toggleCustomFieldVisible(int index);

	// Timers from hotkeys
	void toggleTimerRunning(int index);

	// Scoreboard level hotkeys
	void toggleSwap();
	void toggleScoreboardVisible();

	// Open hotkeys dialog
	void openHotkeysDialog();

	// Ensure plugin.json exists in current resources path
	void ensureResourcesDefaults();

private slots:
	void onClearTeamsAndReset();

	void onOpenCustomFieldsDialog();
	void onOpenTimersDialog();
	void onOpenTeamsDialog();

	void onSetResourcesPath();
	void onOpenResourcesFolder();

private:
	void loadState();
	void saveState();
	void refreshUiFromState(bool onlyTimeIfRunning = false);

	// Custom fields quick controls
	void clearAllCustomFieldRows();
	void loadCustomFieldControlsFromState();
	void syncCustomFieldControlsToState();

	// Timers quick controls
	void clearAllTimerRows();
	void loadTimerControlsFromState();

	// Hotkeys (dialog-driven, plugin-local)
	QList<FlyHotkeyBinding> buildDefaultHotkeyBindings() const;
	QList<FlyHotkeyBinding> buildMergedHotkeyBindings() const;
	void applyHotkeyBindings(const QList<FlyHotkeyBinding> &bindings);
	void clearAllShortcuts();

	// Browser source sync
	void updateBrowserSourceToCurrentResources();

private:
	QString dataDir_;
	FlyState st_;

	// Scoreboard-level toggles
	QCheckBox *swapSides_ = nullptr;
	QCheckBox *showScoreboard_ = nullptr;

	QPushButton *teamsBtn_ = nullptr;
	QPushButton *editFieldsBtn_ = nullptr;
	QPushButton *editTimersBtn_ = nullptr;

	// Quick controls layouts
	QVBoxLayout *customFieldsLayout_ = nullptr;
	QList<FlyCustomFieldUi> customFields_;

	QVBoxLayout *timersLayout_ = nullptr;
	QList<FlyTimerUi> timers_;

	// Footer buttons
	QPushButton *setResourcesPathBtn_ = nullptr;
	QPushButton *openResourcesFolderBtn_ = nullptr;

	// Hotkey bindings + actual shortcuts
	QList<FlyHotkeyBinding> hotkeyBindings_;
	QList<QShortcut *> shortcuts_;
};

// Dock helpers (OBS frontend registration)
void fly_create_dock();
void fly_destroy_dock();
FlyScoreDock *fly_get_dock();
