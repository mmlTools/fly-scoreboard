#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][fields-dialog]"
#include "fly_score_log.hpp"

#include "fly_score_fields_dialog.hpp"
#include "fly_score_state.hpp"
#include "fly_score_qt_helpers.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QStyle>
#include <QTabWidget>
#include <QCheckBox>

FlyFieldsDialog::FlyFieldsDialog(const QString &dataDir, FlyState &state, QWidget *parent)
	: QDialog(parent),
	  dataDir_(dataDir),
	  st_(state)
{
	setObjectName(QStringLiteral("FlyFieldsDialog"));
	setWindowTitle(QStringLiteral("Fly Scoreboard Match stats"));
	setModal(true);

	// Match the timers dialog behaviour: fixed initial size, no growing with rows
	resize(520, 400);
	setSizeGripEnabled(false);

	buildUi();
	loadFromState();
}

void FlyFieldsDialog::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	// -----------------------------------------------------------------
	// Info box
	// -----------------------------------------------------------------
	auto *infoBox = new QGroupBox(QStringLiteral("Stats"), this);
	infoBox->setObjectName(QStringLiteral("fieldsInfoGroup"));

	auto *infoLayout = new QVBoxLayout(infoBox);
	infoLayout->setContentsMargins(10, 10, 10, 10);
	infoLayout->setSpacing(8);

	auto *hintLbl = new QLabel(QStringLiteral(
		"Configure the stats shown in your overlay.\n\n"
		"Team stats are paired values (Home/Guests) like Corners, Fouls, Shots, etc.\n"
		"Single stats are one value only (e.g. Possession %, Period, Downs, Power Plays, etc.).\n\n"
		"Note: The first two Team stats are reserved for the main scoreboard "
		"(Home/Away points and Home/Away score) and cannot be removed."),
		infoBox);
	hintLbl->setObjectName(QStringLiteral("fieldsHint"));
	hintLbl->setWordWrap(true);
	infoLayout->addWidget(hintLbl);

	infoBox->setLayout(infoLayout);
	root->addWidget(infoBox);

	// -----------------------------------------------------------------
	// Tabs: Team Stats | Single Stats
	// -----------------------------------------------------------------
	auto *tabs = new QTabWidget(this);
	tabs->setObjectName(QStringLiteral("flyFieldsTabs"));
	tabs->setDocumentMode(true);
	tabs->setMovable(false);
	tabs->setUsesScrollButtons(true);

	// -----------------------------
	// Team Stats tab
	// -----------------------------
	auto *teamTab = new QWidget(tabs);
	auto *teamRoot = new QVBoxLayout(teamTab);
	teamRoot->setContentsMargins(10, 10, 10, 10);
	teamRoot->setSpacing(8);

	fieldsLayout_ = new QVBoxLayout();
	fieldsLayout_->setContentsMargins(0, 4, 0, 0);
	fieldsLayout_->setSpacing(6);
	fieldsLayout_->setAlignment(Qt::AlignTop);
	teamRoot->addLayout(fieldsLayout_);

	addFieldBtn_ = new QPushButton(QStringLiteral("+ Add team stat"), teamTab);
	addFieldBtn_->setCursor(Qt::PointingHandCursor);
	teamRoot->addWidget(addFieldBtn_, 0, Qt::AlignLeft);

	teamTab->setLayout(teamRoot);

	// -----------------------------
	// Single Stats tab
	// -----------------------------
	auto *singleTab = new QWidget(tabs);
	auto *singleRoot = new QVBoxLayout(singleTab);
	singleRoot->setContentsMargins(10, 10, 10, 10);
	singleRoot->setSpacing(8);

	singleLayout_ = new QVBoxLayout();
	singleLayout_->setContentsMargins(0, 4, 0, 0);
	singleLayout_->setSpacing(6);
	singleLayout_->setAlignment(Qt::AlignTop);
	singleRoot->addLayout(singleLayout_);

	addSingleBtn_ = new QPushButton(QStringLiteral("+ Add single stat"), singleTab);
	addSingleBtn_->setCursor(Qt::PointingHandCursor);
	singleRoot->addWidget(addSingleBtn_, 0, Qt::AlignLeft);

	singleTab->setLayout(singleRoot);

	tabs->addTab(teamTab, QStringLiteral("Team Stats"));
	tabs->addTab(singleTab, QStringLiteral("Single Stats"));
	root->addWidget(tabs);

	// -----------------------------------------------------------------
	// OK / Cancel buttons
	// -----------------------------------------------------------------
	auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	root->addWidget(btnBox);

	connect(addFieldBtn_, &QPushButton::clicked, this, &FlyFieldsDialog::onAddField);
	connect(addSingleBtn_, &QPushButton::clicked, this, &FlyFieldsDialog::onAddSingle);
	connect(btnBox, &QDialogButtonBox::accepted, this, &FlyFieldsDialog::onAccept);
	connect(btnBox, &QDialogButtonBox::rejected, this, &FlyFieldsDialog::reject);
}

FlyFieldsDialog::SingleRow FlyFieldsDialog::addSingleRow(const FlySingleStat &ss)
{
	SingleRow r;

	auto *row = new QWidget(this);
	auto *lay = new QHBoxLayout(row);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(8);

	auto *visible = new QCheckBox(row);
	visible->setChecked(ss.visible);
	visible->setToolTip(QStringLiteral("Visible in overlay"));

	auto *labelEdit = new QLineEdit(row);
	labelEdit->setPlaceholderText(QStringLiteral("Label (e.g. Possession %%)"));
	labelEdit->setText(ss.label);
	labelEdit->setMinimumWidth(220);
	labelEdit->setMaximumWidth(320);

	auto *valueSpin = new QSpinBox(row);
	valueSpin->setRange(-9999, 9999);
	valueSpin->setValue(ss.value);
	valueSpin->setMinimumWidth(70);
	valueSpin->setMaximumWidth(90);

	auto *removeBtn = new QPushButton(row);
	removeBtn->setText(QStringLiteral("❌"));
	removeBtn->setToolTip(QStringLiteral("Remove this single stat"));
	removeBtn->setCursor(Qt::PointingHandCursor);

	const int h = valueSpin->sizeHint().height();
	removeBtn->setFixedSize(h, h);
	removeBtn->setStyleSheet("QPushButton {"
			 "  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
			 "  font-size:12px;"
			 "  padding:0;"
			 "}");

	lay->addWidget(visible, 0, Qt::AlignVCenter);
	lay->addWidget(labelEdit, 1, Qt::AlignVCenter);
	lay->addWidget(new QLabel(QStringLiteral("Value:"), row), 0, Qt::AlignVCenter);
	lay->addWidget(valueSpin, 0, Qt::AlignVCenter);
	lay->addWidget(removeBtn, 0, Qt::AlignVCenter);

	row->setLayout(lay);
	if (singleLayout_)
		singleLayout_->addWidget(row);

	r.row = row;
	r.labelEdit = labelEdit;
	r.valueSpin = valueSpin;
	r.remove = removeBtn;
	r.visible = visible;

	connect(removeBtn, &QPushButton::clicked, this, [this, row]() {
		for (int i = 0; i < singles_.size(); ++i) {
			if (singles_[i].row == row) {
				auto rr = singles_[i];
				singles_.removeAt(i);
				if (singleLayout_)
					singleLayout_->removeWidget(rr.row);
				rr.row->deleteLater();
				break;
			}
		}
	});

	return r;
}

FlyFieldsDialog::Row FlyFieldsDialog::addRow(const FlyCustomField &cf, bool canRemove)
{
	Row r;
	r.canRemove = canRemove;

	auto *row = new QWidget(this);
	auto *lay = new QHBoxLayout(row);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(8);

	auto *labelEdit = new QLineEdit(row);
	labelEdit->setPlaceholderText(QStringLiteral("Label (e.g. Corners)"));
	labelEdit->setText(cf.label);
	labelEdit->setMinimumWidth(180);
	labelEdit->setMaximumWidth(260);

	auto *homeSpin = new QSpinBox(row);
	homeSpin->setRange(0, 999);
	homeSpin->setValue(std::max(0, cf.home));
	homeSpin->setMinimumWidth(60);
	homeSpin->setMaximumWidth(70);

	auto *awaySpin = new QSpinBox(row);
	awaySpin->setRange(0, 999);
	awaySpin->setValue(std::max(0, cf.away));
	awaySpin->setMinimumWidth(60);
	awaySpin->setMaximumWidth(70);

	auto *removeBtn = new QPushButton(row);
	removeBtn->setText(QStringLiteral("❌"));
	removeBtn->setToolTip(canRemove ? QStringLiteral("Remove this field")
					: QStringLiteral("This field cannot be removed"));
	removeBtn->setCursor(canRemove ? Qt::PointingHandCursor : Qt::ArrowCursor);

	const int h = homeSpin->sizeHint().height();
	const int btnSize = h;
	removeBtn->setFixedSize(btnSize, btnSize);
	removeBtn->setEnabled(canRemove);
	removeBtn->setStyleSheet("QPushButton {"
				 "  font-family:'Segoe UI Emoji','Noto Color Emoji','Apple Color Emoji',sans-serif;"
				 "  font-size:12px;"
				 "  padding:0;"
				 "}");

	lay->addWidget(labelEdit, 1, Qt::AlignVCenter);
	lay->addWidget(new QLabel(QStringLiteral("Home:"), row), 0, Qt::AlignVCenter);
	lay->addWidget(homeSpin, 0, Qt::AlignVCenter);
	lay->addWidget(new QLabel(QStringLiteral("Guests:"), row), 0, Qt::AlignVCenter);
	lay->addWidget(awaySpin, 0, Qt::AlignVCenter);
	lay->addWidget(removeBtn, 0, Qt::AlignVCenter);

	row->setLayout(lay);
	fieldsLayout_->addWidget(row);

	r.row = row;
	r.labelEdit = labelEdit;
	r.homeSpin = homeSpin;
	r.awaySpin = awaySpin;
	r.remove = removeBtn;

	if (canRemove) {
		connect(removeBtn, &QPushButton::clicked, this, [this, row]() {
			for (int i = 0; i < rows_.size(); ++i) {
				if (rows_[i].row == row) {
					auto rr = rows_[i];
					rows_.removeAt(i);
					if (i >= 0 && i < vis_.size())
						vis_.removeAt(i);
					if (fieldsLayout_)
						fieldsLayout_->removeWidget(rr.row);
					rr.row->deleteLater();
					break;
				}
			}
		});
	}

	return r;
}

void FlyFieldsDialog::loadFromState()
{
	// Clear team rows
	for (auto &r : rows_) {
		if (fieldsLayout_ && r.row)
			fieldsLayout_->removeWidget(r.row);
		if (r.row)
			r.row->deleteLater();
	}
	rows_.clear();
	vis_.clear();

	// Clear single rows
	for (auto &r : singles_) {
		if (singleLayout_ && r.row)
			singleLayout_->removeWidget(r.row);
		if (r.row)
			r.row->deleteLater();
	}
	singles_.clear();

	if (!fieldsLayout_ || !singleLayout_)
		return;

	rows_.reserve(st_.custom_fields.size());
	vis_.reserve(st_.custom_fields.size());

	for (int i = 0; i < st_.custom_fields.size(); ++i) {
		const FlyCustomField &cf = st_.custom_fields[i];
		const bool canRemove = (i >= 2); // first two rows reserved
		Row r = addRow(cf, canRemove);
		rows_.push_back(r);
		vis_.push_back(cf.visible);
	}

	// Load single stats
	singles_.reserve(st_.single_stats.size());
	for (int i = 0; i < st_.single_stats.size(); ++i) {
		const FlySingleStat &ss = st_.single_stats[i];
		SingleRow r = addSingleRow(ss);
		singles_.push_back(r);
	}
}

void FlyFieldsDialog::saveToState()
{
	st_.custom_fields.clear();
	st_.custom_fields.reserve(rows_.size());

	for (int i = 0; i < rows_.size(); ++i) {
		const auto &r = rows_[i];

		FlyCustomField cf;
		cf.label = r.labelEdit ? r.labelEdit->text() : QString();
		cf.home = r.homeSpin ? r.homeSpin->value() : 0;
		cf.away = r.awaySpin ? r.awaySpin->value() : 0;
		cf.visible = (i >= 0 && i < vis_.size()) ? vis_[i] : true;

		st_.custom_fields.push_back(cf);
	}

	// Save single stats
	st_.single_stats.clear();
	st_.single_stats.reserve(singles_.size());
	for (const auto &r : singles_) {
		FlySingleStat ss;
		ss.label = r.labelEdit ? r.labelEdit->text() : QString();
		ss.value = r.valueSpin ? r.valueSpin->value() : 0;
		ss.visible = r.visible ? r.visible->isChecked() : true;
		st_.single_stats.push_back(ss);
	}

	fly_state_save(dataDir_, st_);
}

void FlyFieldsDialog::onAddField()
{
	FlyCustomField cf;
	cf.label = QString();
	cf.home = 0;
	cf.away = 0;
	cf.visible = true;

	Row r = addRow(cf, /*canRemove=*/true);
	rows_.push_back(r);
	vis_.push_back(true);
}

void FlyFieldsDialog::onAddSingle()
{
	FlySingleStat ss;
	ss.label = QString();
	ss.value = 0;
	ss.visible = true;

	SingleRow r = addSingleRow(ss);
	singles_.push_back(r);
}

void FlyFieldsDialog::onAccept()
{
	saveToState();
	accept();
}
