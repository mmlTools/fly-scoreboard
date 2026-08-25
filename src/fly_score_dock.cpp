#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][dock]"
#include "fly_score_log.hpp"
#include "widget.hpp"

#include "fly_score_dock.hpp"
#include "fly_score_state.hpp"
#include "fly_score_const.hpp"
#include "fly_score_i18n.hpp"
#include "fly_score_paths.hpp"
#include "fly_score_qt_helpers.hpp"
#include "fly_score_logo_helpers.hpp"
#include "fly_score_teams_dialog.hpp"
#include "fly_score_fields_dialog.hpp"
#include "fly_score_timers_dialog.hpp"
#include "fly_score_hotkeys_dialog.hpp"
#include "fly_score_websocket_server.hpp"

#ifdef ENABLE_EMBEDDED_DEFAULTS
#include "embedded_assets.hpp"
#endif

#include <obs.h>
#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QAbstractButton>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QDesktopServices>
#include <QMetaObject>
#include <QShortcut>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextStream>
#include <QSpacerItem>
#include <algorithm>
#include <limits>
#include <QToolButton>
#include <QUrl>

static inline QString fly_settings_org_name()
{
	return QStringLiteral("MMLTech");
}
static inline QString fly_settings_app_name()
{
	return QStringLiteral("fly-scoreboard");
}
static inline QString fly_settings_key_websocket_port()
{
	return QStringLiteral("websocket/port");
}

static inline QString fly_settings_key_event_timestamp_mode()
{
	return QStringLiteral("events/timestamp_mode");
}

static quint16 fly_load_websocket_port()
{
	QSettings s(fly_settings_org_name(), fly_settings_app_name());
	const int p = s.value(fly_settings_key_websocket_port(), 4457).toInt();
	return static_cast<quint16>((p > 0 && p <= 65535) ? p : 4457);
}

static void updateWidgetCarouselToggleUi(QPushButton *btn, QWidget *carousel, QStyle *st)
{
	if (!btn || !carousel || !st)
		return;

	const bool visible = carousel->isVisible();

	btn->blockSignals(true);
	btn->setChecked(visible);
	btn->setToolTip(visible ? fly_i18n("Dock.HideWidgetCarousel") : fly_i18n("Dock.ShowWidgetCarousel"));

	QIcon ico = visible ? QIcon::fromTheme(QStringLiteral("pan-down-symbolic"))
			    : QIcon::fromTheme(QStringLiteral("pan-end-symbolic"));

	if (ico.isNull()) {
		ico = st->standardIcon(visible ? QStyle::SP_TitleBarShadeButton : QStyle::SP_TitleBarUnshadeButton);
	}

	btn->setIcon(ico);
	btn->blockSignals(false);
}

#ifdef ENABLE_FRONTEND_API
static void fly_on_frontend_event(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<FlyScoreDock *>(data);
	if (!self)
		return;
	QMetaObject::invokeMethod(
		self, [self, event]() { self->handleFrontendEvent(static_cast<int>(event)); }, Qt::QueuedConnection);
}
#endif

FlyScoreDock::FlyScoreDock(QWidget *parent) : QWidget(parent)
{
	QSizePolicy sp = sizePolicy();
	sp.setHorizontalPolicy(QSizePolicy::Preferred);
	sp.setVerticalPolicy(QSizePolicy::Expanding);
	setSizePolicy(sp);
}

QList<FlyHotkeyBinding> FlyScoreDock::buildDefaultHotkeyBindings() const
{
	QList<FlyHotkeyBinding> v;

	v.push_back({"swap_sides", fly_i18n("Hotkey.SwapSides"), QKeySequence()});
	v.push_back({"toggle_scoreboard", fly_i18n("Hotkey.ToggleScoreboard"), QKeySequence()});

	for (int i = 0; i < st_.custom_fields.size(); ++i) {
		const auto &cf = st_.custom_fields[i];
		const QString label = cf.label.isEmpty() ? fly_i18n("Hotkey.CustomFieldN").arg(i + 1) : cf.label;
		const QString baseId = QStringLiteral("field_%1").arg(i);

		v.push_back({baseId + "_toggle", fly_i18n("Hotkey.CustomToggle").arg(label), QKeySequence()});
		v.push_back({baseId + "_home_inc", fly_i18n("Hotkey.CustomHomeInc").arg(label), QKeySequence()});
		v.push_back({baseId + "_home_dec", fly_i18n("Hotkey.CustomHomeDec").arg(label), QKeySequence()});
		v.push_back({baseId + "_away_inc", fly_i18n("Hotkey.CustomGuestsInc").arg(label), QKeySequence()});
		v.push_back({baseId + "_away_dec", fly_i18n("Hotkey.CustomGuestsDec").arg(label), QKeySequence()});
	}

	for (int i = 0; i < st_.single_stats.size(); ++i) {
		const auto &ss = st_.single_stats[i];
		const QString label = ss.label.isEmpty() ? fly_i18n("Hotkey.SingleStatN").arg(i + 1) : ss.label;
		const QString baseId = QStringLiteral("single_%1").arg(i);

		v.push_back({baseId + "_toggle", fly_i18n("Hotkey.SingleToggle").arg(label), QKeySequence()});
		v.push_back({baseId + "_inc", fly_i18n("Hotkey.SingleInc").arg(label), QKeySequence()});
		v.push_back({baseId + "_dec", fly_i18n("Hotkey.SingleDec").arg(label), QKeySequence()});
	}

	for (int i = 0; i < st_.timers.size(); ++i) {
		const auto &tm = st_.timers[i];
		const QString label = tm.label.isEmpty() ? fly_i18n("Hotkey.TimerN").arg(i + 1) : tm.label;
		const QString baseId = QStringLiteral("timer_%1").arg(i);

		v.push_back({baseId + "_toggle", fly_i18n("Hotkey.TimerToggle").arg(label), QKeySequence()});
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
	QList<FlyHotkeyBinding> initial = buildMergedHotkeyBindings();

	auto *dlg = new FlyHotkeysDialog(initial, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(dlg, &FlyHotkeysDialog::bindingsChanged, this,
		[this](const QList<FlyHotkeyBinding> &b) { applyHotkeyBindings(b); });

	dlg->show();
}

bool FlyScoreDock::init()
{
	dataDir_ = fly_get_data_root();

	loadState();
	lastLoggedState_ = st_;
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

	auto *mainBox = new QGroupBox(fly_i18n("Common.Scoreboard"), content);
	mainBox->setStyleSheet(cardStyle);

	auto *mainVBox = new QVBoxLayout(mainBox);
	mainVBox->setContentsMargins(8, 8, 8, 8);
	mainVBox->setSpacing(6);

	{
		auto *headerRow = new QHBoxLayout();
		headerRow->setContentsMargins(0, 0, 0, 0);
		headerRow->setSpacing(6);

		teamsBtn_ = new QPushButton(fly_i18n("Dock.Teams"), mainBox);
		teamsBtn_->setMinimumWidth(110);
		teamsBtn_->setMaximumWidth(130);
		teamsBtn_->setCursor(Qt::PointingHandCursor);

		editFieldsBtn_ = new QPushButton(fly_i18n("Dock.StatsButton"), mainBox);
		editFieldsBtn_->setMinimumWidth(110);
		editFieldsBtn_->setMaximumWidth(130);
		editFieldsBtn_->setCursor(Qt::PointingHandCursor);
		editFieldsBtn_->setToolTip(fly_i18n("Dock.ConfigureMatchStats"));

		editTimersBtn_ = new QPushButton(fly_i18n("Dock.TimersButton"), mainBox);
		editTimersBtn_->setMinimumWidth(110);
		editTimersBtn_->setMaximumWidth(130);
		editTimersBtn_->setCursor(Qt::PointingHandCursor);
		editTimersBtn_->setToolTip(fly_i18n("Dock.ConfigureTimers"));

		headerRow->addWidget(teamsBtn_);
		headerRow->addWidget(editFieldsBtn_);
		headerRow->addWidget(editTimersBtn_);
		headerRow->addStretch(1);

		mainVBox->addLayout(headerRow);
	}

	{
		auto *togglesRow = new QHBoxLayout();
		togglesRow->setContentsMargins(0, 0, 0, 0);
		togglesRow->setSpacing(10);

		swapSides_ = new QCheckBox(fly_i18n("Hotkey.SwapSides"), mainBox);
		showScoreboard_ = new QCheckBox(fly_i18n("Dock.ShowScoreboard"), mainBox);

		togglesRow->addWidget(swapSides_);
		togglesRow->addWidget(showScoreboard_);
		togglesRow->addStretch(1);

		mainVBox->addLayout(togglesRow);
	}

	{
		auto *tabs = new QTabWidget(mainBox);
		tabs->setObjectName(QStringLiteral("flyScoreTabs"));
		tabs->setDocumentMode(true);
		tabs->setMovable(false);
		tabs->setUsesScrollButtons(true);

		auto *teamTab = new QWidget(tabs);
		auto *teamVBox = new QVBoxLayout(teamTab);
		teamVBox->setContentsMargins(0, 0, 0, 0);
		teamVBox->setSpacing(4);
		teamVBox->setAlignment(Qt::AlignTop);
		customFieldsLayout_ = teamVBox;

		auto *singleTab = new QWidget(tabs);
		auto *singleVBox = new QVBoxLayout(singleTab);
		singleVBox->setContentsMargins(0, 0, 0, 0);
		singleVBox->setSpacing(4);
		singleVBox->setAlignment(Qt::AlignTop);
		singleStatsLayout_ = singleVBox;

		auto *timersTab = new QWidget(tabs);
		auto *timersVBox = new QVBoxLayout(timersTab);
		timersVBox->setContentsMargins(0, 0, 0, 0);
		timersVBox->setSpacing(4);
		timersVBox->setAlignment(Qt::AlignTop);
		timersLayout_ = timersVBox;

		auto *eventsTab = new QWidget(tabs);
		auto *eventsVBox = new QVBoxLayout(eventsTab);
		eventsVBox->setContentsMargins(6, 6, 6, 6);
		eventsVBox->setSpacing(6);

		auto *eventsTop = new QHBoxLayout();
		eventLogStatus_ = new QLabel(eventsTab);
		eventTimestampMode_ = new QComboBox(eventsTab);
		eventTimestampMode_->addItem(fly_i18n("Events.RelativeTime"), QStringLiteral("relative"));
		eventTimestampMode_->addItem(fly_i18n("Events.WallClock"), QStringLiteral("clock"));
		QSettings eventSettings(fly_settings_org_name(), fly_settings_app_name());
		const QString savedMode =
			eventSettings.value(fly_settings_key_event_timestamp_mode(), QStringLiteral("relative"))
				.toString();
		eventTimestampMode_->setCurrentIndex(savedMode == QLatin1String("clock") ? 1 : 0);
		eventLog_.setTimestampMode(savedMode == QLatin1String("clock")
						   ? FlyScoreEventLog::TimestampMode::WallClock
						   : FlyScoreEventLog::TimestampMode::Relative);
		eventLogToggle_ = new QPushButton(eventsTab);
		auto *eventsFolder = new QPushButton(fly_i18n("Events.OpenFolder"), eventsTab);
		connect(eventsFolder, &QPushButton::clicked, this, [this]() {
			QDir().mkpath(eventLogsDirectory());
			QDesktopServices::openUrl(QUrl::fromLocalFile(eventLogsDirectory()));
		});
		eventsTop->addWidget(eventLogStatus_, 1);
		eventsTop->addWidget(eventTimestampMode_);
		eventsTop->addWidget(eventLogToggle_);
		eventsTop->addWidget(eventsFolder);

		auto *eventsEntry = new QHBoxLayout();
		eventText_ = new QLineEdit(eventsTab);
		eventText_->setPlaceholderText(fly_i18n("Events.Placeholder"));
		eventAdd_ = new QPushButton(fly_i18n("Events.Add"), eventsTab);
		eventsEntry->addWidget(eventText_, 1);
		eventsEntry->addWidget(eventAdd_);

		auto *eventsHint = new QLabel(fly_i18n("Events.Hint"), eventsTab);
		eventsHint->setWordWrap(true);
		eventsVBox->addLayout(eventsTop);
		eventsVBox->addLayout(eventsEntry);
		eventsVBox->addWidget(eventsHint);
		eventsVBox->addStretch(1);

		tabs->addTab(teamTab, fly_i18n("Common.TeamStats"));
		tabs->addTab(singleTab, fly_i18n("Common.SingleStats"));
		tabs->addTab(timersTab, fly_i18n("Common.Timers"));
		tabs->addTab(eventsTab, fly_i18n("Common.Events"));

		mainVBox->addWidget(tabs);
	}

	mainBox->setLayout(mainVBox);
	root->addWidget(mainBox);

	auto *templateRow = new QHBoxLayout();
	templateRow->setContentsMargins(0, 0, 0, 0);
	templateRow->setSpacing(6);

	selectTemplateFolderBtn_ = new QPushButton(fly_i18n("Dock.SelectTemplateFolder"), content);
	selectTemplateFolderBtn_->setCursor(Qt::PointingHandCursor);
	selectTemplateFolderBtn_->setToolTip(fly_i18n("Dock.SelectTemplateFolderTooltip"));
	activeTemplateLabel_ = new QLabel(content);
	activeTemplateLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	activeTemplateLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	auto *openOverlayFolderBtn = new QPushButton(fly_i18n("Dock.OpenOverlayFolder"), content);
	openOverlayFolderBtn->setCursor(Qt::PointingHandCursor);
	openOverlayFolderBtn->setToolTip(fly_i18n("Dock.OpenOverlayFolderTooltip"));

	webSocketStatus_ = new QLabel(content);
	webSocketStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	webSocketStatus_->setToolTip(fly_i18n("Dock.WebSocketTooltip"));

	templateRow->addWidget(selectTemplateFolderBtn_);
	templateRow->addWidget(activeTemplateLabel_, 1);
	templateRow->addWidget(openOverlayFolderBtn);
	templateRow->addWidget(webSocketStatus_);
	root->addLayout(templateRow);

	auto *bottomRow = new QHBoxLayout();
	bottomRow->setContentsMargins(0, 0, 0, 0);
	bottomRow->setSpacing(6);

	auto *clearBtn = new QPushButton(QStringLiteral("🧹"), content);
	clearBtn->setCursor(Qt::PointingHandCursor);
	clearBtn->setToolTip(fly_i18n("Dock.ResetTooltip"));

	auto *hotkeysBtn = new QPushButton(QStringLiteral("⌨️"), content);
	hotkeysBtn->setCursor(Qt::PointingHandCursor);
	hotkeysBtn->setToolTip(fly_i18n("Dock.ConfigureHotkeys"));

	toggleCarouselBtn_ = new QPushButton(content);
	toggleCarouselBtn_->setCheckable(true);
	toggleCarouselBtn_->setCursor(Qt::PointingHandCursor);

	bottomRow->addWidget(clearBtn);
	bottomRow->addStretch(1);
	bottomRow->addWidget(toggleCarouselBtn_);
	bottomRow->addWidget(hotkeysBtn);

	root->addLayout(bottomRow);
	root->addStretch(1);

	widgetCarousel_ = create_widget_carousel(this);
	root->addWidget(widgetCarousel_);

	refreshActiveTemplateLabel();

	connect(swapSides_, &QCheckBox::toggled, this, [this](bool on) {
		st_.swap_sides = on;
		saveState();
	});

	connect(showScoreboard_, &QCheckBox::toggled, this, [this](bool on) {
		st_.show_scoreboard = on;
		saveState();
	});

	connect(clearBtn, &QPushButton::clicked, this, &FlyScoreDock::onClearTeamsAndReset);

	connect(selectTemplateFolderBtn_, &QPushButton::clicked, this, &FlyScoreDock::onSelectTemplateFolder);
	connect(openOverlayFolderBtn, &QPushButton::clicked, this, [this]() {
		const QString path = selectedTemplatePath();
		if (!path.isEmpty() && QDir(path).exists())
			QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	});

	connect(editFieldsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenCustomFieldsDialog);
	connect(editTimersBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTimersDialog);
	connect(teamsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTeamsDialog);

	connect(hotkeysBtn, &QPushButton::clicked, this, &FlyScoreDock::openHotkeysDialog);
	connect(toggleCarouselBtn_, &QPushButton::clicked, this, &FlyScoreDock::toggleWidgetCarouselVisible);
	connect(eventLogToggle_, &QPushButton::clicked, this, [this]() {
		if (eventLog_.isActive())
			stopEventLog();
		else
			startEventLog();
	});
	connect(eventAdd_, &QPushButton::clicked, this, [this]() {
		if (!eventText_)
			return;
		appendEvent(eventText_->text());
		eventText_->clear();
	});
	connect(eventText_, &QLineEdit::returnPressed, eventAdd_, &QPushButton::click);
	connect(eventTimestampMode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		const bool wallClock = index == 1;
		eventLog_.setTimestampMode(wallClock ? FlyScoreEventLog::TimestampMode::WallClock
						     : FlyScoreEventLog::TimestampMode::Relative);
		QSettings settings(fly_settings_org_name(), fly_settings_app_name());
		settings.setValue(fly_settings_key_event_timestamp_mode(),
				  wallClock ? QStringLiteral("clock") : QStringLiteral("relative"));
	});
	webSocketServer_ = new FlyScoreWebSocketServer(this);
	connect(webSocketServer_, &FlyScoreWebSocketServer::commandReceived, this, &FlyScoreDock::handleRemoteCommand);
	connect(webSocketServer_, &FlyScoreWebSocketServer::statusChanged, this, &FlyScoreDock::updateWebSocketStatus);
	webSocketServer_->start(fly_load_websocket_port());
	updateWebSocketStatus();
	refreshEventLogUi();

#ifdef ENABLE_FRONTEND_API
	obs_frontend_add_event_callback(fly_on_frontend_event, this);
	frontendEventCallbackConnected_ = true;
	if (obs_frontend_streaming_active())
		startEventLog(QStringLiteral("Stream Start"));
	else if (obs_frontend_recording_active())
		startEventLog(QStringLiteral("Recording Start"));
#endif

	refreshUiFromState(false);
	refreshWidgetCarouselToggleUi();
	broadcastCurrentState();

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);

	return true;
}

void FlyScoreDock::refreshWidgetCarouselToggleUi()
{
	updateWidgetCarouselToggleUi(toggleCarouselBtn_, widgetCarousel_, style());
}

void FlyScoreDock::toggleWidgetCarouselVisible()
{
	if (!widgetCarousel_)
		return;

	widgetCarousel_->setVisible(!widgetCarousel_->isVisible());
	refreshWidgetCarouselToggleUi();
}

namespace {
struct FlyThemeManifest {
	QString title;
	QString author;
	QString authorUrl;
	QString description;
	QString version;
};

struct FlyThemeInfo {
	FlyThemeManifest manifest;
	QString path;
	QString folderName;
	QString error;
	bool hasIndex = false;
	bool hasManifest = false;
	bool valid = false;
};

static QString fly_theme_manifest_path(const QString &themePath)
{
	return QDir(themePath).filePath(QStringLiteral("manifest.ini"));
}

static QString fly_normalize_manifest_key(QString key, const QString &section)
{
	key = key.trimmed().toLower();
	if (!section.trimmed().isEmpty())
		key = section.trimmed().toLower() + QStringLiteral("/") + key;
	return key;
}

static void fly_apply_manifest_value(FlyThemeManifest &out, const QString &key, const QString &value)
{
	const QString k = key.trimmed().toLower();
	const QString v = value.trimmed();
	if ((k == QLatin1String("title") || k == QLatin1String("theme/title")) && out.title.isEmpty())
		out.title = v;
	else if ((k == QLatin1String("author") || k == QLatin1String("theme/author")) && out.author.isEmpty())
		out.author = v;
	else if ((k == QLatin1String("author_url") || k == QLatin1String("theme/author_url")) &&
		 out.authorUrl.isEmpty())
		out.authorUrl = v;
	else if ((k == QLatin1String("description") || k == QLatin1String("theme/description")) &&
		 out.description.isEmpty())
		out.description = v;
	else if ((k == QLatin1String("version") || k == QLatin1String("theme/version")) && out.version.isEmpty())
		out.version = v;
}

static void fly_read_theme_manifest_text(const QString &manifestPath, FlyThemeManifest &out)
{
	QFile file(manifestPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	QTextStream stream(&file);
	QString section;
	while (!stream.atEnd()) {
		QString line = stream.readLine().trimmed();
		if (line.startsWith(QChar(0xFEFF)))
			line.remove(0, 1);
		if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')))
			continue;
		if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
			section = line.mid(1, line.size() - 2).trimmed();
			continue;
		}

		int pos = line.indexOf(QLatin1Char('='));
		if (pos < 0)
			pos = line.indexOf(QLatin1Char(':'));
		if (pos < 0)
			continue;

		QString value = line.mid(pos + 1).trimmed();
		if ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) ||
		    (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))
			value = value.mid(1, value.size() - 2).trimmed();

		fly_apply_manifest_value(out, fly_normalize_manifest_key(line.left(pos), section), value);
	}
}

static bool fly_read_theme_manifest(const QString &themePath, FlyThemeManifest &out)
{
	const QString manifestPath = fly_theme_manifest_path(themePath);
	if (!QFileInfo::exists(manifestPath))
		return false;

	QSettings manifest(manifestPath, QSettings::IniFormat);
	out.title = manifest.value(QStringLiteral("title")).toString().trimmed();
	out.author = manifest.value(QStringLiteral("author")).toString().trimmed();
	out.authorUrl = manifest.value(QStringLiteral("author_url")).toString().trimmed();
	out.description = manifest.value(QStringLiteral("description")).toString().trimmed();
	out.version = manifest.value(QStringLiteral("version")).toString().trimmed();

	if (out.title.isEmpty())
		out.title = manifest.value(QStringLiteral("theme/title")).toString().trimmed();
	if (out.author.isEmpty())
		out.author = manifest.value(QStringLiteral("theme/author")).toString().trimmed();
	if (out.authorUrl.isEmpty())
		out.authorUrl = manifest.value(QStringLiteral("theme/author_url")).toString().trimmed();
	if (out.description.isEmpty())
		out.description = manifest.value(QStringLiteral("theme/description")).toString().trimmed();
	if (out.version.isEmpty())
		out.version = manifest.value(QStringLiteral("theme/version")).toString().trimmed();
	if (out.title.isEmpty())
		out.title = manifest.value(QStringLiteral("Theme/title")).toString().trimmed();
	if (out.author.isEmpty())
		out.author = manifest.value(QStringLiteral("Theme/author")).toString().trimmed();
	if (out.authorUrl.isEmpty())
		out.authorUrl = manifest.value(QStringLiteral("Theme/author_url")).toString().trimmed();
	if (out.description.isEmpty())
		out.description = manifest.value(QStringLiteral("Theme/description")).toString().trimmed();
	if (out.version.isEmpty())
		out.version = manifest.value(QStringLiteral("Theme/version")).toString().trimmed();

	fly_read_theme_manifest_text(manifestPath, out);

	return !out.title.isEmpty() && !out.author.isEmpty() && !out.authorUrl.isEmpty() &&
	       !out.description.isEmpty() && !out.version.isEmpty();
}

static FlyThemeInfo fly_read_theme_info(const QString &themePath)
{
	FlyThemeInfo info;
	const QFileInfo themeDirInfo(themePath);
	info.path = QDir(themePath).absolutePath();
	info.folderName = themeDirInfo.fileName();
	info.hasIndex = QFileInfo::exists(QDir(themePath).filePath(QStringLiteral("index.html")));
	info.hasManifest = QFileInfo::exists(fly_theme_manifest_path(themePath));

	const bool manifestOk = fly_read_theme_manifest(themePath, info.manifest);
	info.valid = info.hasIndex && manifestOk;

	if (!info.hasIndex)
		info.error = fly_i18n("Dock.TemplateMissingIndex");
	else if (!info.hasManifest)
		info.error = fly_i18n("Dock.TemplateMissingManifest");
	else if (!manifestOk)
		info.error = fly_i18n("Dock.TemplateInvalidManifest");

	if (info.manifest.title.isEmpty())
		info.manifest.title = info.folderName.isEmpty() ? info.path : info.folderName;

	return info;
}

static bool fly_is_valid_theme_folder(const QString &themePath, FlyThemeManifest *manifest = nullptr)
{
	FlyThemeInfo info = fly_read_theme_info(themePath);
	if (!info.valid)
		return false;

	if (manifest)
		*manifest = info.manifest;
	return true;
}

} // namespace

void FlyScoreDock::onSelectTemplateFolder()
{
	const QString cur = dataDir_.isEmpty() ? fly_get_data_root_no_ui() : dataDir_;
	const QString picked = QFileDialog::getExistingDirectory(this, fly_i18n("Dock.SelectTemplateFolderTitle"), cur);
	if (picked.isEmpty())
		return;

	activateTemplateFolder(picked);
}

QString FlyScoreDock::selectedTemplateName() const
{
	FlyThemeManifest manifest;
	if (fly_is_valid_theme_folder(dataDir_, &manifest))
		return manifest.title;
	return QFileInfo(dataDir_).fileName();
}

QString FlyScoreDock::selectedTemplatePath() const
{
	return dataDir_;
}

void FlyScoreDock::refreshActiveTemplateLabel()
{
	if (!activeTemplateLabel_)
		return;

	const QString title = selectedTemplateName();
	activeTemplateLabel_->setText(title.isEmpty() ? fly_i18n("Dock.NoTemplateFolder") : title);
	activeTemplateLabel_->setToolTip(dataDir_);
}

void FlyScoreDock::activateTemplateFolder(const QString &path)
{
	if (path.isEmpty())
		return;

	FlyThemeInfo info = fly_read_theme_info(path);
	if (!info.valid) {
		LOGW("Invalid theme selected: path='%s', hasIndex=%d, hasManifest=%d, reason='%s'",
		     info.path.toUtf8().constData(), info.hasIndex ? 1 : 0, info.hasManifest ? 1 : 0,
		     info.error.toUtf8().constData());
		QMessageBox::warning(this, fly_i18n("Dock.TemplateInvalidTitle"),
				     fly_i18n("Dock.TemplateInvalidMessage"));
		return;
	}

	fly_set_data_root(path);
	dataDir_ = fly_get_data_root_no_ui();
	fly_state_ensure_json_exists(dataDir_, &st_);
	loadState();
	lastLoggedState_ = st_;
	if (eventLog_.isActive())
		eventLog_.appendEvent(QStringLiteral("Template folder activated: %1").arg(info.manifest.title));
	refreshUiFromState(false);
	refreshActiveTemplateLabel();
	broadcastCurrentState();
}

void FlyScoreDock::broadcastCurrentState()
{
	if (!webSocketServer_ || !webSocketServer_->isListening())
		return;
	webSocketServer_->broadcastState(st_, selectedTemplateName(), selectedTemplatePath());
}

void FlyScoreDock::updateWebSocketStatus()
{
	if (!webSocketStatus_ || !webSocketServer_)
		return;

	const QString text =
		webSocketServer_->isListening()
			? fly_i18n("Dock.WSOnline").arg(webSocketServer_->url()).arg(webSocketServer_->clientCount())
			: fly_i18n("Dock.WSOffline");
	webSocketStatus_->setText(text);
}

QString FlyScoreDock::eventLogsDirectory() const
{
	return QDir(fly_data_dir()).filePath(QStringLiteral("event-logs"));
}

void FlyScoreDock::refreshEventLogUi()
{
	if (!eventLogStatus_ || !eventLogToggle_)
		return;

	if (eventLog_.isActive()) {
		const QFileInfo info(eventLog_.filePath());
		eventLogStatus_->setText(fly_i18n("Events.Recording").arg(info.fileName()));
		eventLogStatus_->setToolTip(eventLog_.filePath());
		eventLogToggle_->setText(fly_i18n("Events.Stop"));
	} else {
		eventLogStatus_->setText(fly_i18n("Events.NotRecording"));
		eventLogStatus_->setToolTip(eventLog_.filePath());
		eventLogToggle_->setText(fly_i18n("Events.Start"));
	}
	if (eventTimestampMode_)
		eventTimestampMode_->setEnabled(!eventLog_.isActive());
}

void FlyScoreDock::startEventLog(const QString &firstEvent)
{
	if (eventLog_.isActive())
		return;

	lastLoggedState_ = st_;
	if (!eventLog_.startSession(eventLogsDirectory(), firstEvent)) {
		if (eventLogStatus_)
			eventLogStatus_->setText(fly_i18n("Events.WriteFailed"));
		return;
	}
	refreshEventLogUi();
}

void FlyScoreDock::stopEventLog(const QString &lastEvent)
{
	if (!eventLog_.isActive())
		return;
	if (!lastEvent.trimmed().isEmpty())
		eventLog_.appendEvent(lastEvent);
	eventLog_.stopSession();
	refreshEventLogUi();
}

void FlyScoreDock::appendEvent(const QString &event)
{
	const QString clean = event.simplified();
	if (clean.isEmpty())
		return;
	if (!eventLog_.isActive())
		startEventLog(QStringLiteral("Log Start"));
	if (eventLog_.isActive())
		eventLog_.appendEvent(clean);
	refreshEventLogUi();
}

void FlyScoreDock::handleFrontendEvent(int event)
{
#ifdef ENABLE_FRONTEND_API
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		if (eventLog_.isActive())
			eventLog_.appendEvent(QStringLiteral("Stream Start"));
		else
			startEventLog(QStringLiteral("Stream Start"));
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		if (eventLog_.isActive())
			eventLog_.appendEvent(QStringLiteral("Recording Start"));
		else
			startEventLog(QStringLiteral("Recording Start"));
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		if (eventLog_.isActive()) {
			eventLog_.appendEvent(QStringLiteral("Stream End"));
			if (!obs_frontend_recording_active())
				eventLog_.stopSession();
		}
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		if (eventLog_.isActive()) {
			eventLog_.appendEvent(QStringLiteral("Recording End"));
			if (!obs_frontend_streaming_active())
				eventLog_.stopSession();
		}
	}
	refreshEventLogUi();
#else
	Q_UNUSED(event);
#endif
}

void FlyScoreDock::logStateChanges(const FlyState &before, const FlyState &after)
{
	if (!eventLog_.isActive())
		return;

	auto teamName = [](const FlyTeam &team, const QString &fallback) {
		return team.title.trimmed().isEmpty() ? fallback : team.title.trimmed();
	};
	auto labelOr = [](const QString &label, const QString &fallback) {
		return label.trimmed().isEmpty() ? fallback : label.trimmed();
	};

	if (before.home.title != after.home.title)
		eventLog_.appendEvent(
			QStringLiteral("Home team: %1").arg(teamName(after.home, QStringLiteral("Home"))));
	if (before.away.title != after.away.title)
		eventLog_.appendEvent(
			QStringLiteral("Guests team: %1").arg(teamName(after.away, QStringLiteral("Guests"))));
	if (before.swap_sides != after.swap_sides)
		eventLog_.appendEvent(after.swap_sides ? QStringLiteral("Sides swapped")
						       : QStringLiteral("Sides restored"));
	if (before.show_scoreboard != after.show_scoreboard)
		eventLog_.appendEvent(after.show_scoreboard ? QStringLiteral("Scoreboard shown")
							    : QStringLiteral("Scoreboard hidden"));

	const int commonFields = static_cast<int>(std::min(before.custom_fields.size(), after.custom_fields.size()));
	for (int i = 0; i < commonFields; ++i) {
		const auto &oldField = before.custom_fields[i];
		const auto &field = after.custom_fields[i];
		const QString label = labelOr(field.label, QStringLiteral("Score %1").arg(i + 1));
		if (oldField.home != field.home) {
			eventLog_.appendEvent(QStringLiteral("%1: %2 (%3-%4)")
						      .arg(label, teamName(after.home, QStringLiteral("Home")))
						      .arg(field.home)
						      .arg(field.away));
		}
		if (oldField.away != field.away) {
			eventLog_.appendEvent(QStringLiteral("%1: %2 (%3-%4)")
						      .arg(label, teamName(after.away, QStringLiteral("Guests")))
						      .arg(field.home)
						      .arg(field.away));
		}
		if (oldField.visible != field.visible)
			eventLog_.appendEvent(QStringLiteral("%1 %2").arg(
				label, field.visible ? QStringLiteral("shown") : QStringLiteral("hidden")));
	}
	if (after.custom_fields.size() != before.custom_fields.size())
		eventLog_.appendEvent(QStringLiteral("Team stat configuration changed"));

	const int commonSingles = static_cast<int>(std::min(before.single_stats.size(), after.single_stats.size()));
	for (int i = 0; i < commonSingles; ++i) {
		const auto &oldStat = before.single_stats[i];
		const auto &stat = after.single_stats[i];
		if (oldStat.value != stat.value)
			eventLog_.appendEvent(QStringLiteral("%1: %2")
						      .arg(labelOr(stat.label, QStringLiteral("Stat %1").arg(i + 1)))
						      .arg(stat.value));
	}
	if (after.single_stats.size() != before.single_stats.size())
		eventLog_.appendEvent(QStringLiteral("Single stat configuration changed"));

	const int commonTimers = static_cast<int>(std::min(before.timers.size(), after.timers.size()));
	for (int i = 0; i < commonTimers; ++i) {
		const auto &oldTimer = before.timers[i];
		const auto &timer = after.timers[i];
		const QString label = labelOr(timer.label, QStringLiteral("Timer %1").arg(i + 1));
		if (oldTimer.running != timer.running)
			eventLog_.appendEvent(QStringLiteral("%1 %2").arg(
				label, timer.running ? QStringLiteral("Start") : QStringLiteral("Pause")));
		else if (!timer.running && oldTimer.remaining_ms != timer.remaining_ms)
			eventLog_.appendEvent(
				QStringLiteral("%1 set to %2 seconds").arg(label).arg(timer.remaining_ms / 1000));
		if (oldTimer.visible != timer.visible)
			eventLog_.appendEvent(QStringLiteral("%1 %2").arg(
				label, timer.visible ? QStringLiteral("shown") : QStringLiteral("hidden")));
	}
	if (after.timers.size() != before.timers.size())
		eventLog_.appendEvent(QStringLiteral("Timer configuration changed"));
}

static int jsonInt(const QJsonObject &o, const QString &key, int fallback = 0)
{
	const QJsonValue v = o.value(key);
	if (v.isDouble())
		return v.toInt(fallback);
	if (v.isString()) {
		bool ok = false;
		const int n = v.toString().toInt(&ok);
		return ok ? n : fallback;
	}
	return fallback;
}

static qint64 jsonInt64(const QJsonObject &o, const QString &key, qint64 fallback = 0)
{
	const QJsonValue v = o.value(key);
	if (v.isDouble())
		return static_cast<qint64>(v.toDouble(fallback));
	if (v.isString()) {
		bool ok = false;
		const qint64 n = v.toString().toLongLong(&ok);
		return ok ? n : fallback;
	}
	return fallback;
}

static bool jsonBool(const QJsonObject &o, const QString &key, bool fallback = false)
{
	const QJsonValue v = o.value(key);
	if (v.isBool())
		return v.toBool(fallback);
	if (v.isDouble())
		return v.toInt() != 0;
	if (v.isString()) {
		const QString s = v.toString().trimmed().toLower();
		if (s == QLatin1String("true") || s == QLatin1String("1") || s == QLatin1String("yes"))
			return true;
		if (s == QLatin1String("false") || s == QLatin1String("0") || s == QLatin1String("no"))
			return false;
	}
	return fallback;
}

static QString jsonString(const QJsonObject &o, const QString &key)
{
	return o.value(key).toString().trimmed();
}

static uint32_t jsonColor(const QJsonObject &o, const QString &key, uint32_t fallback)
{
	const QJsonValue v = o.value(key);
	if (v.isDouble())
		return static_cast<uint32_t>(v.toInt(static_cast<int>(fallback)));
	if (v.isString()) {
		QString s = v.toString().trimmed();
		if (s.startsWith(QLatin1Char('#')))
			s.remove(0, 1);
		bool ok = false;
		const uint n = s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ? s.mid(2).toUInt(&ok, 16)
										       : s.toUInt(&ok, 16);
		if (ok)
			return static_cast<uint32_t>(n);
		const uint dec = s.toUInt(&ok, 10);
		if (ok)
			return static_cast<uint32_t>(dec);
	}
	return fallback;
}

static void setTimerStoppedAtInitial(FlyTimer &timer)
{
	timer.running = false;
	timer.last_tick_ms = 0;
	if (timer.initial_ms < 0)
		timer.initial_ms = 0;
	timer.remaining_ms = timer.initial_ms;
}

void FlyScoreDock::handleRemoteCommand(const QJsonObject &command)
{
	const QString action = jsonString(command, QStringLiteral("action")).isEmpty()
				       ? jsonString(command, QStringLiteral("type"))
				       : jsonString(command, QStringLiteral("action"));

	if (action == QLatin1String("get_state")) {
		broadcastCurrentState();
		return;
	}
	if (action == QLatin1String("log_event")) {
		appendEvent(jsonString(command, QStringLiteral("label")));
		return;
	}
	if (action == QLatin1String("start_event_log")) {
		const QString label = jsonString(command, QStringLiteral("label"));
		startEventLog(label.isEmpty() ? QStringLiteral("Stream Start") : label);
		return;
	}
	if (action == QLatin1String("stop_event_log")) {
		const QString label = jsonString(command, QStringLiteral("label"));
		stopEventLog(label.isEmpty() ? QStringLiteral("Stream End") : label);
		return;
	}

	if (action == QLatin1String("set_state") && command.value(QStringLiteral("state")).isObject()) {
		FlyState next;
		if (fly_state_from_json_object(command.value(QStringLiteral("state")).toObject(), next)) {
			st_ = next;
			saveState();
			refreshUiFromState(false);
		}
		return;
	}

	if (action == QLatin1String("swap")) {
		toggleSwap();
		return;
	}
	if (action == QLatin1String("show_scoreboard")) {
		st_.show_scoreboard = command.value(QStringLiteral("value")).toBool(st_.show_scoreboard);
		saveState();
		refreshUiFromState(false);
		return;
	}
	if (action == QLatin1String("toggle_scoreboard")) {
		toggleScoreboardVisible();
		return;
	}

	if (action == QLatin1String("set_team")) {
		const QString side = jsonString(command, QStringLiteral("side"));
		FlyTeam *team = nullptr;
		if (side == QLatin1String("away") || side == QLatin1String("guest") || side == QLatin1String("guests"))
			team = &st_.away;
		else
			team = &st_.home;

		if (command.contains(QStringLiteral("title")))
			team->title = jsonString(command, QStringLiteral("title"));
		if (command.contains(QStringLiteral("subtitle")))
			team->subtitle = jsonString(command, QStringLiteral("subtitle"));
		if (command.contains(QStringLiteral("logo")))
			team->logo = jsonString(command, QStringLiteral("logo"));
		if (command.contains(QStringLiteral("color")))
			team->color = jsonColor(command, QStringLiteral("color"), team->color);

		saveState();
		refreshUiFromState(false);
		return;
	}

	const int index = jsonInt(command, QStringLiteral("index"));
	if (action == QLatin1String("add_score") || action == QLatin1String("add_field")) {
		FlyCustomField cf;
		cf.label = jsonString(command, QStringLiteral("label"));
		if (cf.label.isEmpty())
			cf.label = QStringLiteral("Score");
		cf.home = std::max(0, jsonInt(command, QStringLiteral("home")));
		cf.away = std::max(0, jsonInt(command, QStringLiteral("away")));
		cf.visible = jsonBool(command, QStringLiteral("visible"), true);
		st_.custom_fields.push_back(cf);
		saveState();
		refreshUiFromState(false);
		hotkeyBindings_ = buildMergedHotkeyBindings();
		applyHotkeyBindings(hotkeyBindings_);
		return;
	}
	if ((action == QLatin1String("remove_score") || action == QLatin1String("remove_field")) && index >= 0 &&
	    index < st_.custom_fields.size()) {
		if (st_.custom_fields.size() > 1) {
			st_.custom_fields.removeAt(index);
			saveState();
			refreshUiFromState(false);
			hotkeyBindings_ = buildMergedHotkeyBindings();
			applyHotkeyBindings(hotkeyBindings_);
		}
		return;
	}
	if ((action == QLatin1String("set_field") || action == QLatin1String("set_score_field")) && index >= 0 &&
	    index < st_.custom_fields.size()) {
		FlyCustomField &cf = st_.custom_fields[index];
		if (command.contains(QStringLiteral("label")))
			cf.label = jsonString(command, QStringLiteral("label"));
		if (command.contains(QStringLiteral("home")))
			cf.home = std::max(0, jsonInt(command, QStringLiteral("home")));
		if (command.contains(QStringLiteral("away")))
			cf.away = std::max(0, jsonInt(command, QStringLiteral("away")));
		if (command.contains(QStringLiteral("visible")))
			cf.visible = jsonBool(command, QStringLiteral("visible"), cf.visible);
		saveState();
		refreshUiFromState(false);
		return;
	}
	if ((action == QLatin1String("toggle_field") || action == QLatin1String("score_visibility")) && index >= 0 &&
	    index < st_.custom_fields.size()) {
		st_.custom_fields[index].visible = !st_.custom_fields[index].visible;
		saveState();
		refreshUiFromState(false);
		return;
	}
	if (action == QLatin1String("bump_score")) {
		const QString side = jsonString(command, QStringLiteral("side"));
		const int delta = jsonInt(command, QStringLiteral("delta"), 1);
		if (side == QLatin1String("away") || side == QLatin1String("guest") ||
		    (side == QLatin1String("y") && !st_.swap_sides) || (side == QLatin1String("x") && st_.swap_sides))
			bumpCustomFieldAway(index, delta);
		else
			bumpCustomFieldHome(index, delta);
		return;
	}
	if (action == QLatin1String("set_score") && index >= 0 && index < st_.custom_fields.size()) {
		const QString side = jsonString(command, QStringLiteral("side"));
		const int value = std::max(0, jsonInt(command, QStringLiteral("value")));
		const bool away = side == QLatin1String("away") || side == QLatin1String("guest") ||
				  (side == QLatin1String("y") && !st_.swap_sides) ||
				  (side == QLatin1String("x") && st_.swap_sides);
		if (away)
			st_.custom_fields[index].away = value;
		else
			st_.custom_fields[index].home = value;
		saveState();
		refreshUiFromState(false);
		return;
	}
	if (action == QLatin1String("bump_single")) {
		bumpSingleStat(index, jsonInt(command, QStringLiteral("delta"), 1));
		return;
	}
	if (action == QLatin1String("toggle_single") && index >= 0 && index < st_.single_stats.size()) {
		toggleSingleStatVisible(index);
		return;
	}
	if (action == QLatin1String("add_single")) {
		FlySingleStat ss;
		ss.label = jsonString(command, QStringLiteral("label"));
		if (ss.label.isEmpty())
			ss.label = QStringLiteral("STAT");
		ss.value = jsonInt(command, QStringLiteral("value"));
		ss.visible = jsonBool(command, QStringLiteral("visible"), true);
		st_.single_stats.push_back(ss);
		saveState();
		refreshUiFromState(false);
		hotkeyBindings_ = buildMergedHotkeyBindings();
		applyHotkeyBindings(hotkeyBindings_);
		return;
	}
	if (action == QLatin1String("remove_single") && index >= 0 && index < st_.single_stats.size()) {
		if (st_.single_stats.size() > 1) {
			st_.single_stats.removeAt(index);
			saveState();
			refreshUiFromState(false);
			hotkeyBindings_ = buildMergedHotkeyBindings();
			applyHotkeyBindings(hotkeyBindings_);
		}
		return;
	}
	if (action == QLatin1String("set_single") && index >= 0 && index < st_.single_stats.size()) {
		FlySingleStat &ss = st_.single_stats[index];
		if (command.contains(QStringLiteral("label")))
			ss.label = jsonString(command, QStringLiteral("label"));
		if (command.contains(QStringLiteral("value")))
			ss.value = jsonInt(command, QStringLiteral("value"));
		if (command.contains(QStringLiteral("visible")))
			ss.visible = jsonBool(command, QStringLiteral("visible"), ss.visible);
		saveState();
		refreshUiFromState(false);
		return;
	}
	if (action == QLatin1String("add_timer")) {
		FlyTimer timer;
		timer.label = jsonString(command, QStringLiteral("label"));
		if (timer.label.isEmpty())
			timer.label = QStringLiteral("Timer");
		timer.mode = jsonString(command, QStringLiteral("mode"));
		if (timer.mode != QLatin1String("countup"))
			timer.mode = QStringLiteral("countdown");
		timer.initial_ms = std::max<qint64>(0, jsonInt64(command, QStringLiteral("initial_ms")));
		timer.visible = jsonBool(command, QStringLiteral("visible"), true);
		setTimerStoppedAtInitial(timer);
		st_.timers.push_back(timer);
		saveState();
		refreshUiFromState(false);
		hotkeyBindings_ = buildMergedHotkeyBindings();
		applyHotkeyBindings(hotkeyBindings_);
		return;
	}
	if (action == QLatin1String("remove_timer") && index >= 0 && index < st_.timers.size()) {
		if (st_.timers.size() > 1) {
			st_.timers.removeAt(index);
			saveState();
			refreshUiFromState(false);
			hotkeyBindings_ = buildMergedHotkeyBindings();
			applyHotkeyBindings(hotkeyBindings_);
		}
		return;
	}
	if (action == QLatin1String("set_timer") && index >= 0 && index < st_.timers.size()) {
		FlyTimer &timer = st_.timers[index];
		const bool wasRunning = timer.running;
		if (wasRunning)
			toggleTimerRunning(index);

		if (command.contains(QStringLiteral("label")))
			timer.label = jsonString(command, QStringLiteral("label"));
		if (command.contains(QStringLiteral("mode"))) {
			const QString mode = jsonString(command, QStringLiteral("mode"));
			timer.mode = mode == QLatin1String("countup") ? QStringLiteral("countup")
								      : QStringLiteral("countdown");
		}
		if (command.contains(QStringLiteral("initial_ms")))
			timer.initial_ms = std::max<qint64>(0, jsonInt64(command, QStringLiteral("initial_ms")));
		if (command.contains(QStringLiteral("remaining_ms")))
			timer.remaining_ms = std::max<qint64>(0, jsonInt64(command, QStringLiteral("remaining_ms")));
		else if (command.contains(QStringLiteral("initial_ms")))
			timer.remaining_ms = timer.initial_ms;
		if (command.contains(QStringLiteral("visible")))
			timer.visible = jsonBool(command, QStringLiteral("visible"), timer.visible);

		timer.running = false;
		timer.last_tick_ms = 0;
		saveState();
		refreshUiFromState(false);
		return;
	}
	if (action == QLatin1String("timer_toggle")) {
		toggleTimerRunning(index);
		return;
	}
	if (action == QLatin1String("timer_visibility") && index >= 0 && index < st_.timers.size()) {
		st_.timers[index].visible = !st_.timers[index].visible;
		saveState();
		refreshUiFromState(false);
		return;
	}
	if ((action == QLatin1String("timer_start") || action == QLatin1String("timer_pause")) && index >= 0 &&
	    index < st_.timers.size()) {
		const bool shouldRun = action == QLatin1String("timer_start");
		if (st_.timers[index].running != shouldRun)
			toggleTimerRunning(index);
		return;
	}
	if (action == QLatin1String("timer_reset") && index >= 0 && index < st_.timers.size()) {
		FlyTimer &t = st_.timers[index];
		t.remaining_ms = std::max<qint64>(0, t.initial_ms);
		t.running = false;
		t.last_tick_ms = 0;
		saveState();
		refreshUiFromState(false);
		return;
	}
}

void FlyScoreDock::loadState()
{
	if (!fly_state_load(dataDir_, st_)) {
		st_ = fly_state_make_defaults();
		fly_state_save(dataDir_, st_);
	}

	if (st_.timers.isEmpty()) {
		FlyTimer main;
		main.label = fly_i18n("Default.Timer.FirstHalf");
		main.mode = QStringLiteral("countup");
		main.running = false;
		main.initial_ms = 0;
		main.remaining_ms = 0;
		main.last_tick_ms = 0;
		st_.timers.push_back(main);
		fly_state_save(dataDir_, st_);
	} else if (st_.timers[0].mode.isEmpty()) {
		st_.timers[0].mode = QStringLiteral("countup");
	}
}

void FlyScoreDock::saveState()
{
	logStateChanges(lastLoggedState_, st_);
	fly_state_save(dataDir_, st_);
	lastLoggedState_ = st_;
	broadcastCurrentState();
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
	auto rc = QMessageBox::question(this, fly_i18n("Dock.ResetValuesTitle"), fly_i18n("Dock.ResetValuesMessage"));
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

	{
		auto *hdrRow = new QWidget(this);
		auto *grid = new QGridLayout(hdrRow);
		grid->setContentsMargins(0, 0, 0, 0);
		grid->setHorizontalSpacing(6);
		grid->setVerticalSpacing(0);

		auto *cbSpacer = new QSpacerItem(22, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
		grid->addItem(cbSpacer, 0, 0);

		auto *statHdr = new QLabel(fly_i18n("Common.Stat"), hdrRow);
		statHdr->setStyleSheet(QStringLiteral("font-weight:bold;"));
		grid->addWidget(statHdr, 0, 1);

		auto *homeHdr = new QLabel(fly_i18n("Common.Home"), hdrRow);
		homeHdr->setStyleSheet(QStringLiteral("font-weight:bold;"));
		homeHdr->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		grid->addWidget(homeHdr, 0, 2);

		auto *guestHdr = new QLabel(fly_i18n("Common.Guests"), hdrRow);
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

		auto makeStepBtn = [this](const QString &themeIconName, QStyle::StandardPixmap fallbackPixmap,
					  const QString &fallbackText, const QString &tooltip,
					  QWidget *parent) -> QToolButton * {
			auto *btn = new QToolButton(parent);
			btn->setToolTip(tooltip);
			btn->setCursor(Qt::PointingHandCursor);
			btn->setAutoRaise(false);
			btn->setFocusPolicy(Qt::StrongFocus);
			btn->setFixedSize(28, 28);
			btn->setIconSize(QSize(14, 14));

			QIcon icon = QIcon::fromTheme(themeIconName);
			if (icon.isNull())
				icon = style()->standardIcon(fallbackPixmap);

			if (!icon.isNull()) {
				btn->setIcon(icon);
			} else {
				btn->setText(fallbackText);
			}

			return btn;
		};

		auto *homeSpin = new QSpinBox(row);
		homeSpin->setRange(0, std::numeric_limits<int>::max());
		homeSpin->setValue(std::max(0, cf.home));
		homeSpin->setMinimumWidth(86);
		homeSpin->setMaximumHeight(32);
		homeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

		auto *minusHome = makeStepBtn(QStringLiteral("list-remove"), QStyle::SP_ArrowDown, QStringLiteral("-"),
					      fly_i18n("Dock.HomeMinusOne"), row);

		auto *plusHome = makeStepBtn(QStringLiteral("list-add"), QStyle::SP_ArrowUp, QStringLiteral("+"),
					     fly_i18n("Dock.HomePlusOne"), row);

		auto *awaySpin = new QSpinBox(row);
		awaySpin->setRange(0, std::numeric_limits<int>::max());
		awaySpin->setValue(std::max(0, cf.away));
		awaySpin->setMinimumWidth(86);
		awaySpin->setMaximumHeight(32);
		awaySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

		auto *minusAway = makeStepBtn(QStringLiteral("list-remove"), QStyle::SP_ArrowDown, QStringLiteral("-"),
					      fly_i18n("Dock.GuestsMinusOne"), row);

		auto *plusAway = makeStepBtn(QStringLiteral("list-add"), QStyle::SP_ArrowUp, QStringLiteral("+"),
					     fly_i18n("Dock.GuestsPlusOne"), row);

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

		connect(minusHome, &QToolButton::clicked, this, [homeSpin, sync]() {
			homeSpin->setValue(std::max(0, homeSpin->value() - 1));
			sync();
		});
		connect(plusHome, &QToolButton::clicked, this, [homeSpin, sync]() {
			homeSpin->setValue(homeSpin->value() + 1);
			sync();
		});
		connect(minusAway, &QToolButton::clicked, this, [awaySpin, sync]() {
			awaySpin->setValue(std::max(0, awaySpin->value() - 1));
			sync();
		});
		connect(plusAway, &QToolButton::clicked, this, [awaySpin, sync]() {
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

void FlyScoreDock::clearAllSingleStatRows()
{
	if (singleStatsLayout_) {
		QLayoutItem *it = nullptr;
		while ((it = singleStatsLayout_->takeAt(0)) != nullptr) {
			if (QWidget *w = it->widget())
				w->deleteLater();
			delete it;
		}
	}

	singleStats_.clear();
}

void FlyScoreDock::loadSingleStatControlsFromState()
{
	clearAllSingleStatRows();
	if (!singleStatsLayout_)
		return;

	singleStats_.reserve(st_.single_stats.size());

	auto makeStepBtn = [this](const QString &themeIconName, QStyle::StandardPixmap fallbackPixmap,
				  const QString &fallbackText, const QString &tooltip,
				  QWidget *parent) -> QToolButton * {
		auto *btn = new QToolButton(parent);
		btn->setToolTip(tooltip);
		btn->setCursor(Qt::PointingHandCursor);
		btn->setAutoRaise(false);
		btn->setFocusPolicy(Qt::StrongFocus);
		btn->setFixedSize(28, 28);
		btn->setIconSize(QSize(14, 14));

		QIcon icon = QIcon::fromTheme(themeIconName);
		if (icon.isNull())
			icon = this->style()->standardIcon(fallbackPixmap);

		if (!icon.isNull())
			btn->setIcon(icon);
		else
			btn->setText(fallbackText);

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

		auto *labelLbl =
			new QLabel(ss.label.isEmpty() ? fly_i18n("Hotkey.SingleStatN").arg(i + 1) : ss.label, row);
		labelLbl->setMinimumWidth(120);

		auto *valueSpin = new QSpinBox(row);
		valueSpin->setRange(-9999, 9999);
		valueSpin->setValue(ss.value);
		valueSpin->setMinimumWidth(60);

		auto *minusBtn = makeStepBtn(QStringLiteral("list-remove"), QStyle::SP_ArrowDown, QStringLiteral("-"),
					     fly_i18n("Dock.LabelMinusOne").arg(labelLbl->text()), row);

		auto *plusBtn = makeStepBtn(QStringLiteral("list-add"), QStyle::SP_ArrowUp, QStringLiteral("+"),
					    fly_i18n("Dock.LabelPlusOne").arg(labelLbl->text()), row);

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

		connect(minusBtn, &QToolButton::clicked, this, [valueSpin, sync]() {
			valueSpin->setValue(valueSpin->value() - 1);
			sync();
		});

		connect(plusBtn, &QToolButton::clicked, this, [valueSpin, sync]() {
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

void FlyScoreDock::clearAllTimerRows()
{
	if (timersLayout_) {
		QLayoutItem *it = nullptr;
		while ((it = timersLayout_->takeAt(0)) != nullptr) {
			if (QWidget *w = it->widget())
				w->deleteLater();
			delete it;
		}
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
		main.label = fly_i18n("Default.Timer.FirstHalf");
		main.mode = QStringLiteral("countup");
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
				     tm.running ? fly_i18n("Dock.PauseTimer") : fly_i18n("Dock.StartTimer"), row);

		auto *resetBtn = makeEmojiBtn(QStringLiteral("🔄️"), fly_i18n("Dock.ResetTimer"), row);

		const int h = timeEdit->sizeHint().height();
		startStopBtn->setFixedSize(h, h);
		resetBtn->setFixedSize(h, h);

		lay->addWidget(visibleCheck, 0, Qt::AlignVCenter);
		lay->addWidget(labelLbl);
		lay->addStretch(1);
		lay->addWidget(timeEdit, 0, Qt::AlignVCenter);
		lay->addWidget(startStopBtn, 0, Qt::AlignVCenter);
		lay->addWidget(resetBtn, 0, Qt::AlignVCenter);

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

		connect(startStopBtn, &QPushButton::clicked, this, [this, i]() { toggleTimerRunning(i); });

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

void FlyScoreDock::bumpCustomFieldHome(int index, int delta)
{
	if (index < 0 || index >= st_.custom_fields.size())
		return;

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

void FlyScoreDock::onOpenCustomFieldsDialog()
{
	const FlyState before = st_;
	FlyFieldsDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	logStateChanges(before, st_);
	lastLoggedState_ = st_;
	refreshUiFromState(false);

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);
}

void FlyScoreDock::onOpenTimersDialog()
{
	const FlyState before = st_;
	FlyTimersDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	logStateChanges(before, st_);
	lastLoggedState_ = st_;
	refreshUiFromState(false);

	hotkeyBindings_ = buildMergedHotkeyBindings();
	applyHotkeyBindings(hotkeyBindings_);
}

void FlyScoreDock::onOpenTeamsDialog()
{
	const FlyState before = st_;
	FlyTeamsDialog dlg(dataDir_, st_, this);
	dlg.exec();

	loadState();
	logStateChanges(before, st_);
	lastLoggedState_ = st_;
	refreshUiFromState(false);
}

void FlyScoreDock::ensureResourcesDefaults()
{
	QString resDir = fly_get_data_root_no_ui();
	if (resDir.isEmpty())
		resDir = dataDir_;

#ifdef ENABLE_EMBEDDED_DEFAULTS
	auto writeDefaultText = [](const QString &path, const char *contents) {
		if (QFileInfo::exists(path) || !contents)
			return;
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile f(path);
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
			f.write(contents);
	};

	writeDefaultText(QDir(resDir).filePath(QStringLiteral("index.html")), fly_score_embedded::index_html);
	writeDefaultText(QDir(resDir).filePath(QStringLiteral("fouls.html")), fly_score_embedded::fouls_html);
	writeDefaultText(QDir(resDir).filePath(QStringLiteral("cards.html")), fly_score_embedded::cards_html);
	writeDefaultText(QDir(resDir).filePath(QStringLiteral("corners.html")), fly_score_embedded::corners_html);
	writeDefaultText(QDir(resDir).filePath(QStringLiteral("manifest.ini")), fly_score_embedded::manifest_ini);
	writeDefaultText(QDir(resDir).filePath(QStringLiteral("style.css")), fly_score_embedded::style_css);
	writeDefaultText(QDir(resDir).filePath(QStringLiteral("script.js")), fly_score_embedded::script_js);
#endif

	fly_state_ensure_json_exists(resDir, &st_);
}

static QWidget *g_dockContent = nullptr;

FlyScoreDock::~FlyScoreDock()
{
#ifdef ENABLE_FRONTEND_API
	if (frontendEventCallbackConnected_) {
		obs_frontend_remove_event_callback(fly_on_frontend_event, this);
		frontendEventCallbackConnected_ = false;
	}
#endif
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
