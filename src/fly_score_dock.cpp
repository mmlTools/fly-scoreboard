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

#include <obs.h>
#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QFrame>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QStyle>
#include <QAbstractButton>
#include <QCheckBox>
#include <QIcon>
#include <QSize>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QMessageBox>

#include <algorithm>

// -----------------------------------------------------------------------------
// FlyScoreDock implementation
// -----------------------------------------------------------------------------

FlyScoreDock::FlyScoreDock(QWidget *parent)
    : QWidget(parent)
{
    // Make it comfortable to dock full-height
    QSizePolicy sp = sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Preferred);
    sp.setVerticalPolicy(QSizePolicy::Expanding);
    setSizePolicy(sp);
}

bool FlyScoreDock::init()
{
    // Resolve global data root (auto-default on first run, no UI)
    dataDir_ = fly_get_data_root();
    loadState();

    auto root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ---------------------------------------------------------------------
    // Time label row
    // ---------------------------------------------------------------------
    auto row1 = new QHBoxLayout();
    time_label_ = new QLineEdit(this);
    time_label_->setText(st_.time_label);
    row1->addWidget(new QLabel(QStringLiteral("Time Label:"), this));
    row1->addWidget(time_label_, 1);

    // ---------------------------------------------------------------------
    // Timer box (only sets initial json values; overlay JS animates)
    // ---------------------------------------------------------------------
    auto timerBox = new QGroupBox(QStringLiteral("Timer"), this);
    auto timerGrid = new QGridLayout(timerBox);
    timerGrid->setContentsMargins(8, 8, 8, 8);
    timerGrid->setHorizontalSpacing(8);

    time_ = new QLineEdit(timerBox);
    time_->setPlaceholderText(QStringLiteral("mm:ss"));
    time_->setClearButtonEnabled(true);
    time_->setText(fly_format_ms_mmss(st_.timer.remaining_ms));
    time_->setMinimumWidth(70);
    time_->setMaxLength(8);

    mode_ = new QComboBox(timerBox);
    mode_->addItems({QStringLiteral("countdown"), QStringLiteral("countup")});
    if (st_.timer.mode.isEmpty())
        st_.timer.mode = QStringLiteral("countdown");
    mode_->setCurrentText(st_.timer.mode);

    startStop_ = new QPushButton(timerBox);
    reset_     = new QPushButton(timerBox);
    {
        const QIcon playIcon  = fly_themed_icon(this, "media-playback-start", QStyle::SP_MediaPlay);
        const QIcon pauseIcon = fly_themed_icon(this, "media-playback-pause", QStyle::SP_MediaPause);
        const QIcon resetIcon = fly_themed_icon(this, "view-refresh", QStyle::SP_BrowserReload);
        fly_style_icon_only_button(startStop_,
                                   st_.timer.running ? pauseIcon : playIcon,
                                   st_.timer.running ? QStringLiteral("Pause timer")
                                                     : QStringLiteral("Start timer"));
        fly_style_icon_only_button(reset_,
                                   resetIcon,
                                   QStringLiteral("Reset timer"));
    }

    int r = 0, c = 0;
    timerGrid->addWidget(new QLabel(QStringLiteral("Time:"), timerBox), r, c++);
    timerGrid->addWidget(time_, r, c++);
    timerGrid->addWidget(new QLabel(QStringLiteral("Type:"), timerBox), r, c++);
    timerGrid->addWidget(mode_, r, c++);
    timerGrid->addWidget(startStop_, r, c++);
    timerGrid->addWidget(reset_, r, c++);
    timerGrid->setColumnStretch(c, 1);

    // ---------------------------------------------------------------------
    // Scoreboard box
    // ---------------------------------------------------------------------
    auto scoreBox = new QGroupBox(QStringLiteral("Scoreboard"), this);
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
    scoreGrid->addWidget(new QLabel(QStringLiteral("Home:"), scoreBox), srow, 0, Qt::AlignRight);
    scoreGrid->addWidget(homeScore_, srow, 1);
    scoreGrid->addItem(new QSpacerItem(20, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), srow, 2);
    scoreGrid->addWidget(new QLabel(QStringLiteral("Guests:"), scoreBox), srow, 3, Qt::AlignRight);
    scoreGrid->addWidget(awayScore_, srow, 4);

    srow++;
    scoreGrid->addWidget(new QLabel(QStringLiteral("Home Wins:"), scoreBox), srow, 0, Qt::AlignRight);
    scoreGrid->addWidget(homeRounds_, srow, 1);
    scoreGrid->addItem(new QSpacerItem(20, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), srow, 2);
    scoreGrid->addWidget(new QLabel(QStringLiteral("Guests Wins:"), scoreBox), srow, 3, Qt::AlignRight);
    scoreGrid->addWidget(awayRounds_, srow, 4);
    scoreGrid->setColumnStretch(2, 1);

    homeScore_->setMaximumWidth(80);
    awayScore_->setMaximumWidth(80);
    homeRounds_->setMaximumWidth(80);
    awayRounds_->setMaximumWidth(80);

    auto toggleRow = new QHBoxLayout();
    swapSides_      = new QCheckBox(QStringLiteral("Swap Home ↔ Guests"), scoreBox);
    showScoreboard_ = new QCheckBox(QStringLiteral("Show Scoreboard"), scoreBox);
    showRounds_     = new QCheckBox(QStringLiteral("Show Wins"), scoreBox);
    toggleRow->addWidget(swapSides_);
    toggleRow->addSpacing(12);
    toggleRow->addWidget(showScoreboard_);
    toggleRow->addSpacing(12);
    toggleRow->addWidget(showRounds_);
    toggleRow->addStretch(1);

    auto scoreVBox = new QVBoxLayout(scoreBox);
    scoreVBox->setContentsMargins(8, 8, 8, 8);
    scoreVBox->setSpacing(6);
    scoreVBox->addLayout(scoreGrid);
    scoreVBox->addLayout(toggleRow);
    scoreBox->setLayout(scoreVBox);

    // ---------------------------------------------------------------------
    // Custom stats box (inside dock)
    // ---------------------------------------------------------------------
    auto customBox  = new QGroupBox(QStringLiteral("Custom stats (corners, penalties, etc.)"), this);
    auto customVBox = new QVBoxLayout(customBox);
    customVBox->setContentsMargins(8, 8, 8, 8);
    customVBox->setSpacing(6);

    auto *hintLbl = new QLabel(
        QStringLiteral("Add any number of custom numeric fields.\n"
                       "Example labels: Corners, Penalties, Shots on goal, etc."),
        customBox);
    hintLbl->setWordWrap(true);
    customVBox->addWidget(hintLbl);

    customFieldsLayout_ = new QVBoxLayout();
    customFieldsLayout_->setContentsMargins(0, 0, 0, 0);
    customFieldsLayout_->setSpacing(4);
    customVBox->addLayout(customFieldsLayout_);

    addFieldBtn_ = new QPushButton(QStringLiteral("➕ Add custom field"), customBox);
    addFieldBtn_->setCursor(Qt::PointingHandCursor);
    customVBox->addWidget(addFieldBtn_, 0, Qt::AlignLeft);

    customBox->setLayout(customVBox);

    // ---------------------------------------------------------------------
    // Bottom row buttons
    // ---------------------------------------------------------------------
    auto bottomRow = new QHBoxLayout();

    auto *refreshBrowserBtn = new QPushButton(this);
    {
        const QIcon refIcon = fly_themed_icon(this, "view-refresh", QStyle::SP_BrowserReload);
        fly_style_icon_only_button(
            refreshBrowserBtn, refIcon,
            QStringLiteral("Refresh ‘%1’").arg(kBrowserSourceName));
    }

    auto *addSourceBtn = new QPushButton(this);
    {
        const QIcon webIcon = QIcon::fromTheme(QStringLiteral("internet-web-browser"));
        const QIcon fbIcon  = this->style()->standardIcon(QStyle::SP_ComputerIcon);
        fly_style_icon_only_button(
            addSourceBtn,
            webIcon.isNull() ? fbIcon : webIcon,
            QStringLiteral("Add scoreboard Browser Source to current scene"));
    }

    auto *clearBtn = new QPushButton(this);
    {
        const QIcon delIcon = fly_themed_icon(this, "edit-clear", QStyle::SP_TrashIcon);
        fly_style_icon_only_button(clearBtn, delIcon,
                                   QStringLiteral("Clear teams, delete icons, reset settings"));
    }

    teamsBtn_  = new QPushButton(this);
    applyBtn_  = new QPushButton(this);
    auto *settingsBtn = new QPushButton(this);
    {
        const QIcon teamIcon = QIcon::fromTheme(QStringLiteral("user-group"));
        const QIcon teamFb   = this->style()->standardIcon(QStyle::SP_FileDialogInfoView);
        fly_style_icon_only_button(
            teamsBtn_,
            teamIcon.isNull() ? teamFb : teamIcon,
            QStringLiteral("Edit teams (names & logos)"));

        const QIcon saveIcon     = fly_themed_icon(this, "document-save", QStyle::SP_DialogSaveButton);
        const QIcon settingsIcon = QIcon::fromTheme(QStringLiteral("preferences-system"));
        const QIcon settingsFb   = this->style()->standardIcon(QStyle::SP_FileDialogDetailedView);

        fly_style_icon_only_button(applyBtn_, saveIcon,
                                   QStringLiteral("Save / apply changes"));
        fly_style_icon_only_button(settingsBtn,
                                   settingsIcon.isNull() ? settingsFb : settingsIcon,
                                   QStringLiteral("Settings"));
    }

    bottomRow->addWidget(refreshBrowserBtn);
    bottomRow->addWidget(addSourceBtn);
    bottomRow->addWidget(clearBtn);
    bottomRow->addStretch(1);
    bottomRow->addWidget(teamsBtn_);
    bottomRow->addWidget(applyBtn_);
    bottomRow->addWidget(settingsBtn);

    // ---------------------------------------------------------------------
    // Layout assembly
    // ---------------------------------------------------------------------
    root->addLayout(row1);
    root->addWidget(timerBox);
    root->addWidget(scoreBox);
    root->addWidget(customBox);
    root->addLayout(bottomRow);
    root->addStretch(1);

    // Ko-fi card
    root->addWidget(fly_create_kofi_card(this));

    // Create custom fields from state
    loadCustomFieldsFromState();

    // ---------------------------------------------------------------------
    // Connections for values -> state
    // ---------------------------------------------------------------------
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
    connect(showRounds_, &QCheckBox::toggled, this, [this](bool on) {
        st_.show_rounds = on;
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
    connect(applyBtn_, &QPushButton::clicked, this, &FlyScoreDock::onApply);

    connect(refreshBrowserBtn, &QPushButton::clicked, this, []() {
        const bool ok = fly_refresh_browser_source_by_name(
            QString::fromUtf8(kBrowserSourceName));
        if (!ok)
            LOGW("Refresh failed for Browser Source: %s", kBrowserSourceName);
    });

    // Add browser source only when user explicitly clicks the button
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

        // Create a Browser source with the scoreboard URL
        QString url = QStringLiteral("http://127.0.0.1:%1/").arg(st_.server_port);

        obs_data_t *settings = obs_data_create();
        obs_data_set_string(settings, "url", url.toUtf8().constData());
        obs_data_set_bool(settings, "is_local_file", false);
        obs_data_set_int(settings, "width", 1920);
        obs_data_set_int(settings, "height", 1080);

        obs_source_t *browser = obs_source_create("browser_source",
                                                  kBrowserSourceName,
                                                  settings,
                                                  nullptr);
        obs_data_release(settings);

        if (!browser) {
            LOGW("Failed to create browser source");
            obs_source_release(sceneSource);
            return;
        }

        obs_sceneitem_t *item = obs_scene_add(scene, browser);
        if (!item) {
            LOGW("Failed to add browser source to scene");
        }

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
    connect(addFieldBtn_, &QPushButton::clicked, this, &FlyScoreDock::onAddCustomField);
    connect(teamsBtn_, &QPushButton::clicked, this, &FlyScoreDock::onOpenTeamsDialog);

    refreshUiFromState(false);
    return true;
}

void FlyScoreDock::loadState()
{
    if (!fly_state_load(dataDir_, st_)) {
        st_ = fly_state_make_defaults();
        fly_state_save(dataDir_, st_);
    }

    if (st_.timer.mode.isEmpty())
        st_.timer.mode = QStringLiteral("countdown");
}

void FlyScoreDock::saveState()
{
    fly_state_save(dataDir_, st_);
}

void FlyScoreDock::refreshUiFromState(bool onlyTimeIfRunning)
{
    Q_UNUSED(onlyTimeIfRunning);

    if (time_ && !time_->hasFocus())
        time_->setText(fly_format_ms_mmss(st_.timer.remaining_ms));

    if (startStop_) {
        const QIcon playIcon  = fly_themed_icon(this, "media-playback-start", QStyle::SP_MediaPlay);
        const QIcon pauseIcon = fly_themed_icon(this, "media-playback-pause", QStyle::SP_MediaPause);
        fly_style_icon_only_button(startStop_,
                                   st_.timer.running ? pauseIcon : playIcon,
                                   st_.timer.running ? QStringLiteral("Pause timer")
                                                     : QStringLiteral("Start timer"));
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
    if (showRounds_ && showRounds_->isChecked() != st_.show_rounds)
        showRounds_->setChecked(st_.show_rounds);

    if (time_label_ && !time_label_->hasFocus() && time_label_->text() != st_.time_label)
        time_label_->setText(st_.time_label);
}

void FlyScoreDock::onStartStop()
{
    if (mode_) {
        const QString uiMode = mode_->currentText();
        if (uiMode != st_.timer.mode) {
            st_.timer.mode = uiMode;
        }
    }

    if (!st_.timer.running) {
        qint64 ms = fly_parse_mmss_to_ms(time_->text());
        if (st_.timer.mode == QStringLiteral("countdown")) {
            if (ms < 0)
                ms = (st_.timer.remaining_ms > 0 ? st_.timer.remaining_ms : 0);
            st_.timer.initial_ms   = ms;
            st_.timer.remaining_ms = ms;
        } else {
            if (ms < 0)
                ms = 0;
            st_.timer.initial_ms   = ms;
            st_.timer.remaining_ms = ms;
        }
        st_.timer.last_tick_ms = fly_now_ms();
        st_.timer.running      = true;
    } else {
        // Just mark as stopped; overlay JS handles visual ticking.
        st_.timer.running = false;
    }

    saveState();
    refreshUiFromState(false);
}

void FlyScoreDock::onReset()
{
    if (mode_) {
        const QString uiMode = mode_->currentText();
        if (uiMode != st_.timer.mode)
            st_.timer.mode = uiMode;
    }

    qint64 ms = fly_parse_mmss_to_ms(time_->text());
    if (st_.timer.mode == QStringLiteral("countdown")) {
        if (ms < 0)
            ms = st_.timer.initial_ms;
    } else {
        if (ms < 0)
            ms = 0;
    }
    st_.timer.initial_ms   = ms;
    st_.timer.remaining_ms = ms;
    st_.timer.running      = false;

    saveState();
    refreshUiFromState(false);
}

void FlyScoreDock::onTimeEdited()
{
    if (st_.timer.running)
        return;

    qint64 ms = fly_parse_mmss_to_ms(time_->text());
    if (ms >= 0) {
        st_.timer.initial_ms   = ms;
        st_.timer.remaining_ms = ms;
        saveState();
    } else {
        time_->setText(fly_format_ms_mmss(st_.timer.remaining_ms));
    }
}

void FlyScoreDock::onApply()
{
    if (time_label_)
        st_.time_label = time_label_->text();

    if (swapSides_)
        st_.swap_sides = swapSides_->isChecked();
    if (showScoreboard_)
        st_.show_scoreboard = showScoreboard_->isChecked();
    if (showRounds_)
        st_.show_rounds = showRounds_->isChecked();

    if (homeScore_)
        st_.home.score = homeScore_->value();
    if (awayScore_)
        st_.away.score = awayScore_->value();
    if (homeRounds_)
        st_.home.rounds = homeRounds_->value();
    if (awayRounds_)
        st_.away.rounds = awayRounds_->value();

    if (!st_.timer.running && time_) {
        qint64 ms = fly_parse_mmss_to_ms(time_->text());
        if (ms >= 0)
            st_.timer.remaining_ms = ms;
    }

    // Save main state + custom fields (this also saves to disk)
    saveCustomFieldsToState();
    refreshUiFromState(false);
}

void FlyScoreDock::onClearTeamsAndReset()
{
    auto rc = QMessageBox::question(
        this, QStringLiteral("Reset scoreboard"),
        QStringLiteral("Clear teams, delete uploaded icons, reset settings and custom stats?"));
    if (rc != QMessageBox::Yes)
        return;

    fly_delete_logo_if_exists(dataDir_, st_.home.logo);
    fly_delete_logo_if_exists(dataDir_, st_.away.logo);

    for (const auto &base : {QStringLiteral("home"), QStringLiteral("guest")})
        fly_clean_overlay_prefix(dataDir_, base);

    if (!fly_state_reset_defaults(dataDir_)) {
        LOGW("Failed to reset plugin.json to defaults");
    }

    clearAllCustomFieldRows();
    st_.custom_fields.clear();

    loadState();
    refreshUiFromState(false);

    LOGI("Cleared teams, deleted icons, reset plugin.json and custom stats");
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

// -----------------------------------------------------------------------------
// Custom fields (inside dock, wired to FlyState)
// -----------------------------------------------------------------------------

FlyCustomFieldUi FlyScoreDock::addCustomFieldRow(const QString &label,
                                                 int home,
                                                 int away,
                                                 bool visible)
{
    FlyCustomFieldUi ui;

    auto *row = new QWidget(this);
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto *visibleCheck = new QCheckBox(row);
    visibleCheck->setChecked(visible);
    visibleCheck->setToolTip(QStringLiteral("Show this field on overlay"));

    auto *labelEdit = new QLineEdit(row);
    labelEdit->setPlaceholderText(QStringLiteral("Label (e.g. Corners)"));
    labelEdit->setText(label);

    auto *homeSpin = new QSpinBox(row);
    homeSpin->setRange(0, 999);
    homeSpin->setValue(std::max(0, home));
    homeSpin->setMaximumWidth(70);

    auto *awaySpin = new QSpinBox(row);
    awaySpin->setRange(0, 999);
    awaySpin->setValue(std::max(0, away));
    awaySpin->setMaximumWidth(70);

    auto *removeBtn = new QPushButton(row);
    const QIcon delIcon = fly_themed_icon(this, "edit-delete", QStyle::SP_TrashIcon);
    fly_style_icon_only_button(removeBtn, delIcon,
                               QStringLiteral("Remove this custom field"));
    removeBtn->setFixedWidth(28);

    lay->addWidget(visibleCheck);
    lay->addWidget(labelEdit, 1);
    lay->addWidget(new QLabel(QStringLiteral("Home:"), row));
    lay->addWidget(homeSpin);
    lay->addWidget(new QLabel(QStringLiteral("Guests:"), row));
    lay->addWidget(awaySpin);
    lay->addWidget(removeBtn);

    row->setLayout(lay);
    if (customFieldsLayout_)
        customFieldsLayout_->addWidget(row);

    ui.row          = row;
    ui.labelEdit    = labelEdit;
    ui.homeSpin     = homeSpin;
    ui.awaySpin     = awaySpin;
    ui.visibleCheck = visibleCheck;
    ui.removeBtn    = removeBtn;

    auto triggerSave = [this]() { saveCustomFieldsToState(); };

    connect(labelEdit, &QLineEdit::textEdited, this, [triggerSave](const QString &) {
        triggerSave();
    });
    connect(homeSpin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [triggerSave](int) { triggerSave(); });
    connect(awaySpin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [triggerSave](int) { triggerSave(); });
    connect(visibleCheck,
            &QCheckBox::toggled,
            this,
            [triggerSave](bool) { triggerSave(); });

    connect(removeBtn, &QPushButton::clicked, this, [this, row]() {
        for (int i = 0; i < customFields_.size(); ++i) {
            if (customFields_[i].row == row) {
                auto ui = customFields_[i];
                customFields_.removeAt(i);
                if (customFieldsLayout_) {
                    customFieldsLayout_->removeWidget(ui.row);
                }
                ui.row->deleteLater();
                break;
            }
        }
        saveCustomFieldsToState();
    });

    return ui;
}

void FlyScoreDock::onAddCustomField()
{
    FlyCustomFieldUi ui = addCustomFieldRow(QString(), 0, 0, true);
    customFields_.push_back(ui);
    saveCustomFieldsToState();
}

void FlyScoreDock::clearAllCustomFieldRows()
{
    for (auto &ui : customFields_) {
        if (customFieldsLayout_ && ui.row)
            customFieldsLayout_->removeWidget(ui.row);
        if (ui.row)
            ui.row->deleteLater();
    }
    customFields_.clear();
}

void FlyScoreDock::loadCustomFieldsFromState()
{
    clearAllCustomFieldRows();

    customFields_.reserve(st_.custom_fields.size());
    for (const auto &cf : st_.custom_fields) {
        FlyCustomFieldUi ui = addCustomFieldRow(cf.label, cf.home, cf.away, cf.visible);
        customFields_.push_back(ui);
    }
}

void FlyScoreDock::saveCustomFieldsToState()
{
    st_.custom_fields.clear();
    st_.custom_fields.reserve(customFields_.size());

    for (const auto &ui : customFields_) {
        FlyCustomField cf;
        cf.label   = ui.labelEdit ? ui.labelEdit->text() : QString();
        cf.home    = ui.homeSpin ? ui.homeSpin->value() : 0;
        cf.away    = ui.awaySpin ? ui.awaySpin->value() : 0;
        cf.visible = ui.visibleCheck ? ui.visibleCheck->isChecked() : true;
        st_.custom_fields.push_back(cf);
    }

    saveState(); // writes plugin.json
}

// -----------------------------------------------------------------------------
// Teams dialog
// -----------------------------------------------------------------------------

void FlyScoreDock::onOpenTeamsDialog()
{
    FlyTeamsDialog dlg(dataDir_, st_, this);
    dlg.exec();

    // Reload state from disk in case other fields were touched
    loadState();
    refreshUiFromState(false);
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
