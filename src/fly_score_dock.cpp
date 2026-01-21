#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][dock]"
#include "fly_score_log.hpp"
#include "widget.hpp"

#include "fly_score_dock.hpp"
#include "fly_score_state.hpp"
#include "fly_score_const.hpp"
#include "fly_score_paths.hpp"
#include "fly_score_qt_helpers.hpp"
#include "fly_score_obs_helpers.hpp"
#include "fly_score_logo_helpers.hpp"
#include "fly_score_teams_dialog.hpp"
#include "fly_score_fields_dialog.hpp"
#include "fly_score_timers_dialog.hpp"
#include "fly_score_hotkeys_dialog.hpp"

#include <obs.h>
#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QAbstractButton>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QMetaObject>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>

static void fly_on_source_list_changed(void *data, calldata_t *)
{
	auto *self = static_cast<FlyScoreDock *>(data);
	if (!self)
		return;
	QMetaObject::invokeMethod(self,
		[self]() { self->refreshBrowserSourceCombo(true); },
		Qt::QueuedConnection);
}

#include <QSpinBox>
#include <QTabWidget>
#include <QSpacerItem>

#include <algorithm>

FlyScoreDock::FlyScoreDock(QWidget *parent) : QWidget(parent)
{
	QSizePolicy sp = sizePolicy();
	sp.setHorizontalPolicy(QSizePolicy::Preferred);
	sp.setVerticalPolicy(QSizePolicy::Expanding);
	setSizePolicy(sp);
}

// ------------------------------------------------------------
// Hotkeys
// ------------------------------------------------------------

QList<FlyHotkeyBinding> FlyScoreDock::buildDefaultHotkeyBindings() const
{
	QList<FlyHotkeyBinding> v;

	v.push_back({"swap_sides", tr("Swap Home ↔ Guests"), QKeySequence()});
	v.push_back({"toggle_scoreboard", tr("Show / Hide Scoreboard"), QKeySequence()});

	for (int i = 0; i < st_.custom_fields.size(); ++i) {
		const auto &cf = st_.custom_fields[i];
		const QString label = cf.label.isEmpty() ? tr("Custom field %1").arg(i + 1) : cf.label;
		const QString baseId = QStringLiteral("field_%1").arg(i);

		v.push_back({baseId + "_toggle", tr("Custom: %1 - Toggle visibility").arg(label), QKeySequence()});
		v.push_back({baseId + "_home_inc", tr("Custom: %1 - Home +1").arg(label), QKeySequence()});
		v.push_back({baseId + "_home_dec", tr("Custom: %1 - Home -1").arg(label), QKeySequence()});
		v.push_back({baseId + "_away_inc", tr("Custom: %1 - Guests +1").arg(label), QKeySequence()});
		v.push_back({baseId + "_away_dec", tr("Custom: %1 - Guests -1").arg(label), QKeySequence()});
	}

	for (int i = 0; i < st_.single_stats.size(); ++i) {
		const auto &ss = st_.single_stats[i];
		const QString label = ss.label.isEmpty() ? tr("Single stat %1").arg(i + 1) : ss.label;
		const QString baseId = QStringLiteral("single_%1").arg(i);

		v.push_back({baseId + "_toggle", tr("Single: %1 - Toggle visibility").arg(label), QKeySequence()});
		v.push_back({baseId + "_inc", tr("Single: %1 +1").arg(label), QKeySequence()});
		v.push_back({baseId + "_dec", tr("Single: %1 -1").arg(label), QKeySequence()});
	}

	for (int i = 0; i < st_.timers.size(); ++i) {
		const auto &tm = st_.timers[i];
		const QString label = tm.label.isEmpty() ? tr("Timer %1").arg(i + 1) : tm.label;
		const QString baseId = QStringLiteral("timer_%1").arg(i);

		v.push_back({baseId + "_toggle", tr("Timer: %1 - Start/Pause").arg(label), QKeySequence()});
	}

	return v;
}

QList<FlyHotkeyBinding> FlyScoreDock::buildMergedHotkeyBindings() const
{
	QList<FlyHotkeyBinding> merged = buildDefaultHotkeyBindings();

	if (hotkeyBindings_.isEmpty())
		return merged;

	QHash<QString, QKeySequence> existing;
	existing.reserve(hotkeyBindings_.size());
	for (const auto &b : hotkeyBindings_)
		existing.insert(b.actionId, b.sequence);

	for (auto &b : merged) {
		if (existing.contains(b.actionId))
			b.sequence = existing.value(b.actionId);
	}

	return merged;
}

void FlyScoreDock::clearAllShortcuts()
{
	for (auto *sc : shortcuts_) {
		if (sc)
			sc->deleteLater();
	}
	shortcuts_.clear();
}

void FlyScoreDock::applyHotkeyBindings(const QList<FlyHotkeyBinding> &bindings)
{
	clearAllShortcuts();
	hotkeyBindings_ = bindings;

	// Persist in separate file (existing helper)
	fly_hotkeys_save(dataDir_, hotkeyBindings_);

	for (const auto &b : bindings) {
		if (b.sequence.isEmpty())
			continue;

		auto *sc = new QShortcut(b.sequence, this);
		sc->setContext(Qt::ApplicationShortcut);
		shortcuts_.push_back(sc);

		const QString &id = b.actionId;

		if (id == QLatin1String("swap_sides")) {
			connect(sc, &QShortcut::activated, this, [this]() { toggleSwap(); });
		} else if (id == QLatin1String("toggle_scoreboard")) {
			connect(sc, &QShortcut::activated, this, [this]() { toggleScoreboardVisible(); });

		} else if (id.startsWith(QLatin1String("field_"))) {
			// field_{idx}_{toggle|home_inc|home_dec|away_inc|away_dec}
			const auto parts = id.split(QLatin1Char('_'));
			if (parts.size() < 3)
				continue;

			bool ok = false;
			int idx = parts[1].toInt(&ok);
			if (!ok)
				continue;

			if (parts.size() == 3 && parts[2] == QLatin1String("toggle")) {
				connect(sc, &QShortcut::activated, this,
					[this, idx]() { toggleCustomFieldVisible(idx); });
			} else if (parts.size() == 4) {
				const QString side = parts[2];
				const QString dir = parts[3];

				if (side == QLatin1String("home") && dir == QLatin1String("inc"))
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldHome(idx, +1); });
				else if (side == QLatin1String("home") && dir == QLatin1String("dec"))
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldHome(idx, -1); });
				else if (side == QLatin1String("away") && dir == QLatin1String("inc"))
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldAway(idx, +1); });
				else if (side == QLatin1String("away") && dir == QLatin1String("dec"))
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldAway(idx, -1); });
			}

		} else if (id.startsWith(QLatin1String("single_"))) {
			// single_{idx}_{toggle|inc|dec}
			const auto parts = id.split(QLatin1Char('_'));
			if (parts.size() != 3)
				continue;

			bool ok = false;
			int idx = parts[1].toInt(&ok);
			if (!ok)
				continue;

			const QString action = parts[2];
			if (action == QLatin1String("toggle"))
				connect(sc, &QShortcut::activated, this,
					[this, idx]() { toggleSingleStatVisible(idx); });
			else if (action == QLatin1String("inc"))
				connect(sc, &QShortcut::activated, this, [this, idx]() { bumpSingleStat(idx, +1); });
			else if (action == QLatin1String("dec"))
				connect(sc, &QShortcut::activated, this, [this, idx]() { bumpSingleStat(idx, -1); });

		} else if (id.startsWith(QLatin1String("timer_"))) {
			// timer_{idx}_toggle
			const auto parts = id.split(QLatin1Char('_'));
			if (parts.size() == 3 && parts[2] == QLatin1String("toggle")) {
				bool ok = false;
				int idx = parts[1].toInt(&ok);
				if (!ok)
					continue;

				connect(sc, &QShortcut::activated, this, [this, idx]() { toggleTimerRunning(idx); });
			}
		}
	}
}

void FlyScoreDock::openHotkeysDialog()
{
	// Always rebuild so custom fields/timers are included
	QList<FlyHotkeyBinding> initial = buildMergedHotkeyBindings();

	auto *dlg = new FlyHotkeysDialog(initial, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(dlg, &FlyHotkeysDialog::bindingsChanged, this,
		[this](const QList<FlyHotkeyBinding> &b) { applyHotkeyBindings(b); });

	dlg->show();
}

// ------------------------------------------------------------
// Init/UI
// ------------------------------------------------------------

bool FlyScoreDock::init()
{
	dataDir_ = fly_get_data_root();

	loadState();
	ensureResourcesDefaults();

	hotkeyBindings_ = fly_hotkeys_load(dataDir_);

	setObjectName(QStringLiteral("FlyScoreDock"));
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(QStringLiteral("FlyScoreDock { background: rgba(39, 42, 51, 1.0)}"));

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);

	auto *content = new QWidget(this);
	content->setObjectName(QStringLiteral("flyScoreContent"));
	content->setAttribute(Qt::WA_StyledBackground, true);

	auto *root = new QVBoxLayout(content);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	outer->addWidget(content);

	const QString cardStyle = QStringLiteral("QGroupBox {"
						 "  background-color: rgba(255, 255, 255, 0.06);"
						 "  border: 1px solid rgba(255, 255, 255, 0.10);"
						 "  border-radius: 6px;"
						 "  margin-top: 10px;"
						 "}"
						 "QGroupBox::title {"
						 "  subcontrol-origin: margin;"
						 "  left: 8px;"
						 "  top: -2px;"
						 "  padding: 0 6px;"
						 "}");

	// ---------------------------------------------------------------------
	// Merged card: Scoreboard + Match stats + Timers
	// Header row: Teams | Stats… | Timers…
	// Toggles row: Swap | Show (below buttons)
	// Then stats quick controls, then timers quick controls
	// ---------------------------------------------------------------------
	auto *mainBox = new QGroupBox(QStringLiteral("Scoreboard"), content);
	mainBox->setStyleSheet(cardStyle);

	auto *mainVBox = new QVBoxLayout(mainBox);
	mainVBox->setContentsMargins(8, 8, 8, 8);
	mainVBox->setSpacing(6);

	// Header row (buttons)
	{
		auto *headerRow = new QHBoxLayout();
		headerRow->setContentsMargins(0, 0, 0, 0);
		headerRow->setSpacing(6);

		teamsBtn_ = new QPushButton(QStringLiteral("Teams"), mainBox);
		teamsBtn_->setMinimumWidth(110);
		teamsBtn_->setMaximumWidth(130);
		teamsBtn_->setCursor(Qt::PointingHandCursor);

		editFieldsBtn_ = new QPushButton(QStringLiteral("Stats…"), mainBox);
		editFieldsBtn_->setMinimumWidth(110);
		editFieldsBtn_->setMaximumWidth(130);
		editFieldsBtn_->setCursor(Qt::PointingHandCursor);
		editFieldsBtn_->setToolTip(QStringLiteral("Configure match stats fields"));

		editTimersBtn_ = new QPushButton(QStringLiteral("Timers…"), mainBox);
		editTimersBtn_->setMinimumWidth(110);
		editTimersBtn_->setMaximumWidth(130);
		editTimersBtn_->setCursor(Qt::PointingHandCursor);
		editTimersBtn_->setToolTip(QStringLiteral("Configure timers"));

		headerRow->addWidget(teamsBtn_);
		headerRow->addWidget(editFieldsBtn_);
		headerRow->addWidget(editTimersBtn_);
		headerRow->addStretch(1);

		mainVBox->addLayout(headerRow);
	}

	// Toggles row (below buttons)
	{
		auto *togglesRow = new QHBoxLayout();
		togglesRow->setContentsMargins(0, 0, 0, 0);
		togglesRow->setSpacing(10);

		swapSides_ = new QCheckBox(QStringLiteral("Swap Home ↔ Guests"), mainBox);
		showScoreboard_ = new QCheckBox(QStringLiteral("Show scoreboard"), mainBox);

		togglesRow->addWidget(swapSides_);
		togglesRow->addWidget(showScoreboard_);
		togglesRow->addStretch(1);

		mainVBox->addLayout(togglesRow);
	}

	// Tabbed quick controls: Team Stats | Single Stats | Timers
	{
		auto *tabs = new QTabWidget(mainBox);
		tabs->setObjectName(QStringLiteral("flyScoreTabs"));
		tabs->setDocumentMode(true);
		tabs->setMovable(false);
		tabs->setUsesScrollButtons(true);

		// Team Stats tab
		auto *teamTab = new QWidget(tabs);
		auto *teamVBox = new QVBoxLayout(teamTab);
		teamVBox->setContentsMargins(0, 0, 0, 0);
		teamVBox->setSpacing(4);
		teamVBox->setAlignment(Qt::AlignTop);
		customFieldsLayout_ = teamVBox;

		// Single Stats tab
		auto *singleTab = new QWidget(tabs);
		auto *singleVBox = new QVBoxLayout(singleTab);
		singleVBox->setContentsMargins(0, 0, 0, 0);
		singleVBox->setSpacing(4);
		singleVBox->setAlignment(Qt::AlignTop);
		singleStatsLayout_ = singleVBox;

		// Timers tab
		auto *timersTab = new QWidget(tabs);
		auto *timersVBox = new QVBoxLayout(timersTab);
		timersVBox->setContentsMargins(0, 0, 0, 0);
		timersVBox->setSpacing(4);
		timersVBox->setAlignment(Qt::AlignTop);
		timersLayout_ = timersVBox;

		tabs->addTab(teamTab, tr("Team Stats"));
		tabs->addTab(singleTab, tr("Single Stats"));
		tabs->addTab(timersTab, tr("Timers"));

		mainVBox->addWidget(tabs);
	}

	mainBox->setLayout(mainVBox);
	root->addWidget(mainBox);

	// ---------------------------------------------------------------------
	// Footer row (NO refresh button):
	// Left: add/update browser source, clear, set resources path, open folder
	// Right: hotkeys
	// ---------------------------------------------------------------------
	auto *bottomRow = new QHBoxLayout();
	bottomRow->setContentsMargins(0, 0, 0, 0);
	bottomRow->setSpacing(6);

	browserSourceCombo_ = new QComboBox(content);
	browserSourceCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	browserSourceCombo_->setToolTip(QStringLiteral("Select which Browser Source to sync to the Fly Scoreboard resources"));
	browserSourceCombo_->setMinimumContentsLength(18);
	browserSourceCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

	auto *clearBtn = new QPushButton(QStringLiteral("🧹"), content);
	clearBtn->setCursor(Qt::PointingHandCursor);
	clearBtn->setToolTip(QStringLiteral("Reset stats and timers (keep teams & logos)"));

	setResourcesPathBtn_ = new QPushButton(QStringLiteral("📁"), content);
	setResourcesPathBtn_->setCursor(Qt::PointingHandCursor);
	setResourcesPathBtn_->setToolTip(QStringLiteral("Set resources folder (contains index.html + plugin.json)"));

	openResourcesFolderBtn_ = new QPushButton(QStringLiteral("🗂️"), content);
	openResourcesFolderBtn_->setCursor(Qt::PointingHandCursor);
	openResourcesFolderBtn_->setToolTip(QStringLiteral("Open resources folder"));

	auto *hotkeysBtn = new QPushButton(QStringLiteral("⌨️"), content);
	hotkeysBtn->setCursor(Qt::PointingHandCursor);
	hotkeysBtn->setToolTip(QStringLiteral("Configure hotkeys"));

	bottomRow->addWidget(browserSourceCombo_, 1);
	bottomRow->addWidget(clearBtn);
	bottomRow->addWidget(setResourcesPathBtn_);
	bottomRow->addWidget(openResourcesFolderBtn_);
	bottomRow->addStretch(1);
	bottomRow->addWidget(hotkeysBtn);

	root->addLayout(bottomRow);
	root->addStretch(1);
	root->addWidget(create_widget_carousel(this));

	// ---------------------------------------------------------------------
	// Connections
	// ---------------------------------------------------------------------
	refreshBrowserSourceCombo();
	connect(browserSourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this](int) { updateBrowserSourceToCurrentResources(); });

	// Keep the Browser Source selector up-to-date when sources are created/destroyed.
	obsSignalHandler_ = obs_get_signal_handler();
	if (obsSignalHandler_) {
		auto *sh = static_cast<signal_handler_t *>(obsSignalHandler_);
		signal_handler_connect(sh, "source_create", fly_on_source_list_changed, this);

		signal_handler_connect(sh, "source_destroy", fly_on_source_list_changed, this);

		obsSignalsConnected_ = true;
	}

	connect(swapSides_, &QCheckBox::toggled, this, [this](bool on) {
		st_.swap_sides = on;
		saveState();
	});

	connect(showScoreboard_, &QCheckBox::toggled, this, [this](bool on) {
		st_.show_scoreboard = on;
		saveState();
	});


	connect(clearBtn, &QPushButton::clicked, this, &FlyScoreDock::onClearTeamsAndReset);

	connect(setResourcesPathBtn_, &QPushButton::clicked, this, &FlyScoreDock::onSetResourcesPath);
	connect(openResourcesFolderBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenResourcesFolder);

	connect(editFieldsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenCustomFieldsDialog);
	connect(editTimersBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTimersDialog);
	connect(teamsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTeamsDialog);

	connect(hotkeysBtn, &QPushButton::clicked, this, &FlyScoreDock::openHotkeysDialog);

	// ---------------------------------------------------------------------
	// Initial UI
	// ---------------------------------------------------------------------
	refreshUiFromState(false);

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);

	return true;
}

// ------------------------------------------------------------
// Resources path + browser source update
// ------------------------------------------------------------

void FlyScoreDock::updateBrowserSourceToCurrentResources()
{
	const QString overlayRoot = fly_get_data_root_no_ui();
	if (overlayRoot.isEmpty()) {
		LOGW("Resources folder is empty; cannot update browser source");
		return;
	}

	const QString indexPath = QDir(overlayRoot).filePath(QStringLiteral("index.html"));

	// Ensure JSON exists and is current so overlay reads immediately
	fly_state_ensure_json_exists(overlayRoot, &st_);
	fly_state_save(overlayRoot, st_);

	if (!QFileInfo::exists(indexPath)) {
		LOGW("index.html not found in resources folder: %s", indexPath.toUtf8().constData());
	}

	// Must update existing source or create if missing
	fly_ensure_browser_source_in_current_scene(indexPath, selectedBrowserSourceName());

	LOGI("Browser source synced to: %s", indexPath.toUtf8().constData());
}

void FlyScoreDock::onSetResourcesPath()
{
	const QString cur = fly_get_data_root_no_ui();
	const QString picked = QFileDialog::getExistingDirectory(this, QStringLiteral("Select resources folder"), cur);
	if (picked.isEmpty())
		return;

	fly_set_data_root(picked);
	dataDir_ = fly_get_data_root_no_ui();

	fly_state_ensure_json_exists(dataDir_, &st_);
	fly_state_save(dataDir_, st_);

	// IMPORTANT: update browser source with new path
	updateBrowserSourceToCurrentResources();

	LOGI("Resources folder set to: %s", dataDir_.toUtf8().constData());
}

void FlyScoreDock::onOpenResourcesFolder()
{
	const QString dir = fly_get_data_root_no_ui();
	if (!dir.isEmpty())
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

// ------------------------------------------------------------
// State + UI
// ------------------------------------------------------------

void FlyScoreDock::loadState()
{
	if (!fly_state_load(dataDir_, st_)) {
		st_ = fly_state_make_defaults();
		fly_state_save(dataDir_, st_);
	}

	if (st_.timers.isEmpty()) {
		FlyTimer main;
		main.label = QStringLiteral("First Half");
		main.mode = QStringLiteral("countdown");
		main.running = false;
		main.initial_ms = 0;
		main.remaining_ms = 0;
		main.last_tick_ms = 0;
		st_.timers.push_back(main);
		fly_state_save(dataDir_, st_);
	} else if (st_.timers[0].mode.isEmpty()) {
		st_.timers[0].mode = QStringLiteral("countdown");
	}
}

void FlyScoreDock::saveState()
{
	fly_state_save(dataDir_, st_);
}

void FlyScoreDock::refreshUiFromState(bool onlyTimeIfRunning)
{
	Q_UNUSED(onlyTimeIfRunning);

	if (swapSides_ && swapSides_->isChecked() != st_.swap_sides)
		swapSides_->setChecked(st_.swap_sides);
	if (showScoreboard_ && showScoreboard_->isChecked() != st_.show_scoreboard)
		showScoreboard_->setChecked(st_.show_scoreboard);

	loadCustomFieldControlsFromState();
	loadSingleStatControlsFromState();
	loadTimerControlsFromState();
}

void FlyScoreDock::onClearTeamsAndReset()
{
	auto rc = QMessageBox::question(
		this, QStringLiteral("Reset values"),
		QStringLiteral(
			"Reset all match stats and timers to 0?\nTeams, logos and field/timer configuration will be kept."));
	if (rc != QMessageBox::Yes)
		return;

	for (auto &cf : st_.custom_fields) {
		cf.home = 0;
		cf.away = 0;
	}

	for (auto &ss : st_.single_stats) {
		ss.value = 0;
	}

	for (auto &tm : st_.timers) {
		qint64 ms = tm.initial_ms;
		if (ms < 0)
			ms = 0;
		tm.remaining_ms = ms;
		tm.running = false;
		tm.last_tick_ms = 0;
	}

	saveState();
	refreshUiFromState(false);
}

// ------------------------------------------------------------
// Custom fields quick controls (same logic you already had)
// ------------------------------------------------------------

void FlyScoreDock::clearAllCustomFieldRows()
{
	customFields_.clear();
	if (!customFieldsLayout_)
		return;

	while (QLayoutItem *item = customFieldsLayout_->takeAt(0)) {
		if (QWidget *w = item->widget())
			w->deleteLater();
		delete item;
	}
}

void FlyScoreDock::loadCustomFieldControlsFromState()
{
	clearAllCustomFieldRows();
	if (!customFieldsLayout_)
		return;

	customFields_.reserve(st_.custom_fields.size());

	// header
	{
		auto *hdrRow = new QWidget(this);
		auto *grid = new QGridLayout(hdrRow);
		grid->setContentsMargins(0, 0, 0, 0);
		grid->setHorizontalSpacing(6);
		grid->setVerticalSpacing(0);

		auto *cbSpacer = new QSpacerItem(22, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
		grid->addItem(cbSpacer, 0, 0);

		auto *statHdr = new QLabel(tr("Stat"), hdrRow);
		statHdr->setStyleSheet(QStringLiteral("font-weight:bold;"));
		grid->addWidget(statHdr, 0, 1);

		auto *homeHdr = new QLabel(tr("Home"), hdrRow);
		homeHdr->setStyleSheet(QStringLiteral("font-weight:bold;"));
		homeHdr->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		grid->addWidget(homeHdr, 0, 2);

		auto *guestHdr = new QLabel(tr("Guests"), hdrRow);
		guestHdr->setStyleSheet(QStringLiteral("font-weight:bold;"));
		guestHdr->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		grid->addWidget(guestHdr, 0, 3);

		grid->setColumnStretch(1, 2);
		grid->setColumnStretch(2, 1);
		grid->setColumnStretch(3, 1);

		customFieldsLayout_->addWidget(hdrRow);
	}

	for (int i = 0; i < st_.custom_fields.size(); ++i) {
		const auto &cf = st_.custom_fields[i];
		FlyCustomFieldUi ui;

		auto *row = new QWidget(this);
		auto *grid = new QGridLayout(row);
		grid->setContentsMargins(0, 0, 0, 0);
		grid->setHorizontalSpacing(6);
		grid->setVerticalSpacing(0);

		auto *visibleCheck = new QCheckBox(row);
		visibleCheck->setChecked(cf.visible);
		grid->addWidget(visibleCheck, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

		auto *labelLbl = new QLabel(cf.label.isEmpty() ? QStringLiteral("(unnamed)") : cf.label, row);
		labelLbl->setMinimumWidth(120);
		grid->addWidget(labelLbl, 0, 1);

		auto makeEmojiBtn = [](const QString &emoji, const QString &tooltip, QWidget *parent) {
			auto *btn = new QPushButton(parent);
			btn->setText(emoji);
			btn->setToolTip(tooltip);
			btn->setCursor(Qt::PointingHandCursor);
			btn->setStyleSheet(
				"QPushButton {"
				"  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				"  font-size:8px;"
				"  padding:0;"
				"}");
			return btn;
		};

		auto *homeSpin = new QSpinBox(row);
		homeSpin->setRange(0, 999);
		homeSpin->setValue(std::max(0, cf.home));
		homeSpin->setMaximumWidth(60);
		homeSpin->setMaximumHeight(32);
		homeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

		auto *minusHome = makeEmojiBtn(QStringLiteral("➖"), QStringLiteral("Home -1"), row);
		auto *plusHome = makeEmojiBtn(QStringLiteral("➕"), QStringLiteral("Home +1"), row);

		auto *awaySpin = new QSpinBox(row);
		awaySpin->setRange(0, 999);
		awaySpin->setValue(std::max(0, cf.away));
		awaySpin->setMaximumWidth(60);
		awaySpin->setMaximumHeight(32);
		awaySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

		auto *minusAway = makeEmojiBtn(QStringLiteral("➖"), QStringLiteral("Guests -1"), row);
		auto *plusAway = makeEmojiBtn(QStringLiteral("➕"), QStringLiteral("Guests +1"), row);

		//const int h = homeSpin->sizeHint().height();
		minusHome->setFixedSize(32, 32);
		plusHome->setFixedSize(32, 32);
		minusAway->setFixedSize(32, 32);
		plusAway->setFixedSize(32, 32);

		auto *homeBox = new QWidget(row);
		auto *homeLay = new QHBoxLayout(homeBox);
		homeLay->setContentsMargins(0, 0, 0, 0);
		homeLay->setSpacing(4);
		homeLay->addWidget(minusHome, 0, Qt::AlignHCenter | Qt::AlignVCenter);
		homeLay->addWidget(homeSpin, 0, Qt::AlignHCenter | Qt::AlignVCenter);
		homeLay->addWidget(plusHome, 0, Qt::AlignHCenter | Qt::AlignVCenter);

		auto *awayBox = new QWidget(row);
		auto *awayLay = new QHBoxLayout(awayBox);
		awayLay->setContentsMargins(0, 0, 0, 0);
		awayLay->setSpacing(4);
		awayLay->addWidget(minusAway, 0, Qt::AlignHCenter | Qt::AlignVCenter);
		awayLay->addWidget(awaySpin, 0, Qt::AlignHCenter | Qt::AlignVCenter);
		awayLay->addWidget(plusAway, 0, Qt::AlignHCenter | Qt::AlignVCenter);

		grid->addWidget(homeBox, 0, 2, Qt::AlignHCenter | Qt::AlignVCenter);
		grid->addWidget(awayBox, 0, 3, Qt::AlignHCenter | Qt::AlignVCenter);

		grid->setColumnStretch(1, 2);
		grid->setColumnStretch(2, 1);
		grid->setColumnStretch(3, 1);

		customFieldsLayout_->addWidget(row);

		ui.row = row;
		ui.visibleCheck = visibleCheck;
		ui.labelLbl = labelLbl;
		ui.homeSpin = homeSpin;
		ui.awaySpin = awaySpin;
		ui.minusHome = minusHome;
		ui.plusHome = plusHome;
		ui.minusAway = minusAway;
		ui.plusAway = plusAway;

		auto sync = [this]() {
			syncCustomFieldControlsToState();
		};

		connect(homeSpin, qOverload<int>(&QSpinBox::valueChanged), this, [sync](int) { sync(); });
		connect(awaySpin, qOverload<int>(&QSpinBox::valueChanged), this, [sync](int) { sync(); });
		connect(visibleCheck, &QCheckBox::toggled, this, [sync](bool) { sync(); });

		connect(minusHome, &QPushButton::clicked, this, [homeSpin, sync]() {
			homeSpin->setValue(std::max(0, homeSpin->value() - 1));
			sync();
		});
		connect(plusHome, &QPushButton::clicked, this, [homeSpin, sync]() {
			homeSpin->setValue(homeSpin->value() + 1);
			sync();
		});
		connect(minusAway, &QPushButton::clicked, this, [awaySpin, sync]() {
			awaySpin->setValue(std::max(0, awaySpin->value() - 1));
			sync();
		});
		connect(plusAway, &QPushButton::clicked, this, [awaySpin, sync]() {
			awaySpin->setValue(awaySpin->value() + 1);
			sync();
		});

		customFields_.push_back(ui);
	}
	customFieldsLayout_->addStretch(1);
}

void FlyScoreDock::syncCustomFieldControlsToState()
{
	st_.custom_fields.clear();
	st_.custom_fields.reserve(customFields_.size());

	for (const auto &ui : customFields_) {
		FlyCustomField cf;
		cf.label = ui.labelLbl ? ui.labelLbl->text() : QString();
		cf.home = ui.homeSpin ? ui.homeSpin->value() : 0;
		cf.away = ui.awaySpin ? ui.awaySpin->value() : 0;
		cf.visible = ui.visibleCheck ? ui.visibleCheck->isChecked() : true;
		st_.custom_fields.push_back(cf);
	}

	saveState();
}
// ------------------------------------------------------------
// Single stats quick controls
// ------------------------------------------------------------

void FlyScoreDock::clearAllSingleStatRows()
{
	for (auto &ui : singleStats_) {
		if (singleStatsLayout_ && ui.row)
			singleStatsLayout_->removeWidget(ui.row);
		if (ui.row)
			ui.row->deleteLater();
	}
	singleStats_.clear();
}

void FlyScoreDock::loadSingleStatControlsFromState()
{
	clearAllSingleStatRows();
	if (!singleStatsLayout_)
		return;

	singleStats_.reserve(st_.single_stats.size());

	auto makeEmojiBtn = [](const QString &emoji, const QString &tooltip, QWidget *parent) {
		auto *btn = new QPushButton(parent);
		btn->setText(emoji);
		btn->setToolTip(tooltip);
		btn->setCursor(Qt::PointingHandCursor);
		btn->setStyleSheet("QPushButton {"
				   "  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				   "  font-size:12px;"
				   "  padding:0;"
				   "}");
		return btn;
	};

	for (int i = 0; i < st_.single_stats.size(); ++i) {
		const auto &ss = st_.single_stats[i];
		FlySingleStatUi ui;

		auto *row = new QWidget(this);
		auto *lay = new QHBoxLayout(row);
		lay->setContentsMargins(0, 0, 0, 0);
		lay->setSpacing(6);

		auto *visibleCheck = new QCheckBox(row);
		visibleCheck->setChecked(ss.visible);

		auto *labelLbl = new QLabel(ss.label.isEmpty() ? tr("Single stat %1").arg(i + 1) : ss.label, row);
		labelLbl->setMinimumWidth(120);

		auto *valueSpin = new QSpinBox(row);
		valueSpin->setRange(-9999, 9999);
		valueSpin->setValue(ss.value);
		valueSpin->setMinimumWidth(60);

		auto *minusBtn = makeEmojiBtn(QStringLiteral("➖"), tr("%1 -1").arg(labelLbl->text()), row);
		auto *plusBtn = makeEmojiBtn(QStringLiteral("➕"), tr("%1 +1").arg(labelLbl->text()), row);

		const int h = valueSpin->sizeHint().height();
		minusBtn->setFixedSize(h, h);
		plusBtn->setFixedSize(h, h);

		lay->addWidget(visibleCheck, 0, Qt::AlignVCenter);
		lay->addWidget(labelLbl);
		lay->addStretch(1);
		lay->addWidget(minusBtn, 0, Qt::AlignVCenter);
		lay->addWidget(valueSpin, 0, Qt::AlignVCenter);
		lay->addWidget(plusBtn, 0, Qt::AlignVCenter);

		singleStatsLayout_->addWidget(row);

		ui.row = row;
		ui.visibleCheck = visibleCheck;
		ui.labelLbl = labelLbl;
		ui.valueSpin = valueSpin;
		ui.minusBtn = minusBtn;
		ui.plusBtn = plusBtn;

		auto sync = [this]() {
			syncSingleStatControlsToState();
		};
		connect(visibleCheck, &QCheckBox::toggled, this, [sync](bool) { sync(); });
		connect(valueSpin, qOverload<int>(&QSpinBox::valueChanged), this, [sync](int) { sync(); });
		connect(minusBtn, &QPushButton::clicked, this, [valueSpin, sync]() {
			valueSpin->setValue(valueSpin->value() - 1);
			sync();
		});
		connect(plusBtn, &QPushButton::clicked, this, [valueSpin, sync]() {
			valueSpin->setValue(valueSpin->value() + 1);
			sync();
		});

		singleStats_.push_back(ui);
	}

	singleStatsLayout_->addStretch(1);
}

void FlyScoreDock::syncSingleStatControlsToState()
{
	st_.single_stats.clear();
	st_.single_stats.reserve(singleStats_.size());

	for (const auto &ui : singleStats_) {
		FlySingleStat ss;
		ss.label = ui.labelLbl ? ui.labelLbl->text() : QString();
		ss.value = ui.valueSpin ? ui.valueSpin->value() : 0;
		ss.visible = ui.visibleCheck ? ui.visibleCheck->isChecked() : true;
		st_.single_stats.push_back(ss);
	}

	saveState();
}

// ------------------------------------------------------------
// Timers quick controls (same logic you already had)
// ------------------------------------------------------------

void FlyScoreDock::clearAllTimerRows()
{
	// IMPORTANT:
	// The timers UI is rebuilt frequently (play/pause, reset, dialog changes).
	// If we only remove the row widgets but leave QSpacerItems (stretches)
	// behind, subsequent rebuilds will append new rows *after* the spacer,
	// visually pushing the rows to the bottom. This is exactly the "row jumps
	// to bottom when pressing play/pause" symptom.
	if (timersLayout_) {
		QLayoutItem *it = nullptr;
		while ((it = timersLayout_->takeAt(0)) != nullptr) {
			if (QWidget *w = it->widget())
				w->deleteLater();
			delete it; // also deletes spacer items
		}
	}
	// Also clear our UI bookkeeping.
	timers_.clear();
}

void FlyScoreDock::loadTimerControlsFromState()
{
	clearAllTimerRows();
	if (!timersLayout_)
		return;

	if (st_.timers.isEmpty()) {
		FlyTimer main;
		main.label = QStringLiteral("First Half");
		main.mode = QStringLiteral("countdown");
		main.running = false;
		main.initial_ms = 0;
		main.remaining_ms = 0;
		main.last_tick_ms = 0;
		main.visible = true;
		st_.timers.push_back(main);
	}

	timers_.reserve(st_.timers.size());

	auto makeEmojiBtn = [](const QString &emoji, const QString &tooltip, QWidget *parent) {
		auto *btn = new QPushButton(parent);
		btn->setText(emoji);
		btn->setToolTip(tooltip);
		btn->setCursor(Qt::PointingHandCursor);
		btn->setStyleSheet("QPushButton {"
				   "  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				   "  font-size:12px;"
				   "  padding:0;"
				   "}");
		return btn;
	};

	for (int i = 0; i < st_.timers.size(); ++i) {
		const auto &tm = st_.timers[i];
		FlyTimerUi ui;

		auto *row = new QWidget(this);
		auto *lay = new QHBoxLayout(row);
		lay->setContentsMargins(0, 0, 0, 0);
		lay->setSpacing(6);

		auto *visibleCheck = new QCheckBox(row);
		visibleCheck->setChecked(tm.visible);

		auto *labelLbl = new QLabel(tm.label.isEmpty() ? QStringLiteral("(unnamed)") : tm.label, row);
		labelLbl->setMinimumWidth(120);

		auto *timeEdit = new QLineEdit(row);
		timeEdit->setPlaceholderText(QStringLiteral("mm:ss"));
		timeEdit->setClearButtonEnabled(true);
		timeEdit->setMaxLength(8);
		timeEdit->setMinimumWidth(60);
		timeEdit->setText(fly_format_ms_mmss(tm.remaining_ms));

		auto *startStopBtn =
			makeEmojiBtn(tm.running ? QStringLiteral("⏸️") : QStringLiteral("▶️"),
				     tm.running ? QStringLiteral("Pause timer") : QStringLiteral("Start timer"), row);

		auto *resetBtn = makeEmojiBtn(QStringLiteral("🔄️"), QStringLiteral("Reset timer"), row);

		const int h = timeEdit->sizeHint().height();
		startStopBtn->setFixedSize(h, h);
		resetBtn->setFixedSize(h, h);

		lay->addWidget(visibleCheck, 0, Qt::AlignVCenter);
		lay->addWidget(labelLbl);
		lay->addStretch(1);
		lay->addWidget(timeEdit, 0, Qt::AlignVCenter);
		lay->addWidget(startStopBtn, 0, Qt::AlignVCenter);
		lay->addWidget(resetBtn, 0, Qt::AlignVCenter);

		// Keep rows pinned to top: rows must not expand vertically.
		row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		timersLayout_->addWidget(row);

		ui.row = row;
		ui.labelLbl = labelLbl;
		ui.timeEdit = timeEdit;
		ui.startStop = startStopBtn;
		ui.reset = resetBtn;
		ui.visibleCheck = visibleCheck;

		connect(visibleCheck, &QCheckBox::toggled, this, [this, i](bool on) {
			if (i < 0 || i >= st_.timers.size())
				return;
			st_.timers[i].visible = on;
			saveState();
		});

		connect(timeEdit, &QLineEdit::editingFinished, this, [this, i, timeEdit]() {
			if (i < 0 || i >= st_.timers.size())
				return;

			FlyTimer &t = st_.timers[i];
			if (t.running) {
				timeEdit->setText(fly_format_ms_mmss(t.remaining_ms));
				return;
			}

			qint64 ms = fly_parse_mmss_to_ms(timeEdit->text());
			if (ms < 0) {
				timeEdit->setText(fly_format_ms_mmss(t.remaining_ms));
				return;
			}

			t.initial_ms = ms;
			t.remaining_ms = ms;
			saveState();
			timeEdit->setText(fly_format_ms_mmss(t.remaining_ms));
		});

		connect(startStopBtn, &QPushButton::clicked, this, [this, i]() {
			toggleTimerRunning(i);
		});

		connect(resetBtn, &QPushButton::clicked, this, [this, i]() {
			if (i < 0 || i >= st_.timers.size())
				return;

			FlyTimer &t = st_.timers[i];
			qint64 ms = t.initial_ms;
			if (ms < 0)
				ms = 0;

			t.remaining_ms = ms;
			t.running = false;
			t.last_tick_ms = 0;

			saveState();
			refreshUiFromState(false);
		});

		timers_.push_back(ui);
	}
	timersLayout_->addStretch(1);
}

// ------------------------------------------------------------
// Hotkey-driven actions
// ------------------------------------------------------------

void FlyScoreDock::bumpCustomFieldHome(int index, int delta)
{
	if (index < 0 || index >= st_.custom_fields.size())
		return;

	// go through UI if possible
	if (index < customFields_.size() && customFields_[index].homeSpin) {
		auto *spin = customFields_[index].homeSpin;
		spin->setValue(std::max(0, spin->value() + delta));
		return;
	}

	st_.custom_fields[index].home = std::max(0, st_.custom_fields[index].home + delta);
	saveState();
}

void FlyScoreDock::bumpCustomFieldAway(int index, int delta)
{
	if (index < 0 || index >= st_.custom_fields.size())
		return;

	if (index < customFields_.size() && customFields_[index].awaySpin) {
		auto *spin = customFields_[index].awaySpin;
		spin->setValue(std::max(0, spin->value() + delta));
		return;
	}

	st_.custom_fields[index].away = std::max(0, st_.custom_fields[index].away + delta);
	saveState();
}

void FlyScoreDock::toggleCustomFieldVisible(int index)
{
	if (index < 0 || index >= st_.custom_fields.size())
		return;

	if (index < customFields_.size() && customFields_[index].visibleCheck) {
		auto *chk = customFields_[index].visibleCheck;
		chk->setChecked(!chk->isChecked());
		return;
	}

	st_.custom_fields[index].visible = !st_.custom_fields[index].visible;
	saveState();
}

void FlyScoreDock::bumpSingleStat(int idx, int delta)
{
	if (idx < 0 || idx >= st_.single_stats.size())
		return;

	FlySingleStat &ss = st_.single_stats[idx];
	ss.value += delta;

	saveState();
	loadSingleStatControlsFromState();
}

void FlyScoreDock::toggleSingleStatVisible(int idx)
{
	if (idx < 0 || idx >= st_.single_stats.size())
		return;

	FlySingleStat &ss = st_.single_stats[idx];
	ss.visible = !ss.visible;

	saveState();
	loadSingleStatControlsFromState();
}

void FlyScoreDock::toggleSwap()
{
	if (swapSides_)
		swapSides_->setChecked(!swapSides_->isChecked());
	else {
		st_.swap_sides = !st_.swap_sides;
		saveState();
		refreshUiFromState(false);
	}
}

void FlyScoreDock::toggleScoreboardVisible()
{
	if (showScoreboard_)
		showScoreboard_->setChecked(!showScoreboard_->isChecked());
	else {
		st_.show_scoreboard = !st_.show_scoreboard;
		saveState();
		refreshUiFromState(false);
	}
}

void FlyScoreDock::toggleTimerRunning(int index)
{
	if (index < 0 || index >= st_.timers.size())
		return;

	FlyTimer &tm = st_.timers[index];
	const qint64 now = fly_now_ms();

	if (!tm.running) {
		if (tm.remaining_ms < 0) {
			if (tm.mode == QStringLiteral("countdown"))
				tm.remaining_ms = (tm.initial_ms > 0) ? tm.initial_ms : 0;
			else
				tm.remaining_ms = 0;
		}
		tm.last_tick_ms = now;
		tm.running = true;
	} else {
		if (tm.last_tick_ms > 0) {
			qint64 elapsed = now - tm.last_tick_ms;
			if (elapsed < 0)
				elapsed = 0;

			if (tm.mode == QStringLiteral("countup"))
				tm.remaining_ms += elapsed;
			else {
				tm.remaining_ms -= elapsed;
				if (tm.remaining_ms < 0)
					tm.remaining_ms = 0;
			}
		}
		tm.running = false;
	}

	saveState();
	refreshUiFromState(false);
}

// ------------------------------------------------------------
// Dialogs
// ------------------------------------------------------------

void FlyScoreDock::onOpenCustomFieldsDialog()
{
	FlyFieldsDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	refreshUiFromState(false);

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);
}

void FlyScoreDock::onOpenTimersDialog()
{
	FlyTimersDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	refreshUiFromState(false);

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);
}

void FlyScoreDock::onOpenTeamsDialog()
{
	FlyTeamsDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	refreshUiFromState(false);
}

void FlyScoreDock::ensureResourcesDefaults()
{
	QString resDir = fly_get_data_root_no_ui();
	if (resDir.isEmpty())
		resDir = dataDir_;
	fly_state_ensure_json_exists(resDir, &st_);
}

// ------------------------------------------------------------
// Dock registration with OBS frontend
// ------------------------------------------------------------

static QWidget *g_dockContent = nullptr;


FlyScoreDock::~FlyScoreDock()
{
    if (obsSignalsConnected_ && obsSignalHandler_) {
        auto *sh = static_cast<signal_handler_t *>(obsSignalHandler_);
        signal_handler_disconnect(sh, "source_create", fly_on_source_list_changed, this);
        signal_handler_disconnect(sh, "source_destroy", fly_on_source_list_changed, this);
        obsSignalsConnected_ = false;
        obsSignalHandler_ = nullptr;
    }
}

QString FlyScoreDock::selectedBrowserSourceName() const
{
    if (!browserSourceCombo_ || !browserSourceCombo_->isEnabled())
        return QString::fromUtf8(kBrowserSourceName);

    const QString name = browserSourceCombo_->currentText().trimmed();
    return name.isEmpty() ? QString::fromUtf8(kBrowserSourceName) : name;
}

void FlyScoreDock::refreshBrowserSourceCombo(bool preserveSelection)
{
    if (!browserSourceCombo_)
        return;

    const QString prev = preserveSelection ? browserSourceCombo_->currentText() : QString();

    QSignalBlocker block(browserSourceCombo_);
    browserSourceCombo_->clear();

    const QStringList names = fly_list_browser_sources();
    if (names.isEmpty()) {
        browserSourceCombo_->addItem(tr("No Browser Sources"));
        browserSourceCombo_->setEnabled(false);
        return;
    }

    browserSourceCombo_->setEnabled(true);
    for (const auto &n : names)
        browserSourceCombo_->addItem(n);

    int idx = preserveSelection ? browserSourceCombo_->findText(prev) : -1;
    if (idx < 0)
        idx = browserSourceCombo_->findText(QString::fromUtf8(kBrowserSourceName));
    if (idx < 0)
        idx = 0;

    browserSourceCombo_->setCurrentIndex(idx);
}


void fly_create_dock()
{
	if (g_dockContent)
		return;

	auto *panel = new FlyScoreDock(nullptr);
	panel->init();

#if defined(HAVE_OBS_DOCK_BY_ID)
	obs_frontend_add_dock_by_id(kFlyDockId, kFlyDockTitle, panel);
#else
	obs_frontend_add_dock(panel);
#endif

	g_dockContent = panel;
	LOGI("Fly Scoreboard dock created");
}

void fly_destroy_dock()
{
	if (!g_dockContent)
		return;

#if defined(HAVE_OBS_DOCK_BY_ID)
	obs_frontend_remove_dock(kFlyDockId);
#else
	obs_frontend_remove_dock(g_dockContent);
#endif

	g_dockContent = nullptr;
}

FlyScoreDock *fly_get_dock()
{
	return qobject_cast<FlyScoreDock *>(g_dockContent);
}
