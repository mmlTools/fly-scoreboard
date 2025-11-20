#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QString>
#include <QVector>

class QKeySequenceEdit;
class QPushButton;
class QStackedWidget;

// Simple binding descriptor
struct FlyHotkeyBinding {
	QString actionId;      // e.g. "home_score_inc", "field_0_home_inc", "timer_1_toggle"
	QString label;         // Human readable label
	QKeySequence sequence; // Assigned shortcut (can be empty)
};

class FlyHotkeysDialog : public QDialog {
	Q_OBJECT
public:
	explicit FlyHotkeysDialog(const QVector<FlyHotkeyBinding> &initial, QWidget *parent = nullptr);

	QVector<FlyHotkeyBinding> bindings() const;

signals:
	void bindingsChanged(const QVector<FlyHotkeyBinding> &bindings);

private slots:
	void onAccept();
	void onShowScoreboard();
	void onShowFields();
	void onShowTimers();

private:
	struct RowWidgets {
		QString actionId;
		QString label;
		QKeySequenceEdit *edit = nullptr;
		int section = 0; // 0 = scoreboard, 1 = fields, 2 = timers
	};

	void buildUi(const QVector<FlyHotkeyBinding> &initial);
	QWidget *createSectionPage(const QString &title, const QVector<FlyHotkeyBinding> &items, int sectionIndex);

	void setActiveSectionButton(int index);

	QVector<RowWidgets> rows_;

	// Center content (right side)
	QStackedWidget *stack_ = nullptr;

	// Left-side navigation buttons
	QPushButton *btnNavScore_ = nullptr;
	QPushButton *btnNavFields_ = nullptr;
	QPushButton *btnNavTimers_ = nullptr;

	// Bottom actions
	QPushButton *btnResetAll_ = nullptr;
};

// -----------------------------------------------------------------------------
// Persisted hotkey storage (hotkeys.json, next to plugin.json)
// -----------------------------------------------------------------------------

QVector<FlyHotkeyBinding> fly_hotkeys_load(const QString &dataDir);
bool fly_hotkeys_save(const QString &dataDir, const QVector<FlyHotkeyBinding> &bindings);
