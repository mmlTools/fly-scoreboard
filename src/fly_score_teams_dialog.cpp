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

FlyTeamsDialog::FlyTeamsDialog(const QString &dataDir, FlyState &state, QWidget *parent)
	: QDialog(parent),
	  dataDir_(dataDir),
	  state_(state)
{
	setObjectName(QStringLiteral("FlyTeamsDialog"));
	setWindowTitle(QStringLiteral("Fly Scoreboard – Teams & logos"));
	setModal(false);

	setMinimumWidth(480);
	resize(520, 380);
	setSizeGripEnabled(false);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	auto *hint = new QLabel(QStringLiteral("Configure display names and logos for the home and guests teams.\n"
					       "Logos should be paths relative to the overlay folder."),
				this);
	hint->setObjectName(QStringLiteral("teamsHint"));
	hint->setWordWrap(true);
	root->addWidget(hint);

	auto *gbHome = new QGroupBox(QStringLiteral("Home team"), this);
	gbHome->setObjectName(QStringLiteral("homeGroup"));

	auto *hb = new QGridLayout(gbHome);
	hb->setContentsMargins(10, 10, 10, 10);
	hb->setHorizontalSpacing(8);
	hb->setVerticalSpacing(6);

	homeTitle_ = new QLineEdit(gbHome);
	homeSub_ = new QLineEdit(gbHome);
	homeLogo_ = new QLineEdit(gbHome);
	homeBrowse_ = new QToolButton(gbHome);
	{
		const QIcon openIcon = fly_themed_icon(this, "folder-open", QStyle::SP_DirOpenIcon);
		homeBrowse_->setText(QString());
		homeBrowse_->setIcon(openIcon);
		homeBrowse_->setToolTip(QStringLiteral("Browse logo…"));
		homeBrowse_->setAutoRaise(false);
		homeBrowse_->setCursor(Qt::PointingHandCursor);
	}

	hb->addWidget(new QLabel(QStringLiteral("Title:"), gbHome), 0, 0);
	hb->addWidget(homeTitle_, 0, 1, 1, 2);

	hb->addWidget(new QLabel(QStringLiteral("Subtitle:"), gbHome), 1, 0);
	hb->addWidget(homeSub_, 1, 1, 1, 2);

	hb->addWidget(new QLabel(QStringLiteral("Logo:"), gbHome), 2, 0);
	hb->addWidget(homeLogo_, 2, 1);
	hb->addWidget(homeBrowse_, 2, 2);

	gbHome->setLayout(hb);

	auto *gbAway = new QGroupBox(QStringLiteral("Guests team"), this);
	gbAway->setObjectName(QStringLiteral("awayGroup"));

	auto *ab = new QGridLayout(gbAway);
	ab->setContentsMargins(10, 10, 10, 10);
	ab->setHorizontalSpacing(8);
	ab->setVerticalSpacing(6);

	awayTitle_ = new QLineEdit(gbAway);
	awaySub_ = new QLineEdit(gbAway);
	awayLogo_ = new QLineEdit(gbAway);
	awayBrowse_ = new QToolButton(gbAway);
	{
		const QIcon openIcon = fly_themed_icon(this, "folder-open", QStyle::SP_DirOpenIcon);
		awayBrowse_->setText(QString());
		awayBrowse_->setIcon(openIcon);
		awayBrowse_->setToolTip(QStringLiteral("Browse logo…"));
		awayBrowse_->setAutoRaise(false);
		awayBrowse_->setCursor(Qt::PointingHandCursor);
	}

	ab->addWidget(new QLabel(QStringLiteral("Title:"), gbAway), 0, 0);
	ab->addWidget(awayTitle_, 0, 1, 1, 2);

	ab->addWidget(new QLabel(QStringLiteral("Subtitle:"), gbAway), 1, 0);
	ab->addWidget(awaySub_, 1, 1, 1, 2);

	ab->addWidget(new QLabel(QStringLiteral("Logo:"), gbAway), 2, 0);
	ab->addWidget(awayLogo_, 2, 1);
	ab->addWidget(awayBrowse_, 2, 2);

	gbAway->setLayout(ab);

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

	connect(applyBtn_, &QPushButton::clicked, this, &FlyTeamsDialog::onApply);
	connect(closeBtn, &QPushButton::clicked, this, &FlyTeamsDialog::close);
}

FlyTeamsDialog::~FlyTeamsDialog() = default;

void FlyTeamsDialog::syncUiFromState()
{
	if (homeTitle_)
		homeTitle_->setText(state_.home.title);
	if (homeSub_)
		homeSub_->setText(state_.home.subtitle);
	if (homeLogo_)
		homeLogo_->setText(state_.home.logo);

	if (awayTitle_)
		awayTitle_->setText(state_.away.title);
	if (awaySub_)
		awaySub_->setText(state_.away.subtitle);
	if (awayLogo_)
		awayLogo_->setText(state_.away.logo);
}

void FlyTeamsDialog::syncStateFromUi()
{
	if (homeTitle_)
		state_.home.title = homeTitle_->text();
	if (homeSub_)
		state_.home.subtitle = homeSub_->text();
	if (homeLogo_)
		state_.home.logo = homeLogo_->text();

	if (awayTitle_)
		state_.away.title = awayTitle_->text();
	if (awaySub_)
		state_.away.subtitle = awaySub_->text();
	if (awayLogo_)
		state_.away.logo = awayLogo_->text();
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
	LOGI("Teams dialog: titles/subtitles/logos saved.");
	accept();
}
