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
	auto *infoBox = new QGroupBox(QStringLiteral("Match stats"), this);
	infoBox->setObjectName(QStringLiteral("fieldsInfoGroup"));

	auto *infoLayout = new QVBoxLayout(infoBox);
	infoLayout->setContentsMargins(10, 10, 10, 10);
	infoLayout->setSpacing(8);

	auto *hintLbl = new QLabel(QStringLiteral("Configure all your custom numeric fields.\n"
						  "Example labels: Corners, Penalties, Shots on goal, etc.\n\n"
						  "Note: The first two fields are reserved for the main scoreboard "
						  "(Home/Away points and Home/Away score) and cannot be removed."),
				   infoBox);
	hintLbl->setObjectName(QStringLiteral("fieldsHint"));
	hintLbl->setWordWrap(true);
	infoLayout->addWidget(hintLbl);

	infoBox->setLayout(infoLayout);
	root->addWidget(infoBox);

	// -----------------------------------------------------------------
	// Rows box – same structure as timers dialog (no scroll area)
	// -----------------------------------------------------------------
	auto *rowsBox = new QGroupBox(QString(), this);
	rowsBox->setObjectName(QStringLiteral("fieldsGroup"));

	auto *rowsLayout = new QVBoxLayout(rowsBox);
	rowsLayout->setContentsMargins(10, 10, 10, 10);
	rowsLayout->setSpacing(8);

	fieldsLayout_ = new QVBoxLayout();
	fieldsLayout_->setContentsMargins(0, 4, 0, 0);
	fieldsLayout_->setSpacing(6);
	// Make sure rows hug the top of the groupbox
	fieldsLayout_->setAlignment(Qt::AlignTop);

	rowsLayout->addLayout(fieldsLayout_);

	addFieldBtn_ = new QPushButton(QStringLiteral("＋ Add stat"), rowsBox);
	addFieldBtn_->setCursor(Qt::PointingHandCursor);
	rowsLayout->addWidget(addFieldBtn_, 0, Qt::AlignLeft);

	rowsBox->setLayout(rowsLayout);
	root->addWidget(rowsBox);

	// -----------------------------------------------------------------
	// OK / Cancel buttons
	// -----------------------------------------------------------------
	auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	root->addWidget(btnBox);

	connect(addFieldBtn_, &QPushButton::clicked, this, &FlyFieldsDialog::onAddField);
	connect(btnBox, &QDialogButtonBox::accepted, this, &FlyFieldsDialog::onAccept);
	connect(btnBox, &QDialogButtonBox::rejected, this, &FlyFieldsDialog::reject);
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
	for (auto &r : rows_) {
		if (fieldsLayout_ && r.row)
			fieldsLayout_->removeWidget(r.row);
		if (r.row)
			r.row->deleteLater();
	}
	rows_.clear();
	vis_.clear();

	if (!fieldsLayout_)
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

void FlyFieldsDialog::onAccept()
{
	saveToState();
	accept();
}
