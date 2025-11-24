#include "fly_score_teams_dialog.hpp"
#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][teams-dialog]"
#include "fly_score_log.hpp"

#include "fly_score_logo_helpers.hpp"
#include "fly_score_qt_helpers.hpp"
#include "fly_score_state.hpp"

#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStyle>
#include <QColorDialog>

static QColor colorFromU32(uint32_t c)
{
    // Treat stored value as 0xRRGGBB
    int r = (c >> 16) & 0xFF;
    int g = (c >> 8) & 0xFF;
    int b = (c >> 0) & 0xFF;
    return QColor(r, g, b);
}

static uint32_t u32FromColor(const QColor &qc)
{
    uint32_t r = (uint32_t)qc.red()   & 0xFF;
    uint32_t g = (uint32_t)qc.green() & 0xFF;
    uint32_t b = (uint32_t)qc.blue()  & 0xFF;
    return (r << 16) | (g << 8) | b;
}

FlyTeamsDialog::FlyTeamsDialog(const QString &dataDir, FlyState &state, QWidget *parent)
	: QDialog(parent),
	  dataDir_(dataDir),
	  state_(state)
{
	setObjectName(QStringLiteral("FlyTeamsDialog"));
	setWindowTitle(QStringLiteral("Fly Scoreboard – Teams & logos"));
	setModal(false);

	setMinimumWidth(480);
	resize(520, 420);
	setSizeGripEnabled(false);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	auto *hint = new QLabel(QStringLiteral(
        "Configure display names, colors and logos for the home and guests teams.\n"
		"Logos should be paths relative to the overlay folder."),
		this);
	hint->setObjectName(QStringLiteral("teamsHint"));
	hint->setWordWrap(true);
	root->addWidget(hint);

	// -------------------------
	// Home group
	// -------------------------
	auto *gbHome = new QGroupBox(QStringLiteral("Home team"), this);
	gbHome->setObjectName(QStringLiteral("homeGroup"));

	auto *hb = new QGridLayout(gbHome);
	hb->setContentsMargins(10, 10, 10, 10);
	hb->setHorizontalSpacing(8);
	hb->setVerticalSpacing(6);

	homeTitle_ = new QLineEdit(gbHome);
	homeSub_   = new QLineEdit(gbHome);
	homeLogo_  = new QLineEdit(gbHome);
	homeBrowse_= new QToolButton(gbHome);
	homeColor_ = new QToolButton(gbHome); // NEW
	{
		const QIcon openIcon = fly_themed_icon(this, "folder-open", QStyle::SP_DirOpenIcon);
		homeBrowse_->setText(QString());
		homeBrowse_->setIcon(openIcon);
		homeBrowse_->setToolTip(QStringLiteral("Browse logo…"));
		homeBrowse_->setAutoRaise(false);
		homeBrowse_->setCursor(Qt::PointingHandCursor);

        homeColor_->setText(QStringLiteral("#FFFFFF")); // will be updated
        homeColor_->setToolTip(QStringLiteral("Pick team color…"));
        homeColor_->setAutoRaise(false);
        homeColor_->setCursor(Qt::PointingHandCursor);
        homeColor_->setMinimumWidth(90);
	}

	hb->addWidget(new QLabel(QStringLiteral("Title:"), gbHome), 0, 0);
	hb->addWidget(homeTitle_, 0, 1, 1, 2);

	hb->addWidget(new QLabel(QStringLiteral("Subtitle:"), gbHome), 1, 0);
	hb->addWidget(homeSub_, 1, 1, 1, 2);

	hb->addWidget(new QLabel(QStringLiteral("Logo:"), gbHome), 2, 0);
	hb->addWidget(homeLogo_, 2, 1);
	hb->addWidget(homeBrowse_, 2, 2);

    // NEW row: color picker
    hb->addWidget(new QLabel(QStringLiteral("Color:"), gbHome), 3, 0);
    hb->addWidget(homeColor_, 3, 1, 1, 2);

	gbHome->setLayout(hb);

	// -------------------------
	// Away group
	// -------------------------
	auto *gbAway = new QGroupBox(QStringLiteral("Guests team"), this);
	gbAway->setObjectName(QStringLiteral("awayGroup"));

	auto *ab = new QGridLayout(gbAway);
	ab->setContentsMargins(10, 10, 10, 10);
	ab->setHorizontalSpacing(8);
	ab->setVerticalSpacing(6);

	awayTitle_ = new QLineEdit(gbAway);
	awaySub_   = new QLineEdit(gbAway);
	awayLogo_  = new QLineEdit(gbAway);
	awayBrowse_= new QToolButton(gbAway);
	awayColor_ = new QToolButton(gbAway); // NEW
	{
		const QIcon openIcon = fly_themed_icon(this, "folder-open", QStyle::SP_DirOpenIcon);
		awayBrowse_->setText(QString());
		awayBrowse_->setIcon(openIcon);
		awayBrowse_->setToolTip(QStringLiteral("Browse logo…"));
		awayBrowse_->setAutoRaise(false);
		awayBrowse_->setCursor(Qt::PointingHandCursor);

        awayColor_->setText(QStringLiteral("#FFFFFF")); // will be updated
        awayColor_->setToolTip(QStringLiteral("Pick team color…"));
        awayColor_->setAutoRaise(false);
        awayColor_->setCursor(Qt::PointingHandCursor);
        awayColor_->setMinimumWidth(90);
	}

	ab->addWidget(new QLabel(QStringLiteral("Title:"), gbAway), 0, 0);
	ab->addWidget(awayTitle_, 0, 1, 1, 2);

	ab->addWidget(new QLabel(QStringLiteral("Subtitle:"), gbAway), 1, 0);
	ab->addWidget(awaySub_, 1, 1, 1, 2);

	ab->addWidget(new QLabel(QStringLiteral("Logo:"), gbAway), 2, 0);
	ab->addWidget(awayLogo_, 2, 1);
	ab->addWidget(awayBrowse_, 2, 2);

    // NEW row: color picker
    ab->addWidget(new QLabel(QStringLiteral("Color:"), gbAway), 3, 0);
    ab->addWidget(awayColor_, 3, 1, 1, 2);

	gbAway->setLayout(ab);

	// -------------------------
	// Buttons
	// -------------------------
	auto *buttonsRow = new QHBoxLayout();
	buttonsRow->setContentsMargins(0, 0, 0, 0);
	buttonsRow->setSpacing(8);

	buttonsRow->addStretch(1);

	applyBtn_ = new QPushButton(QStringLiteral("Save && Close"), this);
	auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);

	applyBtn_->setCursor(Qt::PointingHandCursor);
	closeBtn->setCursor(Qt::PointingHandCursor);

	buttonsRow->addWidget(applyBtn_);
	buttonsRow->addWidget(closeBtn);

	root->addWidget(gbHome);
	root->addWidget(gbAway);
	root->addLayout(buttonsRow);
	setLayout(root);

	syncUiFromState();

	connect(homeBrowse_, &QToolButton::clicked, this, &FlyTeamsDialog::onBrowseHomeLogo);
	connect(awayBrowse_, &QToolButton::clicked, this, &FlyTeamsDialog::onBrowseAwayLogo);

    // NEW connects
    connect(homeColor_, &QToolButton::clicked, this, &FlyTeamsDialog::onPickHomeColor);
    connect(awayColor_, &QToolButton::clicked, this, &FlyTeamsDialog::onPickAwayColor);

	connect(applyBtn_, &QPushButton::clicked, this, &FlyTeamsDialog::onApply);
	connect(closeBtn, &QPushButton::clicked, this, &FlyTeamsDialog::close);
}

FlyTeamsDialog::~FlyTeamsDialog() = default;

void FlyTeamsDialog::updateColorButton(QToolButton *btn, uint32_t color)
{
    if (!btn) return;

    QColor qc = colorFromU32(color);
    const QString hex = qc.name(QColor::HexRgb).toUpper(); // "#RRGGBB"

    btn->setText(hex);

    // Small swatch background
    btn->setStyleSheet(QString(
        "QToolButton {"
        "  padding: 6px 8px;"
        "  border: 1px solid rgba(255,255,255,0.15);"
        "  border-radius: 4px;"
        "  background: %1;"
        "  color: %2;"
        "}"
    ).arg(hex)
     .arg(qc.lightness() > 140 ? "#000000" : "#FFFFFF"));
}

void FlyTeamsDialog::syncUiFromState()
{
	if (homeTitle_)
		homeTitle_->setText(state_.home.title);
	if (homeSub_)
		homeSub_->setText(state_.home.subtitle);
	if (homeLogo_)
		homeLogo_->setText(state_.home.logo);

    updateColorButton(homeColor_, state_.home.color);

	if (awayTitle_)
		awayTitle_->setText(state_.away.title);
	if (awaySub_)
		awaySub_->setText(state_.away.subtitle);
	if (awayLogo_)
		awayLogo_->setText(state_.away.logo);

    updateColorButton(awayColor_, state_.away.color);
}

void FlyTeamsDialog::syncStateFromUi()
{
	if (homeTitle_)
		state_.home.title = homeTitle_->text();
	if (homeSub_)
		state_.home.subtitle = homeSub_->text();
	if (homeLogo_)
		state_.home.logo = homeLogo_->text();

    // colors are already set when picked

	if (awayTitle_)
		state_.away.title = awayTitle_->text();
	if (awaySub_)
		state_.away.subtitle = awaySub_->text();
	if (awayLogo_)
		state_.away.logo = awayLogo_->text();

    // colors are already set when picked
}

void FlyTeamsDialog::onPickHomeColor()
{
    QColor start = colorFromU32(state_.home.color);
    QColor chosen = QColorDialog::getColor(
        start, this, QStringLiteral("Pick Home Team Color"),
        QColorDialog::ShowAlphaChannel /* harmless even if not used */
    );

    if (!chosen.isValid())
        return;

    state_.home.color = u32FromColor(chosen);
    updateColorButton(homeColor_, state_.home.color);
    fly_state_save(dataDir_, state_);
    LOGI("Home color updated: %u", state_.home.color);
}

void FlyTeamsDialog::onPickAwayColor()
{
    QColor start = colorFromU32(state_.away.color);
    QColor chosen = QColorDialog::getColor(
        start, this, QStringLiteral("Pick Guests Team Color"),
        QColorDialog::ShowAlphaChannel
    );

    if (!chosen.isValid())
        return;

    state_.away.color = u32FromColor(chosen);
    updateColorButton(awayColor_, state_.away.color);
    fly_state_save(dataDir_, state_);
    LOGI("Guests color updated: %u", state_.away.color);
}

void FlyTeamsDialog::onBrowseHomeLogo()
{
	const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("Select Home Logo"), {},
						       QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.svg)"));
	if (p.isEmpty())
		return;

	const QString rel = fly_copy_logo_to_overlay(dataDir_, p, QStringLiteral("home"));
	if (rel.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("Fly Score Teams"),
				     QStringLiteral("Failed to copy logo to overlay folder."));
		return;
	}

	if (homeLogo_)
		homeLogo_->setText(rel);
	state_.home.logo = rel;

	fly_state_save(dataDir_, state_);

	LOGI("Home logo updated: %s", rel.toUtf8().constData());
}

void FlyTeamsDialog::onBrowseAwayLogo()
{
	const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("Select Guests Logo"), {},
						       QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.svg)"));
	if (p.isEmpty())
		return;

	const QString rel = fly_copy_logo_to_overlay(dataDir_, p, QStringLiteral("guest"));
	if (rel.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("Fly Score Teams"),
				     QStringLiteral("Failed to copy logo to overlay folder."));
		return;
	}

	if (awayLogo_)
		awayLogo_->setText(rel);
	state_.away.logo = rel;

	fly_state_save(dataDir_, state_);

	LOGI("Guests logo updated: %s", rel.toUtf8().constData());
}

void FlyTeamsDialog::onApply()
{
	syncStateFromUi();
	fly_state_save(dataDir_, state_);
	LOGI("Teams dialog: titles/subtitles/logos/colors saved.");
	accept();
}
