#define LOG_TAG "[fly-scoreboard][dock]"
#include "fly_score_log.hpp"

#include "fly_score_dock.hpp"
#include "fly_score_state.hpp"
#include "fly_score_server.hpp"
#include "fly_score_settings_dialog.hpp"
#include "seed_defaults.hpp"
#include "fly_score_const.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <QRegularExpression>
#include <QBoxLayout>
#include <QDateTime>
#include <QFileDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QFrame>
#include <QDesktopServices>
#include <QComboBox>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QMimeDatabase>
#include <QStyle>
#include <QAbstractButton>
#include <QUrl>
#include <QUrlQuery>
#include <QCheckBox>
#include <QIcon>
#include <QSize>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QCryptographicHash>
#include <QMessageBox>

#include <algorithm>

static qint64 now_ms()
{
	return QDateTime::currentMSecsSinceEpoch();
}

static QIcon themedIcon(QWidget *w, const char *name, QStyle::StandardPixmap fallback)
{
	QIcon ic = QIcon::fromTheme(QString::fromUtf8(name));
	if (!ic.isNull())
		return ic;
	return w->style()->standardIcon(fallback);
}

static void styleIconOnlyButton(QAbstractButton *b, const QIcon &icon, const QString &tooltip,
				const QSize &iconSize = QSize(20, 20))
{
	b->setText(QString());
	b->setIcon(icon);
	b->setIconSize(iconSize);
	b->setToolTip(tooltip);
	b->setCursor(Qt::PointingHandCursor);
	if (auto *pb = qobject_cast<QPushButton *>(b))
		pb->setFlat(true);
}

static qint64 parse_mmss_to_ms(const QString &txt)
{
	static const QRegularExpression re(R"(^\s*(\d+)\s*:\s*([0-5]\d)\s*$)");
	auto m = re.match(txt);
	if (!m.hasMatch())
		return -1;
	const qint64 minutes = m.captured(1).toLongLong();
	const qint64 seconds = m.captured(2).toLongLong();
	return (minutes * 60 + seconds) * 1000;
}

static QString format_ms_mmss(qint64 ms)
{
	if (ms < 0)
		ms = 0;
	qint64 total = ms / 1000;
	qint64 m = total / 60;
	qint64 s = total % 60;
	return QString::number(m) + ":" + QString::number(s).rightJustified(2, '0');
}

static QString cacheBustUrl(const QString &in)
{
	QUrl u(in);
	QUrlQuery q(u);
	q.removeAllQueryItems("cb");
	q.addQueryItem("cb", QString::number(QDateTime::currentMSecsSinceEpoch()));
	u.setQuery(q);
	return u.toString();
}

static bool refresh_browser_source_by_name(const QString &name)
{
	if (name.trimmed().isEmpty())
		return false;

	obs_source_t *src = obs_get_source_by_name(name.toUtf8().constData());
	if (!src) {
		LOGW("Browser source not found: %s", name.toUtf8().constData());
		return false;
	}

	bool ok = false;
	const char *sid = obs_source_get_id(src);
	if (sid && strcmp(sid, "browser_source") == 0) {
		obs_data_t *settings = obs_source_get_settings(src);
		if (settings) {
			const bool isLocal = obs_data_get_bool(settings, "is_local_file");
			if (!isLocal) {
				const char *curl = obs_data_get_string(settings, "url");
				QString url = cacheBustUrl(QString::fromUtf8(curl ? curl : ""));
				obs_data_set_string(settings, "url", url.toUtf8().constData());
				obs_source_update(src, settings);
				ok = true;
			} else {
				bool shutdown = obs_data_get_bool(settings, "shutdown");
				obs_data_set_bool(settings, "shutdown", !shutdown);
				obs_source_update(src, settings);
				obs_data_set_bool(settings, "shutdown", shutdown);
				obs_source_update(src, settings);
				ok = true;
			}
			obs_data_release(settings);
		}
	} else {
		LOGW("Source '%s' is not a browser_source", name.toUtf8().constData());
	}
	obs_source_release(src);
	return ok;
}

FlyScoreDock::FlyScoreDock(QWidget *parent) : QWidget(parent) {}

bool FlyScoreDock::init()
{
	fly_ensure_webroot(&dataDir_);
	loadState();

	auto root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	auto row1 = new QHBoxLayout();
	time_label_ = new QLineEdit(this);
	time_label_->setText(st_.time_label);
	row1->addWidget(new QLabel("Time Label:", this));
	row1->addWidget(time_label_, 1);

	auto timerBox = new QGroupBox("Timer", this);
	auto timerGrid = new QGridLayout(timerBox);
	timerGrid->setContentsMargins(8, 8, 8, 8);
	timerGrid->setHorizontalSpacing(8);

	time_ = new QLineEdit(timerBox);
	time_->setPlaceholderText("mm:ss");
	time_->setClearButtonEnabled(true);
	time_->setText(format_ms_mmss(st_.timer.remaining_ms));
	time_->setMinimumWidth(70);
	time_->setMaxLength(8);

	mode_ = new QComboBox(timerBox);
	mode_->addItems({"countdown", "countup"});
	if (st_.timer.mode.isEmpty())
		st_.timer.mode = "countdown";
	mode_->setCurrentText(st_.timer.mode);

	startStop_ = new QPushButton(timerBox);
	reset_ = new QPushButton(timerBox);
	{
		const QIcon playIcon = themedIcon(this, "media-playback-start", QStyle::SP_MediaPlay);
		const QIcon pauseIcon = themedIcon(this, "media-playback-pause", QStyle::SP_MediaPause);
		const QIcon resetIcon = themedIcon(this, "view-refresh", QStyle::SP_BrowserReload);
		styleIconOnlyButton(startStop_, st_.timer.running ? pauseIcon : playIcon,
				    st_.timer.running ? "Pause timer" : "Start timer");
		styleIconOnlyButton(reset_, resetIcon, "Reset timer");
	}

	int r = 0, c = 0;
	timerGrid->addWidget(new QLabel("Time:", timerBox), r, c++);
	timerGrid->addWidget(time_, r, c++);
	timerGrid->addWidget(new QLabel("Type:", timerBox), r, c++);
	timerGrid->addWidget(mode_, r, c++);
	timerGrid->addWidget(startStop_, r, c++);
	timerGrid->addWidget(reset_, r, c++);
	timerGrid->setColumnStretch(c, 1);

	auto scoreBox = new QGroupBox("Scoreboard", this);
	auto scoreGrid = new QGridLayout();
	scoreGrid->setContentsMargins(8, 8, 8, 8);
	scoreGrid->setHorizontalSpacing(8);

	homeScore_ = new QSpinBox(scoreBox);
	homeScore_->setRange(0, 999);
	homeScore_->setValue(st_.home.score);

	awayScore_ = new QSpinBox(scoreBox);
	awayScore_->setRange(0, 999);
	awayScore_->setValue(st_.away.score);

	homeRounds_ = new QSpinBox(scoreBox);
	awayRounds_ = new QSpinBox(scoreBox);
	homeRounds_->setRange(0, 99);
	awayRounds_->setRange(0, 99);
	homeRounds_->setValue(st_.home.rounds);
	awayRounds_->setValue(st_.away.rounds);

	int srow = 0;
	scoreGrid->addWidget(new QLabel("Home:", scoreBox), srow, 0, Qt::AlignRight);
	scoreGrid->addWidget(homeScore_, srow, 1);
	scoreGrid->addItem(new QSpacerItem(20, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), srow, 2);
	scoreGrid->addWidget(new QLabel("Guests:", scoreBox), srow, 3, Qt::AlignRight);
	scoreGrid->addWidget(awayScore_, srow, 4);

	srow++;
	scoreGrid->addWidget(new QLabel("Home Wins:", scoreBox), srow, 0, Qt::AlignRight);
	scoreGrid->addWidget(homeRounds_, srow, 1);
	scoreGrid->addItem(new QSpacerItem(20, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), srow, 2);
	scoreGrid->addWidget(new QLabel("Guests Wins:", scoreBox), srow, 3, Qt::AlignRight);
	scoreGrid->addWidget(awayRounds_, srow, 4);
	scoreGrid->setColumnStretch(2, 1);

	homeScore_->setMaximumWidth(80);
	awayScore_->setMaximumWidth(80);
	homeRounds_->setMaximumWidth(80);
	awayRounds_->setMaximumWidth(80);

	auto toggleRow = new QHBoxLayout();
	swapSides_ = new QCheckBox("Swap Home ↔ Guests", scoreBox);
	showScoreboard_ = new QCheckBox("Show Scoreboard", scoreBox);
	toggleRow->addWidget(swapSides_);
	toggleRow->addSpacing(12);
	toggleRow->addWidget(showScoreboard_);
	toggleRow->addStretch(1);

	auto scoreVBox = new QVBoxLayout(scoreBox);
	scoreVBox->setContentsMargins(8, 8, 8, 8);
	scoreVBox->setSpacing(6);
	scoreVBox->addLayout(scoreGrid);
	scoreVBox->addLayout(toggleRow);
	scoreBox->setLayout(scoreVBox);

	auto gbHome = new QGroupBox("Home team", this);
	auto hb = new QGridLayout(gbHome);
	homeTitle_ = new QLineEdit(st_.home.title, gbHome);
	homeSub_ = new QLineEdit(st_.home.subtitle, gbHome);
	homeLogo_ = new QLineEdit(st_.home.logo, gbHome);
	homeBrowse_ = new QToolButton(gbHome);
	{
		const QIcon openIcon = themedIcon(this, "folder-open", QStyle::SP_DirOpenIcon);
		homeBrowse_->setText(QString());
		homeBrowse_->setIcon(openIcon);
		homeBrowse_->setToolTip("Browse logo…");
		homeBrowse_->setAutoRaise(true);
	}
	hb->addWidget(new QLabel("Title:", gbHome), 0, 0);
	hb->addWidget(homeTitle_, 0, 1, 1, 2);
	hb->addWidget(new QLabel("Subtitle:", gbHome), 1, 0);
	hb->addWidget(homeSub_, 1, 1, 1, 2);
	hb->addWidget(new QLabel("Logo:", gbHome), 2, 0);
	hb->addWidget(homeLogo_, 2, 1);
	hb->addWidget(homeBrowse_, 2, 2);

	auto gbAway = new QGroupBox("Guests team", this);
	auto ab = new QGridLayout(gbAway);
	awayTitle_ = new QLineEdit(st_.away.title, gbAway);
	awaySub_ = new QLineEdit(st_.away.subtitle, gbAway);
	awayLogo_ = new QLineEdit(st_.away.logo, gbAway);
	awayBrowse_ = new QToolButton(gbAway);
	{
		const QIcon openIcon = themedIcon(this, "folder-open", QStyle::SP_DirOpenIcon);
		awayBrowse_->setText(QString());
		awayBrowse_->setIcon(openIcon);
		awayBrowse_->setToolTip("Browse logo…");
		awayBrowse_->setAutoRaise(true);
	}
	ab->addWidget(new QLabel("Title:", gbAway), 0, 0);
	ab->addWidget(awayTitle_, 0, 1, 1, 2);
	ab->addWidget(new QLabel("Subtitle:", gbAway), 1, 0);
	ab->addWidget(awaySub_, 1, 1, 1, 2);
	ab->addWidget(new QLabel("Logo:", gbAway), 2, 0);
	ab->addWidget(awayLogo_, 2, 1);
	ab->addWidget(awayBrowse_, 2, 2);

	auto bottomRow = new QHBoxLayout();

	auto *refreshBrowserBtn = new QPushButton(this);
	{
		const QIcon refIcon = themedIcon(this, "view-refresh", QStyle::SP_BrowserReload);
		styleIconOnlyButton(refreshBrowserBtn, refIcon, QStringLiteral("Refresh ‘%1’").arg(kBrowserSourceName));
	}

	auto *clearBtn = new QPushButton(this);
	{
		const QIcon delIcon = themedIcon(this, "edit-clear", QStyle::SP_TrashIcon);
		styleIconOnlyButton(clearBtn, delIcon, "Clear teams, delete icons, reset plugin.json");
	}

	applyBtn_ = new QPushButton(this);
	auto *settingsBtn = new QPushButton(this);
	{
		const QIcon saveIcon = themedIcon(this, "document-save", QStyle::SP_DialogSaveButton);
		const QIcon settingsIcon = QIcon::fromTheme("preferences-system");
		const QIcon settingsFb = this->style()->standardIcon(QStyle::SP_FileDialogDetailedView);
		styleIconOnlyButton(applyBtn_, saveIcon, "Save / apply changes");
		styleIconOnlyButton(settingsBtn, settingsIcon.isNull() ? settingsFb : settingsIcon, "Settings");
	}

	bottomRow->addWidget(refreshBrowserBtn);
	bottomRow->addWidget(clearBtn);
	bottomRow->addStretch(1);
	bottomRow->addWidget(applyBtn_);
	bottomRow->addWidget(settingsBtn);

	root->addLayout(row1);
	root->addWidget(timerBox);
	root->addWidget(scoreBox);
	root->addWidget(gbHome);
	root->addWidget(gbAway);
	root->addLayout(bottomRow);

	root->addStretch(1);

	auto *kofiCard = new QFrame(this);
	kofiCard->setObjectName("kofiCard");
	kofiCard->setFrameShape(QFrame::NoFrame);
	kofiCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto *kofiLay = new QHBoxLayout(kofiCard);
	kofiLay->setContentsMargins(12, 10, 12, 10);
	kofiLay->setSpacing(10);

	auto *kofiIcon = new QLabel(kofiCard);
	kofiIcon->setText(QString::fromUtf8("☕"));
	kofiIcon->setStyleSheet("font-size:30px;");

	auto *kofiText = new QLabel(kofiCard);
	kofiText->setTextFormat(Qt::RichText);
	kofiText->setOpenExternalLinks(true);
	kofiText->setTextInteractionFlags(Qt::TextBrowserInteraction);
	kofiText->setWordWrap(true);
	kofiText->setText("<b>Enjoying this plugin?</b><br>You can support development on Ko-fi.");

	auto *kofiBtn = new QPushButton("☕ Buy me a Ko-fi", kofiCard);
	kofiBtn->setCursor(Qt::PointingHandCursor);
	kofiBtn->setMinimumHeight(28);
	kofiBtn->setStyleSheet(
		"QPushButton { background: #29abe0; color:white; border:none; border-radius:6px; padding:6px 10px; font-weight:600; }"
		"QPushButton:hover { background: #1e97c6; }"
		"QPushButton:pressed { background: #1984ac; }");
	QObject::connect(kofiBtn, &QPushButton::clicked, this,
			 [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://ko-fi.com/mmltech"))); });

	kofiLay->addWidget(kofiIcon, 0, Qt::AlignVCenter);
	kofiLay->addWidget(kofiText, 1);
	kofiLay->addWidget(kofiBtn, 0, Qt::AlignVCenter);

	kofiCard->setStyleSheet("#kofiCard {"
				"  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2a2d30, stop:1 #1e2124);"
				"  border:1px solid #3a3d40; border-radius:10px; padding:6px; }"
				"#kofiCard QLabel { color:#ffffff; }");

	root->addWidget(kofiCard);

	uiTimer_ = new QTimer(this);
	connect(uiTimer_, &QTimer::timeout, this, &FlyScoreDock::onTick);
	uiTimer_->start(200);

	connect(homeScore_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
		st_.home.score = v;
		saveState();
	});
	connect(awayScore_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
		st_.away.score = v;
		saveState();
	});
	connect(homeRounds_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
		st_.home.rounds = v;
		saveState();
	});
	connect(awayRounds_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
		st_.away.rounds = v;
		saveState();
	});

	connect(swapSides_, &QCheckBox::toggled, this, [this](bool on) {
		st_.swap_sides = on;
		saveState();
	});
	connect(showScoreboard_, &QCheckBox::toggled, this, [this](bool on) {
		st_.show_scoreboard = on;
		saveState();
	});

	connect(time_label_, &QLineEdit::editingFinished, this, [this] {
		st_.time_label = time_label_->text();
		saveState();
	});
	connect(time_label_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.time_label = t;
		saveState();
	});

	connect(homeTitle_, &QLineEdit::editingFinished, this, [this] {
		st_.home.title = homeTitle_->text();
		saveState();
	});
	connect(homeTitle_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.home.title = t;
		saveState();
	});
	connect(homeSub_, &QLineEdit::editingFinished, this, [this] {
		st_.home.subtitle = homeSub_->text();
		saveState();
	});
	connect(homeSub_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.home.subtitle = t;
		saveState();
	});
	connect(homeLogo_, &QLineEdit::editingFinished, this, [this] {
		st_.home.logo = homeLogo_->text();
		saveState();
	});
	connect(homeLogo_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.home.logo = t;
		saveState();
	});

	connect(awayTitle_, &QLineEdit::editingFinished, this, [this] {
		st_.away.title = awayTitle_->text();
		saveState();
	});
	connect(awayTitle_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.away.title = t;
		saveState();
	});
	connect(awaySub_, &QLineEdit::editingFinished, this, [this] {
		st_.away.subtitle = awaySub_->text();
		saveState();
	});
	connect(awaySub_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.away.subtitle = t;
		saveState();
	});
	connect(awayLogo_, &QLineEdit::editingFinished, this, [this] {
		st_.away.logo = awayLogo_->text();
		saveState();
	});
	connect(awayLogo_, &QLineEdit::textEdited, this, [this](const QString &t) {
		st_.away.logo = t;
		saveState();
	});

	connect(time_, &QLineEdit::editingFinished, this, &FlyScoreDock::onTimeEdited);

	connect(mode_, &QComboBox::currentTextChanged, this, [this](const QString &m) {
		st_.timer.mode = m;
		saveState();
	});
	connect(mode_, qOverload<int>(&QComboBox::activated), this, [this](int) {
		const QString m = mode_->currentText();
		if (m != st_.timer.mode) {
			st_.timer.mode = m;
			saveState();
		}
	});

	connect(startStop_, &QPushButton::clicked, this, &FlyScoreDock::onStartStop);
	connect(reset_, &QPushButton::clicked, this, &FlyScoreDock::onReset);
	connect(homeBrowse_, &QToolButton::clicked, this, &FlyScoreDock::onBrowseHomeLogo);
	connect(awayBrowse_, &QToolButton::clicked, this, &FlyScoreDock::onBrowseAwayLogo);
	connect(applyBtn_, &QPushButton::clicked, this, &FlyScoreDock::onApply);

	connect(refreshBrowserBtn, &QPushButton::clicked, this, []() {
		const bool ok = refresh_browser_source_by_name(QString::fromUtf8(kBrowserSourceName));
		if (!ok)
			LOGW("Refresh failed for Browser Source: %s", kBrowserSourceName);
	});

	connect(settingsBtn, &QPushButton::clicked, this, [this]() {
		auto *dlg = new FlySettingsDialog(dataDir_, this);
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		dlg->show();
	});

	connect(clearBtn, &QPushButton::clicked, this, &FlyScoreDock::onClearTeamsAndReset);

	refreshUiFromState(false);
	return true;
}

void FlyScoreDock::loadState()
{
	fly_state_load(dataDir_, st_);
	if (st_.timer.mode.isEmpty())
		st_.timer.mode = "countdown";
}

void FlyScoreDock::saveState()
{
	fly_state_save(dataDir_, st_);
}

void FlyScoreDock::refreshUiFromState(bool onlyTimeIfRunning)
{
	if (!onlyTimeIfRunning || st_.timer.running) {
		if (time_ && !time_->hasFocus())
			time_->setText(format_ms_mmss(st_.timer.remaining_ms));
	}

	if (startStop_) {
		const QIcon playIcon = themedIcon(this, "media-playback-start", QStyle::SP_MediaPlay);
		const QIcon pauseIcon = themedIcon(this, "media-playback-pause", QStyle::SP_MediaPause);
		styleIconOnlyButton(startStop_, st_.timer.running ? pauseIcon : playIcon,
				    st_.timer.running ? "Pause timer" : "Start timer");
	}

	if (mode_ && !mode_->hasFocus() && mode_->currentText() != st_.timer.mode)
		mode_->setCurrentText(st_.timer.mode);

	if (homeScore_ && !homeScore_->hasFocus() && homeScore_->value() != st_.home.score)
		homeScore_->setValue(st_.home.score);
	if (awayScore_ && !awayScore_->hasFocus() && awayScore_->value() != st_.away.score)
		awayScore_->setValue(st_.away.score);

	if (homeRounds_ && !homeRounds_->hasFocus() && homeRounds_->value() != st_.home.rounds)
		homeRounds_->setValue(st_.home.rounds);
	if (awayRounds_ && !awayRounds_->hasFocus() && awayRounds_->value() != st_.away.rounds)
		awayRounds_->setValue(st_.away.rounds);

	if (swapSides_ && swapSides_->isChecked() != st_.swap_sides)
		swapSides_->setChecked(st_.swap_sides);
	if (showScoreboard_ && showScoreboard_->isChecked() != st_.show_scoreboard)
		showScoreboard_->setChecked(st_.show_scoreboard);

	if (time_label_ && !time_label_->hasFocus() && time_label_->text() != st_.time_label)
		time_label_->setText(st_.time_label);

	if (homeTitle_ && !homeTitle_->hasFocus() && homeTitle_->text() != st_.home.title)
		homeTitle_->setText(st_.home.title);
	if (homeSub_ && !homeSub_->hasFocus() && homeSub_->text() != st_.home.subtitle)
		homeSub_->setText(st_.home.subtitle);
	if (homeLogo_ && !homeLogo_->hasFocus() && homeLogo_->text() != st_.home.logo)
		homeLogo_->setText(st_.home.logo);

	if (awayTitle_ && !awayTitle_->hasFocus() && awayTitle_->text() != st_.away.title)
		awayTitle_->setText(st_.away.title);
	if (awaySub_ && !awaySub_->hasFocus() && awaySub_->text() != st_.away.subtitle)
		awaySub_->setText(st_.away.subtitle);
	if (awayLogo_ && !awayLogo_->hasFocus() && awayLogo_->text() != st_.away.logo)
		awayLogo_->setText(st_.away.logo);
}

void FlyScoreDock::ensureTimerAccounting()
{
	if (!st_.timer.running)
		return;
	const qint64 now = now_ms();
	const qint64 elapsed = now - st_.timer.last_tick_ms;
	st_.timer.last_tick_ms = now;

	if (st_.timer.mode == "countdown") {
		st_.timer.remaining_ms = std::max<qint64>(0LL, st_.timer.remaining_ms - elapsed);
		if (st_.timer.remaining_ms == 0)
			st_.timer.running = false;
	} else {
		st_.timer.remaining_ms += elapsed;
	}
}

void FlyScoreDock::onTick()
{
	const bool wasRunning = st_.timer.running;
	ensureTimerAccounting();
	refreshUiFromState(/*onlyTimeIfRunning=*/true);
	if (wasRunning)
		saveState();
}

void FlyScoreDock::onStartStop()
{
	if (mode_) {
		const QString uiMode = mode_->currentText();
		if (uiMode != st_.timer.mode) {
			st_.timer.mode = uiMode;
			saveState();
		}
	}

	if (!st_.timer.running) {
		qint64 ms = parse_mmss_to_ms(time_->text());
		if (st_.timer.mode == "countdown") {
			if (ms < 0)
				ms = (st_.timer.remaining_ms > 0 ? st_.timer.remaining_ms : 0);
			st_.timer.initial_ms = ms;
			st_.timer.remaining_ms = ms;
		} else {
			if (ms < 0)
				ms = 0;
			st_.timer.initial_ms = ms;
			st_.timer.remaining_ms = ms;
		}
		st_.timer.last_tick_ms = now_ms();
		st_.timer.running = true;
	} else {
		ensureTimerAccounting();
		st_.timer.running = false;
	}

	saveState();
	refreshUiFromState(false);
}

void FlyScoreDock::onReset()
{
	if (mode_) {
		const QString uiMode = mode_->currentText();
		if (uiMode != st_.timer.mode) {
			st_.timer.mode = uiMode;
		}
	}

	qint64 ms = parse_mmss_to_ms(time_->text());
	if (st_.timer.mode == "countdown") {
		if (ms < 0)
			ms = st_.timer.initial_ms;
	} else {
		if (ms < 0)
			ms = 0;
	}
	st_.timer.initial_ms = ms;
	st_.timer.remaining_ms = ms;
	st_.timer.running = false;

	saveState();
	refreshUiFromState(false);
}

void FlyScoreDock::onTimeEdited()
{
	if (st_.timer.running)
		return;
	qint64 ms = parse_mmss_to_ms(time_->text());
	if (ms >= 0) {
		st_.timer.initial_ms = ms;
		st_.timer.remaining_ms = ms;
		saveState();
	} else {
		time_->setText(format_ms_mmss(st_.timer.remaining_ms));
	}
}

QString FlyScoreDock::normalizedExtFromMime(const QString &path) const
{
	QMimeDatabase db;
	auto mt = db.mimeTypeForFile(path);
	QString ext = QFileInfo(path).suffix().toLower();
	if (ext.isEmpty() || ext.size() > 5) {
		if (mt.name().contains("png"))
			ext = "png";
		else if (mt.name().contains("jpeg"))
			ext = "jpg";
		else if (mt.name().contains("svg"))
			ext = "svg";
		else if (mt.name().contains("webp"))
			ext = "webp";
	}
	if (ext == "jpeg")
		ext = "jpg";
	return ext.isEmpty() ? "png" : ext;
}

static QString shortHashOfFile(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return QString();
	QCryptographicHash h(QCryptographicHash::Sha256);
	while (!f.atEnd())
		h.addData(f.read(64 * 1024));
	return QString::fromLatin1(h.result().toHex().left(8));
}

QString FlyScoreDock::copyLogoToOverlay(const QString &srcAbs, const QString &baseName)
{
	if (srcAbs.isEmpty())
		return QString();

	const QString ovlDir = QDir(dataDir_).filePath("overlay");
	QDir().mkpath(ovlDir);

	{
		QDir d(ovlDir);
		const auto files = d.entryList(QStringList{QString("%1*.*").arg(baseName)}, QDir::Files);
		for (const auto &fn : files)
			QFile::remove(d.filePath(fn));
	}

	const QString ext = normalizedExtFromMime(srcAbs);
	const QString sh = shortHashOfFile(srcAbs);
	const QString rel = sh.isEmpty() ? QString("%1.%2").arg(baseName, ext)
					 : QString("%1-%2.%3").arg(baseName, sh, ext);
	const QString dst = QDir(ovlDir).filePath(rel);

	if (!QFile::copy(srcAbs, dst)) {
		LOGW("Failed to copy logo to overlay: %s -> %s", srcAbs.toUtf8().constData(), dst.toUtf8().constData());
		return QString();
	}
	return rel;
}

bool FlyScoreDock::deleteLogoIfExists(const QString &relPath)
{
	if (relPath.trimmed().isEmpty())
		return false;
	const QString abs = QDir(QDir(dataDir_).filePath("overlay")).filePath(relPath);
	if (QFile::exists(abs)) {
		if (!QFile::remove(abs)) {
			LOGW("Failed removing logo: %s", abs.toUtf8().constData());
			return false;
		}
		return true;
	}
	return false;
}

void FlyScoreDock::onBrowseHomeLogo()
{
	auto p = QFileDialog::getOpenFileName(this, "Select Home Logo", {}, "Images (*.png *.jpg *.jpeg *.webp *.svg)");
	if (p.isEmpty())
		return;
	const QString rel = copyLogoToOverlay(p, "home");
	if (!rel.isEmpty()) {
		homeLogo_->setText(rel);
		st_.home.logo = rel;
		saveState();
	}
}

void FlyScoreDock::onBrowseAwayLogo()
{
	auto p = QFileDialog::getOpenFileName(this, "Select Guests Logo", {},
					      "Images (*.png *.jpg *.jpeg *.webp *.svg)");
	if (p.isEmpty())
		return;
	const QString rel = copyLogoToOverlay(p, "guest");
	if (!rel.isEmpty()) {
		awayLogo_->setText(rel);
		st_.away.logo = rel;
		saveState();
	}
}

void FlyScoreDock::onApply()
{
	st_.time_label = time_label_->text();
	st_.home.title = homeTitle_->text();
	st_.home.subtitle = homeSub_->text();
	st_.away.title = awayTitle_->text();
	st_.away.subtitle = awaySub_->text();

	auto normalizeLogo = [this](const QString &txt, const char *who) -> QString {
		if (txt.startsWith("data:", Qt::CaseInsensitive))
			return txt;
		if (QFileInfo(txt).isAbsolute())
			return copyLogoToOverlay(txt, (QString(who) == "home") ? "home" : "guest");
		return txt;
	};

	st_.home.logo = normalizeLogo(homeLogo_->text(), "home");
	st_.away.logo = normalizeLogo(awayLogo_->text(), "guest");

	if (swapSides_)
		st_.swap_sides = swapSides_->isChecked();
	if (showScoreboard_)
		st_.show_scoreboard = showScoreboard_->isChecked();

	st_.home.score = homeScore_->value();
	st_.away.score = awayScore_->value();
	st_.home.rounds = homeRounds_->value();
	st_.away.rounds = awayRounds_->value();

	if (!st_.timer.running) {
		qint64 ms = parse_mmss_to_ms(time_->text());
		if (ms >= 0)
			st_.timer.remaining_ms = ms;
	}

	saveState();
	refreshUiFromState(false);
	refresh_browser_source_by_name(QString::fromUtf8(kBrowserSourceName));
}

void FlyScoreDock::onClearTeamsAndReset()
{
	auto rc = QMessageBox::question(this, "Reset scoreboard",
					"Clear teams, delete uploaded icons, and reset settings?");
	if (rc != QMessageBox::Yes)
		return;

	deleteLogoIfExists(st_.home.logo);
	deleteLogoIfExists(st_.away.logo);

	const QString ovlDir = QDir(dataDir_).filePath("overlay");
	for (const auto &base : {QStringLiteral("home"), QStringLiteral("guest")}) {
		QDir d(ovlDir);
		const auto files = d.entryList(QStringList{QString("%1*.*").arg(base)}, QDir::Files);
		for (const auto &fn : files)
			QFile::remove(d.filePath(fn));
	}

	if (!fly_state_reset_defaults(dataDir_)) {
		LOGW("Failed to reset plugin.json to defaults");
	}

	loadState();
	refreshUiFromState(false);

	refresh_browser_source_by_name(QString::fromUtf8(kBrowserSourceName));

	LOGI("Cleared teams, deleted icons, and reset plugin.json");
}

static inline int clampi(int v, int lo, int hi)
{
	return std::max(lo, std::min(v, hi));
}

void FlyScoreDock::bumpHomeScore(int delta)
{
	int nv = clampi((homeScore_ ? homeScore_->value() : st_.home.score) + delta, 0, 999);
	if (homeScore_)
		homeScore_->setValue(nv);
	st_.home.score = nv;
	saveState();
}

void FlyScoreDock::bumpAwayScore(int delta)
{
	int nv = clampi((awayScore_ ? awayScore_->value() : st_.away.score) + delta, 0, 999);
	if (awayScore_)
		awayScore_->setValue(nv);
	st_.away.score = nv;
	saveState();
}

void FlyScoreDock::bumpHomeRounds(int delta)
{
	int nv = clampi((homeRounds_ ? homeRounds_->value() : st_.home.rounds) + delta, 0, 99);
	if (homeRounds_)
		homeRounds_->setValue(nv);
	st_.home.rounds = nv;
	saveState();
}

void FlyScoreDock::bumpAwayRounds(int delta)
{
	int nv = clampi((awayRounds_ ? awayRounds_->value() : st_.away.rounds) + delta, 0, 99);
	if (awayRounds_)
		awayRounds_->setValue(nv);
	st_.away.rounds = nv;
	saveState();
}

void FlyScoreDock::toggleSwap()
{
	bool nv = !(swapSides_ ? swapSides_->isChecked() : st_.swap_sides);
	if (swapSides_)
		swapSides_->setChecked(nv);
	st_.swap_sides = nv;
	saveState();
}

void FlyScoreDock::toggleShow()
{
	bool nv = !(showScoreboard_ ? showScoreboard_->isChecked() : st_.show_scoreboard);
	if (showScoreboard_)
		showScoreboard_->setChecked(nv);
	st_.show_scoreboard = nv;
	saveState();
}

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
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	obs_frontend_add_dock(panel);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

	g_dockContent = panel;
}

void fly_destroy_dock()
{
	if (!g_dockContent)
		return;

#if defined(HAVE_OBS_DOCK_BY_ID)
	obs_frontend_remove_dock(kFlyDockId);
#elif defined(__linux__)
	QWidget *w = g_dockContent;
	w->setParent(nullptr);
	w->hide();
	w->deleteLater();
#else
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	obs_frontend_remove_dock(g_dockContent);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

	g_dockContent = nullptr;
}

FlyScoreDock *fly_get_dock()
{
	return qobject_cast<FlyScoreDock *>(g_dockContent);
}
