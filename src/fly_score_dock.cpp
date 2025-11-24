#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][dock]"
#include "fly_score_log.hpp"

#include "fly_score_dock.hpp"
#include "fly_score_state.hpp"
#include "fly_score_server.hpp"
#include "fly_score_settings_dialog.hpp"
#include "fly_score_const.hpp"
#include "fly_score_paths.hpp"
#include "fly_score_qt_helpers.hpp"
#include "fly_score_obs_helpers.hpp"
#include "fly_score_logo_helpers.hpp"
#include "fly_score_kofi_widget.hpp"
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
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSpacerItem>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

FlyScoreDock::FlyScoreDock(QWidget *parent) : QWidget(parent)
{
	QSizePolicy sp = sizePolicy();
	sp.setHorizontalPolicy(QSizePolicy::Preferred);
	sp.setVerticalPolicy(QSizePolicy::Expanding);
	setSizePolicy(sp);
}

QVector<FlyHotkeyBinding> FlyScoreDock::buildDefaultHotkeyBindings() const
{
	QVector<FlyHotkeyBinding> v;

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

	for (int i = 0; i < st_.timers.size(); ++i) {
		const auto &tm = st_.timers[i];
		const QString label = tm.label.isEmpty() ? tr("Timer %1").arg(i + 1) : tm.label;
		const QString baseId = QStringLiteral("timer_%1").arg(i);

		v.push_back({baseId + "_toggle", tr("Timer: %1 - Start/Pause").arg(label), QKeySequence()});
	}

	return v;
}

QVector<FlyHotkeyBinding> FlyScoreDock::buildMergedHotkeyBindings() const
{
	QVector<FlyHotkeyBinding> merged = buildDefaultHotkeyBindings();

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

bool FlyScoreDock::init()
{
	dataDir_ = fly_get_data_root();

	loadState();
	ensureResourcesDefaults();

	hotkeyBindings_ = fly_hotkeys_load(dataDir_);

	setObjectName(QStringLiteral("FlyScoreDock"));
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(QStringLiteral(
		"FlyScoreDock { background: rgba(39, 42, 51, 1.0)}"
	));

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

	const QString cardStyle = QStringLiteral(
		"QGroupBox {"
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
		"}"
	);

	// ---------------------------------------------------------------------
	// Scoreboard card
	// ---------------------------------------------------------------------
	auto *scoreBox = new QGroupBox(QStringLiteral("Scoreboard"), content);
	scoreBox->setStyleSheet(cardStyle);

	auto *scoreVBox = new QVBoxLayout(scoreBox);
	scoreVBox->setContentsMargins(8, 8, 8, 8);
	scoreVBox->setSpacing(6);

	swapSides_ = new QCheckBox(QStringLiteral("Swap Home ↔ Guests"), scoreBox);
	showScoreboard_ = new QCheckBox(QStringLiteral("Show scoreboard"), scoreBox);

	teamsBtn_ = new QPushButton(QStringLiteral("Teams"), scoreBox);
	teamsBtn_->setMinimumWidth(110);
	teamsBtn_->setMaximumWidth(130);
	teamsBtn_->setCursor(Qt::PointingHandCursor);

	auto *scoreRow = new QHBoxLayout();
	scoreRow->setContentsMargins(0, 0, 0, 0);
	scoreRow->setSpacing(8);

	scoreRow->addWidget(swapSides_);
	scoreRow->addSpacing(12);
	scoreRow->addWidget(showScoreboard_);
	scoreRow->addStretch(1);
	scoreRow->addWidget(teamsBtn_);

	scoreVBox->addLayout(scoreRow);
	scoreBox->setLayout(scoreVBox);

	// ---------------------------------------------------------------------
	// Match stats card
	// ---------------------------------------------------------------------
	auto *customBox = new QGroupBox(QStringLiteral("Match stats"), content);
	customBox->setStyleSheet(cardStyle);

	auto *customVBox = new QVBoxLayout(customBox);
	customVBox->setContentsMargins(8, 8, 8, 8);
	customVBox->setSpacing(6);

	auto *customHeaderRow = new QHBoxLayout();
	customHeaderRow->setContentsMargins(0, 0, 0, 0);
	customHeaderRow->setSpacing(6);

	editFieldsBtn_ = new QPushButton(QStringLiteral("Configure"), customBox);
	editFieldsBtn_->setMinimumWidth(110);
	editFieldsBtn_->setMaximumWidth(130);
	editFieldsBtn_->setCursor(Qt::PointingHandCursor);

	customHeaderRow->addStretch(1);
	customHeaderRow->addWidget(editFieldsBtn_);

	customFieldsLayout_ = new QVBoxLayout();
	customFieldsLayout_->setContentsMargins(0, 0, 0, 0);
	customFieldsLayout_->setSpacing(4);

	customVBox->addLayout(customHeaderRow);
	customVBox->addLayout(customFieldsLayout_);
	customBox->setLayout(customVBox);

	// ---------------------------------------------------------------------
	// Timers card
	// ---------------------------------------------------------------------
	auto *timersBox = new QGroupBox(QStringLiteral("Timers"), content);
	timersBox->setStyleSheet(cardStyle);

	auto *timersVBox = new QVBoxLayout(timersBox);
	timersVBox->setContentsMargins(8, 8, 8, 8);
	timersVBox->setSpacing(6);

	auto *timersHeaderRow = new QHBoxLayout();
	timersHeaderRow->setContentsMargins(0, 0, 0, 0);
	timersHeaderRow->setSpacing(6);

	editTimersBtn_ = new QPushButton(QStringLiteral("Configure"), timersBox);
	editTimersBtn_->setMinimumWidth(110);
	editTimersBtn_->setMaximumWidth(130);
	editTimersBtn_->setCursor(Qt::PointingHandCursor);

	timersHeaderRow->addStretch(1);
	timersHeaderRow->addWidget(editTimersBtn_);

	timersLayout_ = new QVBoxLayout();
	timersLayout_->setContentsMargins(0, 0, 0, 0);
	timersLayout_->setSpacing(4);

	timersVBox->addLayout(timersHeaderRow);
	timersVBox->addLayout(timersLayout_);
	timersBox->setLayout(timersVBox);

	// ---------------------------------------------------------------------
	// Bottom row buttons (footer outside cards)
	// ---------------------------------------------------------------------
	auto *bottomRow = new QHBoxLayout();
	bottomRow->setContentsMargins(0, 0, 0, 0);
	bottomRow->setSpacing(6);

	auto *refreshBrowserBtn = new QPushButton(QStringLiteral("🔄"), content);
	refreshBrowserBtn->setCursor(Qt::PointingHandCursor);
	refreshBrowserBtn->setToolTip(QStringLiteral("Refresh ‘%1’").arg(kBrowserSourceName));

	auto *addSourceBtn = new QPushButton(QStringLiteral("🌐"), content);
	addSourceBtn->setCursor(Qt::PointingHandCursor);
	addSourceBtn->setToolTip(QStringLiteral("Add scoreboard Browser Source to current scene"));

	auto *clearBtn = new QPushButton(QStringLiteral("🧹"), content);
	clearBtn->setCursor(Qt::PointingHandCursor);
	clearBtn->setToolTip(QStringLiteral("Reset stats and timers (keep teams & logos)"));

	auto *settingsBtn = new QPushButton(QStringLiteral("⚙️"), content);
	settingsBtn->setCursor(Qt::PointingHandCursor);
	settingsBtn->setToolTip(QStringLiteral("Settings"));

	auto *hotkeysBtn = new QPushButton(QStringLiteral("⌨️"), content);
	hotkeysBtn->setCursor(Qt::PointingHandCursor);
	hotkeysBtn->setToolTip(QStringLiteral("Configure Fly Scoreboard hotkeys"));

	bottomRow->addWidget(refreshBrowserBtn);
	bottomRow->addWidget(addSourceBtn);
	bottomRow->addWidget(clearBtn);
	bottomRow->addStretch(1);
	bottomRow->addWidget(settingsBtn);
	bottomRow->addWidget(hotkeysBtn);

	// ---------------------------------------------------------------------
	// Layout assembly (TierDock style)
	// ---------------------------------------------------------------------
	root->addWidget(scoreBox);
	root->addWidget(customBox);
	root->addWidget(timersBox);
	root->addLayout(bottomRow);
	root->addStretch(1);
	root->addWidget(fly_create_widget_carousel(content));

	// ---------------------------------------------------------------------
	// Signals
	// ---------------------------------------------------------------------
	connect(swapSides_, &QCheckBox::toggled, this, [this](bool on) {
		st_.swap_sides = on;
		saveState();
	});
	connect(showScoreboard_, &QCheckBox::toggled, this, [this](bool on) {
		st_.show_scoreboard = on;
		saveState();
	});

	connect(refreshBrowserBtn, &QPushButton::clicked, this, [this]() {
	    const bool ok = fly_refresh_browser_source_by_name(QString::fromUtf8(kBrowserSourceName));
	    ensureResourcesDefaults();
	    if (!ok)
	        LOGW("Refresh failed for Browser Source: %s", kBrowserSourceName);
	});

	connect(addSourceBtn, &QPushButton::clicked, this, [this]() {
#ifdef ENABLE_FRONTEND_API
		obs_source_t *sceneSource = obs_frontend_get_current_scene();
		if (!sceneSource) {
			LOGW("No current scene when trying to add browser source");
			return;
		}

		obs_scene_t *scene = obs_scene_from_source(sceneSource);
		if (!scene) {
			LOGW("Current scene is not a scene");
			obs_source_release(sceneSource);
			return;
		}

		const QString url = QStringLiteral("http://127.0.0.1:%1/").arg(st_.server_port);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "url", url.toUtf8().constData());
		obs_data_set_bool(settings, "is_local_file", false);
		obs_data_set_int(settings, "width", 1920);
		obs_data_set_int(settings, "height", 1080);

		obs_source_t *browser = obs_source_create("browser_source", kBrowserSourceName, settings, nullptr);
		obs_data_release(settings);

		if (!browser) {
			LOGW("Failed to create browser source");
			obs_source_release(sceneSource);
			return;
		}

		obs_sceneitem_t *item = obs_scene_add(scene, browser);
		if (!item)
			LOGW("Failed to add browser source to scene");

		obs_source_release(browser);
		obs_source_release(sceneSource);
#else
		LOGW("Frontend API not enabled; cannot add browser source from dock.");
#endif
	});

	connect(settingsBtn, &QPushButton::clicked, this, [this]() {
		auto *dlg = new FlySettingsDialog(this);
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		dlg->show();
	});

	connect(clearBtn, &QPushButton::clicked, this, &FlyScoreDock::onClearTeamsAndReset);
	connect(editFieldsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenCustomFieldsDialog);
	connect(editTimersBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTimersDialog);
	connect(teamsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTeamsDialog);
	connect(hotkeysBtn, &QPushButton::clicked, this, &FlyScoreDock::openHotkeysDialog);

	refreshUiFromState(false);

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);

	return true;
}

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

	// Scoreboard-level toggles
	if (swapSides_ && swapSides_->isChecked() != st_.swap_sides)
		swapSides_->setChecked(st_.swap_sides);
	if (showScoreboard_ && showScoreboard_->isChecked() != st_.show_scoreboard)
		showScoreboard_->setChecked(st_.show_scoreboard);

	// Custom fields & timers
	loadCustomFieldControlsFromState();
	loadTimerControlsFromState();
}

void FlyScoreDock::onClearTeamsAndReset()
{
	auto rc = QMessageBox::question(this, QStringLiteral("Reset values"),
					QStringLiteral("Reset all match stats and timers to 0?\n"
						       "Teams, logos and field/timer configuration will be kept."));
	if (rc != QMessageBox::Yes)
		return;

	for (auto &cf : st_.custom_fields) {
		cf.home = 0;
		cf.away = 0;
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

	LOGI("Reset match stats and timers (teams, logos and configuration preserved)");
}

static inline int clampi(int v, int lo, int hi)
{
	return std::max(lo, std::min(v, hi));
}

void FlyScoreDock::bumpCustomFieldHome(int index, int delta)
{
	if (index < 0 || index >= st_.custom_fields.size())
		return;

	// Prefer going through the UI spin box so existing connections sync state.
	if (index < customFields_.size() && customFields_[index].homeSpin) {
		auto *spin = customFields_[index].homeSpin;
		spin->setValue(std::max(0, spin->value() + delta));
		return;
	}

	FlyCustomField &cf = st_.custom_fields[index];
	cf.home = std::max(0, cf.home + delta);
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

	FlyCustomField &cf = st_.custom_fields[index];
	cf.away = std::max(0, cf.away + delta);
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

	FlyCustomField &cf = st_.custom_fields[index];
	cf.visible = !cf.visible;
	saveState();
}

void FlyScoreDock::toggleSwap()
{
	if (swapSides_) {
		swapSides_->setChecked(!swapSides_->isChecked());
	} else {
		st_.swap_sides = !st_.swap_sides;
		saveState();
		refreshUiFromState(false);
	}
}

void FlyScoreDock::toggleScoreboardVisible()
{
	if (showScoreboard_) {
		showScoreboard_->setChecked(!showScoreboard_->isChecked());
	} else {
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
			if (tm.mode == QStringLiteral("countdown")) {
				tm.remaining_ms = (tm.initial_ms > 0) ? tm.initial_ms : 0;
			} else {
				tm.remaining_ms = 0;
			}
		}
		tm.last_tick_ms = now;
		tm.running = true;
	} else {
		if (tm.last_tick_ms > 0) {
			qint64 elapsed = now - tm.last_tick_ms;
			if (elapsed < 0)
				elapsed = 0;

			if (tm.mode == QStringLiteral("countup")) {
				tm.remaining_ms += elapsed;
			} else {
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

// -----------------------------------------------------------------------------
// Custom fields quick controls
// -----------------------------------------------------------------------------

void FlyScoreDock::clearAllCustomFieldRows()
{
	customFields_.clear();

	if (!customFieldsLayout_)
		return;

	// Remove *all* items from the layout (header + rows)
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
		statHdr->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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

		hdrRow->setLayout(grid);
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
		visibleCheck->setToolTip(QStringLiteral("Show this field on overlay"));
		grid->addWidget(visibleCheck, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

		auto *labelLbl = new QLabel(cf.label.isEmpty() ? QStringLiteral("(unnamed)") : cf.label, row);
		labelLbl->setMinimumWidth(120);
		grid->addWidget(labelLbl, 0, 1, Qt::AlignVCenter);

		auto makeEmojiBtnRow = [](const QString &emoji, const QString &tooltip, QWidget *parent) {
			auto *btn = new QPushButton(parent);
			btn->setText(emoji);
			btn->setToolTip(tooltip);
			btn->setCursor(Qt::PointingHandCursor);
			btn->setStyleSheet(
				"QPushButton {"
				"  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				"  font-size:12px;"
				"  padding:0;"
				"}");
			return btn;
		};

		auto *homeSpin = new QSpinBox(row);
		homeSpin->setRange(0, 999);
		homeSpin->setValue(std::max(0, cf.home));
		homeSpin->setMaximumWidth(60);
		homeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

		auto *minusHome = makeEmojiBtnRow(QStringLiteral("➖"), QStringLiteral("Home -1"), row);
		auto *plusHome = makeEmojiBtnRow(QStringLiteral("➕"), QStringLiteral("Home +1"), row);

		auto *awaySpin = new QSpinBox(row);
		awaySpin->setRange(0, 999);
		awaySpin->setValue(std::max(0, cf.away));
		awaySpin->setMaximumWidth(60);
		awaySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

		auto *minusAway = makeEmojiBtnRow(QStringLiteral("➖"), QStringLiteral("Guests -1"), row);
		auto *plusAway = makeEmojiBtnRow(QStringLiteral("➕"), QStringLiteral("Guests +1"), row);

		const int h = homeSpin->sizeHint().height();
		const int btnSize = h;

		minusHome->setFixedSize(btnSize, btnSize);
		plusHome->setFixedSize(btnSize, btnSize);
		minusAway->setFixedSize(btnSize, btnSize);
		plusAway->setFixedSize(btnSize, btnSize);

		auto *homeBox = new QWidget(row);
		auto *homeLay = new QHBoxLayout(homeBox);
		homeLay->setContentsMargins(0, 0, 0, 0);
		homeLay->setSpacing(4);
		homeLay->addWidget(minusHome, 0, Qt::AlignVCenter);
		homeLay->addWidget(homeSpin, 0, Qt::AlignVCenter);
		homeLay->addWidget(plusHome, 0, Qt::AlignVCenter);
		homeBox->setLayout(homeLay);

		grid->addWidget(homeBox, 0, 2, Qt::AlignHCenter | Qt::AlignVCenter);

		auto *awayBox = new QWidget(row);
		auto *awayLay = new QHBoxLayout(awayBox);
		awayLay->setContentsMargins(0, 0, 0, 0);
		awayLay->setSpacing(4);
		awayLay->addWidget(minusAway, 0, Qt::AlignVCenter);
		awayLay->addWidget(awaySpin, 0, Qt::AlignVCenter);
		awayLay->addWidget(plusAway, 0, Qt::AlignVCenter);
		awayBox->setLayout(awayLay);

		grid->addWidget(awayBox, 0, 3, Qt::AlignHCenter | Qt::AlignVCenter);

		grid->setColumnStretch(1, 2);
		grid->setColumnStretch(2, 1);
		grid->setColumnStretch(3, 1);

		row->setLayout(grid);
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

// -----------------------------------------------------------------------------
// Timers quick controls
// -----------------------------------------------------------------------------

void FlyScoreDock::clearAllTimerRows()
{
	for (auto &ui : timers_) {
		if (timersLayout_ && ui.row)
			timersLayout_->removeWidget(ui.row);
		if (ui.row)
			ui.row->deleteLater();
	}
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
		st_.timers.push_back(main);
	}

	timers_.reserve(st_.timers.size());

	for (int i = 0; i < st_.timers.size(); ++i) {
		const auto &tm = st_.timers[i];

		FlyTimerUi ui;

		auto *row = new QWidget(this);
		auto *lay = new QHBoxLayout(row);
		lay->setContentsMargins(0, 0, 0, 0);
		lay->setSpacing(6);

		auto *labelLbl = new QLabel(tm.label.isEmpty() ? QStringLiteral("(unnamed)") : tm.label, row);
		labelLbl->setMinimumWidth(120);

		auto *timeEdit = new QLineEdit(row);
		timeEdit->setPlaceholderText(QStringLiteral("mm:ss"));
		timeEdit->setClearButtonEnabled(true);
		timeEdit->setMaxLength(8);
		timeEdit->setMinimumWidth(60);
		timeEdit->setText(fly_format_ms_mmss(tm.remaining_ms));

		auto makeEmojiBtnTimer = [](const QString &emoji, const QString &tooltip, QWidget *parent) {
			auto *btn = new QPushButton(parent);
			btn->setText(emoji);
			btn->setToolTip(tooltip);
			btn->setCursor(Qt::PointingHandCursor);
			btn->setStyleSheet(
				"QPushButton {"
				"  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				"  font-size:12px;"
				"  padding:0;"
				"}");
			return btn;
		};

		auto *startStopBtn = makeEmojiBtnTimer(
			tm.running ? QStringLiteral("⏸️") : QStringLiteral("▶️"),
			tm.running ? QStringLiteral("Pause timer") : QStringLiteral("Start timer"), row);

		auto *resetBtn = makeEmojiBtnTimer(QStringLiteral("🔄️"), QStringLiteral("Reset timer"), row);

		const int h = timeEdit->sizeHint().height();
		const int btnSize = h;

		startStopBtn->setFixedSize(btnSize, btnSize);
		resetBtn->setFixedSize(btnSize, btnSize);

		lay->addWidget(labelLbl);
		lay->addStretch(1);
		lay->addWidget(timeEdit, 0, Qt::AlignVCenter);
		lay->addWidget(startStopBtn, 0, Qt::AlignVCenter);
		lay->addWidget(resetBtn, 0, Qt::AlignVCenter);

		row->setLayout(lay);
		timersLayout_->addWidget(row);

		ui.row = row;
		ui.labelLbl = labelLbl;
		ui.timeEdit = timeEdit;
		ui.startStop = startStopBtn;
		ui.reset = resetBtn;

		connect(timeEdit, &QLineEdit::editingFinished, this, [this, i, timeEdit]() {
			if (i < 0 || i >= st_.timers.size())
				return;

			FlyTimer &tm = st_.timers[i];
			if (tm.running) {
				timeEdit->setText(fly_format_ms_mmss(tm.remaining_ms));
				return;
			}

			qint64 ms = fly_parse_mmss_to_ms(timeEdit->text());
			if (ms < 0) {
				timeEdit->setText(fly_format_ms_mmss(tm.remaining_ms));
				return;
			}

			tm.initial_ms = ms;
			tm.remaining_ms = ms;
			saveState();
			timeEdit->setText(fly_format_ms_mmss(tm.remaining_ms));
		});

		connect(startStopBtn, &QPushButton::clicked, this, [this, i]() {
			if (i < 0 || i >= st_.timers.size())
				return;

			FlyTimer &tm = st_.timers[i];
			const qint64 now = fly_now_ms();

			if (!tm.running) {
				if (tm.remaining_ms < 0) {
					if (tm.mode == QStringLiteral("countdown")) {
						tm.remaining_ms = (tm.initial_ms > 0) ? tm.initial_ms : 0;
					} else {
						tm.remaining_ms = 0;
					}
				}
				tm.last_tick_ms = now;
				tm.running = true;
			} else {
				if (tm.last_tick_ms > 0) {
					qint64 elapsed = now - tm.last_tick_ms;
					if (elapsed < 0)
						elapsed = 0;

					if (tm.mode == QStringLiteral("countup")) {
						tm.remaining_ms += elapsed;
					} else {
						tm.remaining_ms -= elapsed;
						if (tm.remaining_ms < 0)
							tm.remaining_ms = 0;
					}
				}
				tm.running = false;
			}

			saveState();
			refreshUiFromState(false);
		});

		connect(resetBtn, &QPushButton::clicked, this, [this, i]() {
			if (i < 0 || i >= st_.timers.size())
				return;

			FlyTimer &tm = st_.timers[i];

			qint64 ms = tm.initial_ms;
			if (ms < 0)
				ms = 0;

			tm.remaining_ms = ms;
			tm.running = false;
			tm.last_tick_ms = 0;

			saveState();
			refreshUiFromState(false);
		});

		timers_.push_back(ui);
	}
}

// -----------------------------------------------------------------------------
// Hotkeys - dialog-driven, plugin-local (QShortcut)
// -----------------------------------------------------------------------------

void FlyScoreDock::clearAllShortcuts()
{
	for (auto *sc : shortcuts_) {
		if (sc)
			sc->deleteLater();
	}
	shortcuts_.clear();
}

void FlyScoreDock::applyHotkeyBindings(const QVector<FlyHotkeyBinding> &bindings)
{
	clearAllShortcuts();
	hotkeyBindings_ = bindings;

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

				if (side == QLatin1String("home") && dir == QLatin1String("inc")) {
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldHome(idx, +1); });
				} else if (side == QLatin1String("home") && dir == QLatin1String("dec")) {
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldHome(idx, -1); });
				} else if (side == QLatin1String("away") && dir == QLatin1String("inc")) {
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldAway(idx, +1); });
				} else if (side == QLatin1String("away") && dir == QLatin1String("dec")) {
					connect(sc, &QShortcut::activated, this,
						[this, idx]() { bumpCustomFieldAway(idx, -1); });
				}
			}
		} else if (id.startsWith(QLatin1String("timer_"))) {
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
	// Always rebuild from current state so we get all custom fields & timers.
	QVector<FlyHotkeyBinding> initial = buildMergedHotkeyBindings();

	auto *dlg = new FlyHotkeysDialog(initial, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(dlg, &FlyHotkeysDialog::bindingsChanged, this, [this](const QVector<FlyHotkeyBinding> &b) {
		applyHotkeyBindings(b);
		// TODO: persist `b` into FlyState if you want them across restarts.
	});

	dlg->show();
}

// -----------------------------------------------------------------------------
// Dialogs
// -----------------------------------------------------------------------------

void FlyScoreDock::onOpenCustomFieldsDialog()
{
	FlyFieldsDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	refreshUiFromState(false);

	// Rebuild dynamic hotkeys because custom fields changed
	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);
}

void FlyScoreDock::onOpenTimersDialog()
{
	FlyTimersDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	refreshUiFromState(false);

	// Rebuild dynamic hotkeys because timers changed
	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);
}

void FlyScoreDock::onOpenTeamsDialog()
{
	FlyTeamsDialog dlg(dataDir_, st_, this);
	dlg.exec();

	// Reload state from disk in case other fields were touched
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

// -----------------------------------------------------------------------------
// Dock registration with OBS frontend
// -----------------------------------------------------------------------------

static QWidget *g_dockContent = nullptr;

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
	LOGI("Tier dock created");
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
