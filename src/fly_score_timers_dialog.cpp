#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][timers-dialog]"
#include "fly_score_log.hpp"

#include "fly_score_timers_dialog.hpp"
#include "fly_score_state.hpp"
#include "fly_score_qt_helpers.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QStyle>

FlyTimersDialog::FlyTimersDialog(const QString &dataDir, FlyState &state, QWidget *parent)
	: QDialog(parent),
	  dataDir_(dataDir),
	  st_(state)
{
	setObjectName(QStringLiteral("FlyTimersDialog"));
	setWindowTitle(QStringLiteral("Fly Scoreboard – Timers"));
	setModal(true);
	resize(560, 420);
	setSizeGripEnabled(false);

	buildUi();
	loadFromState();
}

void FlyTimersDialog::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	auto *infoBox = new QGroupBox(QStringLiteral("Timers"), this);
	infoBox->setObjectName(QStringLiteral("timersInfoGroup"));

	auto *infoLayout = new QVBoxLayout(infoBox);
	infoLayout->setContentsMargins(10, 10, 10, 10);
	infoLayout->setSpacing(8);

	auto *hintLbl = new QLabel(QStringLiteral("Configure all timers used by the overlay.\n"
						  "Timer 1 is the main match timer and cannot be removed."),
				   infoBox);
	hintLbl->setObjectName(QStringLiteral("timersHint"));
	hintLbl->setWordWrap(true);
	infoLayout->addWidget(hintLbl);

	infoBox->setLayout(infoLayout);
	root->addWidget(infoBox);

	auto *rowsBox = new QGroupBox(QString(), this);
	rowsBox->setObjectName(QStringLiteral("timersRowsGroup"));

	auto *rowsLayout = new QVBoxLayout(rowsBox);
	rowsLayout->setContentsMargins(10, 10, 10, 10);
	rowsLayout->setSpacing(8);

	timersLayout_ = new QVBoxLayout();
	timersLayout_->setContentsMargins(0, 4, 0, 0);
	timersLayout_->setSpacing(6);
	timersLayout_->setAlignment(Qt::AlignTop);

	rowsLayout->addLayout(timersLayout_);

	addTimerBtn_ = new QPushButton(QStringLiteral("＋ Add timer"), rowsBox);
	addTimerBtn_->setCursor(Qt::PointingHandCursor);
	rowsLayout->addWidget(addTimerBtn_, 0, Qt::AlignLeft);

	rowsBox->setLayout(rowsLayout);
	root->addWidget(rowsBox);

	auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	root->addWidget(btnBox);

	connect(addTimerBtn_, &QPushButton::clicked, this, &FlyTimersDialog::onAddTimer);
	connect(btnBox, &QDialogButtonBox::accepted, this, &FlyTimersDialog::onAccept);
	connect(btnBox, &QDialogButtonBox::rejected, this, &FlyTimersDialog::reject);
}

FlyTimersDialog::Row FlyTimersDialog::addRow(const FlyTimer &tm, bool canRemove)
{
	Row r;

	auto *row = new QWidget(this);
	auto *lay = new QHBoxLayout(row);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(8);

	auto *labelEdit = new QLineEdit(row);
	labelEdit->setPlaceholderText(QStringLiteral("Timer label (e.g. First Half, Extra time)"));
	labelEdit->setText(tm.label);
	labelEdit->setMinimumWidth(160);
	labelEdit->setMaximumWidth(260);

	auto *timeEdit = new QLineEdit(row);
	timeEdit->setPlaceholderText(QStringLiteral("mm:ss"));
	timeEdit->setClearButtonEnabled(true);
	timeEdit->setMaxLength(8);
	timeEdit->setMinimumWidth(70);
	timeEdit->setMaximumWidth(90);
	timeEdit->setText(fly_format_ms_mmss(tm.initial_ms > 0 ? tm.initial_ms : tm.remaining_ms));

	auto *modeCombo = new QComboBox(row);
	modeCombo->addItems({QStringLiteral("countdown"), QStringLiteral("countup")});
	if (tm.mode.isEmpty())
		modeCombo->setCurrentText(QStringLiteral("countdown"));
	else
		modeCombo->setCurrentText(tm.mode);
	modeCombo->setMinimumWidth(110);
	modeCombo->setMaximumWidth(140);

	auto *removeBtn = new QPushButton(row);
	removeBtn->setText(QStringLiteral("❌"));
	removeBtn->setToolTip(canRemove ? QStringLiteral("Remove this timer")
					: QStringLiteral("This timer cannot be removed"));
	removeBtn->setCursor(canRemove ? Qt::PointingHandCursor : Qt::ArrowCursor);

	const int h = timeEdit->sizeHint().height();
	const int btnSize = h;
	removeBtn->setFixedSize(btnSize, btnSize);
	removeBtn->setEnabled(canRemove);
	removeBtn->setStyleSheet("QPushButton {"
				 "  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				 "  font-size:12px;"
				 "  padding:0;"
				 "}");

	lay->addWidget(labelEdit, 1, Qt::AlignVCenter);
	lay->addWidget(new QLabel(QStringLiteral("Time:"), row), 0, Qt::AlignVCenter);
	lay->addWidget(timeEdit, 0, Qt::AlignVCenter);
	lay->addWidget(new QLabel(QStringLiteral("Type:"), row), 0, Qt::AlignVCenter);
	lay->addWidget(modeCombo, 0, Qt::AlignVCenter);
	lay->addWidget(removeBtn, 0, Qt::AlignVCenter);

	row->setLayout(lay);
	timersLayout_->addWidget(row);

	r.row = row;
	r.labelEdit = labelEdit;
	r.timeEdit = timeEdit;
	r.modeCombo = modeCombo;
	r.remove = removeBtn;

	if (canRemove) {
		connect(removeBtn, &QPushButton::clicked, this, [this, row]() {
			for (int i = 0; i < rows_.size(); ++i) {
				if (rows_[i].row == row) {
					auto rr = rows_[i];
					rows_.removeAt(i);
					if (timersLayout_)
						timersLayout_->removeWidget(rr.row);
					rr.row->deleteLater();
					break;
				}
			}
		});
	}

	return r;
}

void FlyTimersDialog::loadFromState()
{
	for (auto &r : rows_) {
		if (timersLayout_ && r.row)
			timersLayout_->removeWidget(r.row);
		if (r.row)
			r.row->deleteLater();
	}
	rows_.clear();

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

	rows_.reserve(st_.timers.size());
	for (int i = 0; i < st_.timers.size(); ++i) {
		const auto &tm = st_.timers[i];
		const bool canRemove = (i > 0);
		Row r = addRow(tm, canRemove);
		rows_.push_back(r);
	}
}

void FlyTimersDialog::saveToState()
{
	const auto oldTimers = st_.timers;

	st_.timers.clear();
	st_.timers.reserve(rows_.size());

	for (int i = 0; i < rows_.size(); ++i) {
		const auto &r = rows_[i];

		FlyTimer tm;
		tm.label = r.labelEdit ? r.labelEdit->text() : QString();

		QString mode = r.modeCombo ? r.modeCombo->currentText() : QStringLiteral("countdown");
		tm.mode = mode;

		qint64 ms = 0;
		if (r.timeEdit) {
			qint64 parsed = fly_parse_mmss_to_ms(r.timeEdit->text());
			ms = (parsed >= 0) ? parsed : 0;
		}

		tm.initial_ms = ms;
		tm.remaining_ms = ms;
		tm.running = false;
		tm.last_tick_ms = 0;

		// Preserve previous visible flag if it exists; default to true for new timers
		if (i < oldTimers.size()) {
			tm.visible = oldTimers[i].visible;
		} else {
			tm.visible = true;
		}

		st_.timers.push_back(tm);
	}

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

	fly_state_save(dataDir_, st_);
}

void FlyTimersDialog::onAddTimer()
{
	FlyTimer tm;
	tm.label = QString();
	tm.mode = QStringLiteral("countdown");
	tm.running = false;
	tm.initial_ms = 0;
	tm.remaining_ms = 0;
	tm.last_tick_ms = 0;
	tm.visible = true;

	Row r = addRow(tm, true);
	rows_.push_back(r);
}

void FlyTimersDialog::onAccept()
{
	saveToState();
	accept();
}
