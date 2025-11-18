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

#include "httplib.h"

static void styleDot(QLabel *dot, bool ok)
{
	dot->setFixedSize(12, 12);
	dot->setStyleSheet(QString("border-radius:6px;"
				   "background-color:%1;"
				   "border:1px solid rgba(0,0,0,0.25);")
				   .arg(ok ? "#39d353" : "#ff6b6b"));
}

FlySettingsDialog::FlySettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Fly Score – Settings"));
	setModal(false);

	auto v = new QVBoxLayout(this);
	v->setContentsMargins(10, 10, 10, 10);
	v->setSpacing(8);

	// Server status row
	auto statusRow = new QWidget(this);
	auto statusLay = new QHBoxLayout(statusRow);
	statusLay->setContentsMargins(0, 0, 0, 0);
	statusLay->setSpacing(6);
	statusLay->addWidget(new QLabel(QStringLiteral("Server:"), statusRow));
	statusDot_ = new QLabel(statusRow);
	statusText_ = new QLabel(QStringLiteral("Stopped"), statusRow);
	statusLay->addWidget(statusDot_);
	statusLay->addWidget(statusText_);
	statusLay->addStretch(1);
	statusRow->setLayout(statusLay);

	// Data/doc root row (user-configurable)
	auto docRootRow = new QWidget(this);
	auto docRootLay = new QHBoxLayout(docRootRow);
	docRootLay->setContentsMargins(0, 0, 0, 0);
	docRootLay->setSpacing(6);
	docRootLay->addWidget(new QLabel(QStringLiteral("Data / web root:"), docRootRow));
	docRootEdit_ = new QLineEdit(docRootRow);
	docRootEdit_->setReadOnly(true);

	// First-run handling: use global helper (asks user if unset)
	QString docRoot = fly_get_data_root(this);
	docRootEdit_->setText(docRoot);

	browseDocRootBtn_ = new QPushButton(QStringLiteral("Browse…"), docRootRow);
	docRootLay->addWidget(docRootEdit_, 1);
	docRootLay->addWidget(browseDocRootBtn_);
	docRootRow->setLayout(docRootLay);

	// Port row
	auto portRow = new QWidget(this);
	auto portLay = new QHBoxLayout(portRow);
	portLay->setContentsMargins(0, 0, 0, 0);
	portLay->setSpacing(6);
	portLay->addWidget(new QLabel(QStringLiteral("Web server port:"), portRow));
	portSpin_ = new QSpinBox(portRow);
	portSpin_->setRange(1024, 65535);
	{
		int p = fly_server_port();
		if (p <= 0)
			p = 8089;
		portSpin_->setValue(p);
	}
	restartBtn_ = new QPushButton(QStringLiteral("Restart Server"), portRow);
	portLay->addWidget(portSpin_, 1);
	portLay->addWidget(restartBtn_);
	portRow->setLayout(portLay);

	// Action buttons row
	auto actionsRow = new QWidget(this);
	auto actionsLay = new QHBoxLayout(actionsRow);
	actionsLay->setContentsMargins(0, 0, 0, 0);
	actionsLay->setSpacing(8);
	hotkeysBtn_ = new QPushButton(QStringLiteral("Hotkeys: how to bind…"), actionsRow);
	openFolderBtn_ = new QPushButton(QStringLiteral("Open Overlay Folder…"), actionsRow);
	actionsLay->addWidget(hotkeysBtn_);
	actionsLay->addWidget(openFolderBtn_);
	actionsLay->addStretch(1);
	actionsRow->setLayout(actionsLay);

	v->addWidget(statusRow);
	v->addWidget(docRootRow);
	v->addWidget(portRow);
	v->addSpacing(6);
	v->addWidget(actionsRow);
	setLayout(v);

	// Status polling
	statusTimer_ = new QTimer(this);
	connect(statusTimer_, &QTimer::timeout, this, &FlySettingsDialog::onPollHealth);
	statusTimer_->start(2000);

	// Signals
	connect(restartBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onRestartServer);
	connect(openFolderBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onOpenOverlay);
	connect(hotkeysBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onOpenHotkeysHelp);
	connect(browseDocRootBtn_, &QPushButton::clicked, this, &FlySettingsDialog::onBrowseDocRoot);

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
	if (!statusDot_ || !statusText_)
		return;
	styleDot(statusDot_, ok);
	if (ok)
		statusText_->setText(QStringLiteral("Running on :%1").arg(port));
	else
		statusText_->setText(QStringLiteral("Stopped (:%1)").arg(port > 0 ? port : 0));
}

void FlySettingsDialog::onPollHealth()
{
	const int p = fly_server_port();
	const bool ok = fly_server_is_running() || healthOkHttp(p, 400);
	setStatusUi(ok, p);
}

void FlySettingsDialog::onRestartServer()
{
	const int desiredPort = portSpin_ ? portSpin_->value() : 8089;
	QString docRoot = docRootEdit_ ? docRootEdit_->text() : fly_get_data_root_no_ui();

	if (docRoot.isEmpty())
		docRoot = fly_get_data_root(this); // will ask user

	if (fly_server_port() > 0)
		LOGI("Stopping current server on :%d", fly_server_port());
	fly_server_stop();

	int bound = fly_server_start(docRoot, desiredPort);
	if (!bound) {
		LOGW("Could not start server on requested port %d", desiredPort);
	} else if (bound != desiredPort) {
		LOGI("Port adjusted to %d (requested %d)", bound, desiredPort);
		if (portSpin_)
			portSpin_->setValue(bound);
	} else {
		LOGI("Server running on :%d", bound);
	}

	const int p = fly_server_port();
	setStatusUi(healthOkHttp(p), p);
}

void FlySettingsDialog::onOpenOverlay()
{
	QString dataRoot = docRootEdit_ ? docRootEdit_->text() : fly_get_data_root_no_ui();
	if (dataRoot.isEmpty())
		dataRoot = fly_get_data_root(this);

	const QString overlay = QDir(dataRoot).filePath(QStringLiteral("overlay"));
	QDesktopServices::openUrl(QUrl::fromLocalFile(overlay));
	LOGI("Open overlay/doc root folder: %s", overlay.toUtf8().constData());
}

void FlySettingsDialog::onOpenHotkeysHelp()
{
	QMessageBox::information(
		this, tr("Fly Score – Hotkeys"),
		tr("Bind keys in <b>OBS → Settings → Hotkeys</b>.\n\n"
		   "Search for actions that start with <i>\"Fly Scoreboard:\"</i>:\n"
		   " • Home/Guests Score ±1\n"
		   " • Home/Guests Wins ±1\n"
		   " • Swap Home ↔ Guests\n"
		   " • Show/Hide Scoreboard\n\n"
		   "Hotkeys are saved per profile by OBS."));
}

void FlySettingsDialog::onBrowseDocRoot()
{
	QString current = fly_get_data_root_no_ui();
	if (current.isEmpty())
		current = fly_default_data_root();

	QString picked = QFileDialog::getExistingDirectory(
		this,
		tr("Select Fly Scoreboard data folder (overlay + plugin.json)"),
		current);

	if (picked.isEmpty())
		return;

	fly_set_data_root(picked);
	if (docRootEdit_)
		docRootEdit_->setText(picked);

	LOGI("Data root changed: %s", picked.toUtf8().constData());
}
