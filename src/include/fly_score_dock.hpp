#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <QKeySequence>

#include "fly_score_state.hpp"

class QPushButton;
class QSpinBox;
class QLineEdit;
class QComboBox;
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
};

// Forward-declared; defined in fly_score_hotkeys_dialog.hpp
struct FlyHotkeyBinding;

class FlyScoreDock : public QWidget {
	Q_OBJECT
public:
	explicit FlyScoreDock(QWidget *parent = nullptr);
	bool init();

signals:
	void requestSave();

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
	void ensureResourcesDefaults();

private slots:
	void onClearTeamsAndReset();

	void onOpenCustomFieldsDialog();
	void onOpenTimersDialog();
	void onOpenTeamsDialog();

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
	QVector<FlyHotkeyBinding> buildDefaultHotkeyBindings() const;
	QVector<FlyHotkeyBinding> buildMergedHotkeyBindings() const;
	void applyHotkeyBindings(const QVector<FlyHotkeyBinding> &bindings);
	void clearAllShortcuts();

private:
	QString dataDir_;
	FlyState st_;

	// Scoreboard-level toggles
	QCheckBox *swapSides_ = nullptr;
	QCheckBox *showScoreboard_ = nullptr;

	QPushButton *teamsBtn_ = nullptr;

	// Custom fields quick area
	QVBoxLayout *customFieldsLayout_ = nullptr;
	QPushButton *editFieldsBtn_ = nullptr;
	QVector<FlyCustomFieldUi> customFields_;

	// Timers quick area
	QVBoxLayout *timersLayout_ = nullptr;
	QPushButton *editTimersBtn_ = nullptr;
	QVector<FlyTimerUi> timers_;

	// Hotkey bindings + actual shortcuts
	QVector<FlyHotkeyBinding> hotkeyBindings_;
	QVector<QShortcut *> shortcuts_;
};

// Dock helpers (OBS frontend registration)
void fly_create_dock();
void fly_destroy_dock();

FlyScoreDock *fly_get_dock();
