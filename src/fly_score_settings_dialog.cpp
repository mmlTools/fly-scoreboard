#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][settings]"
#include "fly_score_log.hpp"

#include "fly_score_settings_dialog.hpp"
#include "fly_score_server.hpp"
#include "fly_score_state.hpp"
#include "fly_score_paths.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QGroupBox>

#include "httplib.h"

static void styleDot(QLabel *dot, bool ok)
{
	dot->setFixedSize(12, 12);
	dot->setStyleSheet(QString("border-radius:6px;"
				   "background-color:%1;"
				   "border:1px solid rgba(0,0,0,0.25);")
				   .arg(ok ? "#39d353" : "#ff6b6b"));
}

FlySettingsDialog::FlySettingsDialog(QWidget *parent) : QDialog(parent)
{
	setObjectName(QStringLiteral("FlySettingsDialog"));
	setWindowTitle(QStringLiteral("Fly Scoreboard – Settings"));
	setModal(false);
	setSizeGripEnabled(false);
	setMinimumWidth(460);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	auto *hint = new QLabel(QStringLiteral("Configure the local web server used by the Fly Scoreboard overlay.\n"
					       "The overlay Browser Source will connect to this local server."),
				this);
	hint->setWordWrap(true);
	root->addWidget(hint);

	auto *gbServer = new QGroupBox(QStringLiteral("Server status"), this);

	auto *serverLayout = new QVBoxLayout(gbServer);
	serverLayout->setContentsMargins(10, 10, 10, 10);
	serverLayout->setSpacing(6);

	auto *statusRow = new QWidget(gbServer);
	auto *statusLay = new QHBoxLayout(statusRow);
	statusLay->setContentsMargins(0, 0, 0, 0);
	statusLay->setSpacing(6);

	statusDot_ = new QLabel(statusRow);
	statusText_ = new QLabel(QStringLiteral("Stopped"), statusRow);

	statusLay->addWidget(statusDot_);
	statusLay->addWidget(statusText_);
	statusLay->addStretch(1);

	statusRow->setLayout(statusLay);
	serverLayout->addWidget(statusRow);

	auto *statusHint = new QLabel(
		QStringLiteral("If the server is running, your Browser Source can load the overlay URL."), gbServer);
	statusHint->setWordWrap(true);
	serverLayout->addWidget(statusHint);

	gbServer->setLayout(serverLayout);
	root->addWidget(gbServer);

	auto *gbDoc = new QGroupBox(QStringLiteral("Document root (overlay files)"), this);

	auto *docLayout = new QVBoxLayout(gbDoc);
	docLayout->setContentsMargins(10, 10, 10, 10);
	docLayout->setSpacing(6);

	auto *docHint = new QLabel(
		QStringLiteral("This folder will contain plugin.json, hotkeys.json and your overlay HTML/CSS/JS."),
		gbDoc);
	docHint->setWordWrap(true);
	docLayout->addWidget(docHint);

	docRootEdit_ = new QLineEdit(gbDoc);
	docRootEdit_->setReadOnly(true);
	docRootEdit_->setMinimumWidth(320);

	const QString rootPath = fly_get_data_root(this);
	docRootEdit_->setText(rootPath);
	docLayout->addWidget(docRootEdit_);

	auto *docButtonsRow = new QWidget(gbDoc);
	auto *docButtonsLay = new QHBoxLayout(docButtonsRow);
	docButtonsLay->setContentsMargins(0, 0, 0, 0);
	docButtonsLay->setSpacing(6);

	browseDocRootBtn_ = new QPushButton(QStringLiteral("Browse…"), docButtonsRow);
	openOverlayBtn_ = new QPushButton(QStringLiteral("Open folder"), docButtonsRow);

	browseDocRootBtn_->setCursor(Qt::PointingHandCursor);
	openOverlayBtn_->setCursor(Qt::PointingHandCursor);

	docButtonsLay->addStretch(1);
	docButtonsLay->addWidget(browseDocRootBtn_);
	docButtonsLay->addWidget(openOverlayBtn_);

	docButtonsRow->setLayout(docButtonsLay);
	docLayout->addWidget(docButtonsRow);

	gbDoc->setLayout(docLayout);
	root->addWidget(gbDoc);

	auto *gbPort = new QGroupBox(QStringLiteral("Web server port"), this);

	auto *portLayoutOuter = new QVBoxLayout(gbPort);
	portLayoutOuter->setContentsMargins(10, 10, 10, 10);
	portLayoutOuter->setSpacing(6);

	auto *portHint = new QLabel(
		QStringLiteral("Default is 8089. Use a free port that your Browser Source can reach."), gbPort);
	portHint->setWordWrap(true);
	portLayoutOuter->addWidget(portHint);

	auto *portRow = new QWidget(gbPort);
	auto *portLay = new QHBoxLayout(portRow);
	portLay->setContentsMargins(0, 0, 0, 0);
	portLay->setSpacing(8);

	portSpin_ = new QSpinBox(portRow);
	portSpin_->setRange(1024, 65535);
	{
		int p = fly_server_port();
		if (p <= 0)
			p = 8089;
		portSpin_->setValue(p);
	}
	portSpin_->setFixedWidth(100);

	restartBtn_ = new QPushButton(QStringLiteral("Restart server"), portRow);
	restartBtn_->setCursor(Qt::PointingHandCursor);

	portLay->addWidget(portSpin_, 0, Qt::AlignLeft);
	portLay->addStretch(1);
	portLay->addWidget(restartBtn_, 0, Qt::AlignRight);

	portRow->setLayout(portLay);
	portLayoutOuter->addWidget(portRow);

	gbPort->setLayout(portLayoutOuter);
	root->addWidget(gbPort);

	auto *bottomRow = new QWidget(this);
	auto *bottomLay = new QHBoxLayout(bottomRow);
	bottomLay->setContentsMargins(0, 4, 0, 0);
	bottomLay->setSpacing(8);

	auto *bottomHint =
		new QLabel(QStringLiteral("Changes to port and document root are applied when you restart the server."),
			   bottomRow);

	auto *closeBtn = new QPushButton(QStringLiteral("Close"), bottomRow);
	closeBtn->setCursor(Qt::PointingHandCursor);
	closeBtn->setDefault(true);

	bottomLay->addWidget(bottomHint);
	bottomLay->addStretch(1);
	bottomLay->addWidget(closeBtn);

	bottomRow->setLayout(bottomLay);
	root->addWidget(bottomRow);

	setLayout(root);

	statusTimer_ = new QTimer(this);
	connect(statusTimer_, &QTimer::timeout, this, &FlySettingsDialog::onPollHealth);
	statusTimer_->start(2000);
	connect(browseDocRootBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onBrowseDocRoot);
	connect(openOverlayBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onOpenOverlayFolder);
	connect(restartBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onRestartServer);
	connect(closeBtn, &QPushButton::clicked, this, &FlySettingsDialog::accept);

	const int p = fly_server_port();
	const bool ok = healthOkHttp(p);
	setStatusUi(ok, p);
}

FlySettingsDialog::~FlySettingsDialog() {}

bool FlySettingsDialog::healthOkHttp(int port, int timeout_ms)
{
	if (port <= 0)
		return false;

	httplib::Client cli("127.0.0.1", port);
	cli.set_keep_alive(false);
	cli.set_connection_timeout(0, timeout_ms * 1000);
	cli.set_read_timeout(0, timeout_ms * 1000);
	cli.set_write_timeout(0, timeout_ms * 1000);

	if (auto res = cli.Get("/__ow/health"))
		return res->status == 200;

	return false;
}

void FlySettingsDialog::setStatusUi(bool ok, int port)
{
	styleDot(statusDot_, ok);

	if (ok)
		statusText_->setText(QStringLiteral("Running on :%1").arg(port));
	else
		statusText_->setText(QStringLiteral("Stopped (:%1)").arg(port > 0 ? port : 0));
}

void FlySettingsDialog::onPollHealth()
{
	const int p = fly_server_port();
	const bool ok = fly_server_is_running() || healthOkHttp(p);
	setStatusUi(ok, p);
}

void FlySettingsDialog::onRestartServer()
{
	int desiredPort = portSpin_ ? portSpin_->value() : 8089;

	QString docRoot = fly_get_data_root_no_ui();
	if (docRoot.isEmpty())
		docRoot = fly_get_data_root(this);

	if (fly_server_port() > 0)
		fly_server_stop();

	int bound = fly_server_start(docRoot, desiredPort);
	if (!bound) {
		LOGW("Could not bind any port near %d", desiredPort);
	} else if (bound != desiredPort) {
		LOGI("Port adjusted to %d (requested %d)", bound, desiredPort);
		if (portSpin_)
			portSpin_->setValue(bound);
	}

	const int p = fly_server_port();
	setStatusUi(healthOkHttp(p), p);
}

void FlySettingsDialog::onOpenOverlayFolder()
{
	QString root = fly_get_data_root_no_ui();
	if (root.isEmpty())
		root = fly_get_data_root(this);

	QDesktopServices::openUrl(QUrl::fromLocalFile(root));

	LOGI("Open data root folder: %s", root.toUtf8().constData());
}

void FlySettingsDialog::onBrowseDocRoot()
{
	QString current = fly_get_data_root_no_ui();
	if (current.isEmpty())
		current = fly_default_data_root();

	QString picked = QFileDialog::getExistingDirectory(
		this, tr("Select Fly Scoreboard document root (contains plugin.json and overlay files)"), current);

	if (picked.isEmpty())
		return;

	fly_set_data_root(picked);

	docRootEdit_->setText(picked);

	LOGI("Data root (document root) changed to: %s", picked.toUtf8().constData());
}
