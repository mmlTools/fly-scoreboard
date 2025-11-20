#include "fly_score_hotkeys_dialog.hpp"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

FlyHotkeysDialog::FlyHotkeysDialog(const QVector<FlyHotkeyBinding> &initial, QWidget *parent) : QDialog(parent)
{
	setObjectName(QStringLiteral("FlyHotkeysDialog"));
	setWindowTitle(tr("Fly Scoreboard Hotkeys"));
	setModal(true);
	resize(720, 520);
	setMinimumSize(640, 420);
	setSizeGripEnabled(false);

	buildUi(initial);
}

void FlyHotkeysDialog::buildUi(const QVector<FlyHotkeyBinding> &initial)
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	auto *titleLabel = new QLabel(tr("Keyboard shortcuts for Fly Scoreboard"), this);
	titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600;"));

	auto *subtitleLabel = new QLabel(tr("Assign keyboard shortcuts for Fly Scoreboard actions. "
					    "Leave a shortcut empty to disable it."),
					 this);
	subtitleLabel->setWordWrap(true);
	subtitleLabel->setStyleSheet(QStringLiteral("color: #999; font-size: 11px;"));

	root->addWidget(titleLabel);
	root->addWidget(subtitleLabel);

	auto *center = new QHBoxLayout();
	center->setSpacing(10);
	root->addLayout(center, 1);

	auto *navPanel = new QFrame(this);
	navPanel->setObjectName(QStringLiteral("navPanel"));
	navPanel->setFrameShape(QFrame::StyledPanel);
	navPanel->setFrameShadow(QFrame::Plain);

	auto *navLayout = new QVBoxLayout(navPanel);
	navLayout->setContentsMargins(10, 10, 10, 10);
	navLayout->setSpacing(6);

	auto *navTitle = new QLabel(tr("Sections"), navPanel);
	navTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
	navLayout->addWidget(navTitle);

	btnNavScore_ = new QPushButton(tr("Scoreboard"), navPanel);
	btnNavFields_ = new QPushButton(tr("Match stats"), navPanel);
	btnNavTimers_ = new QPushButton(tr("Timers"), navPanel);

	btnNavScore_->setCheckable(true);
	btnNavFields_->setCheckable(true);
	btnNavTimers_->setCheckable(true);

	btnNavScore_->setCursor(Qt::PointingHandCursor);
	btnNavFields_->setCursor(Qt::PointingHandCursor);
	btnNavTimers_->setCursor(Qt::PointingHandCursor);

	navLayout->addWidget(btnNavScore_);
	navLayout->addWidget(btnNavFields_);
	navLayout->addWidget(btnNavTimers_);
	navLayout->addStretch(1);

	center->addWidget(navPanel, 0);

	stack_ = new QStackedWidget(this);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(stack_);

	center->addWidget(scroll, 1);

	QVector<FlyHotkeyBinding> scoreboard;
	QVector<FlyHotkeyBinding> fields;
	QVector<FlyHotkeyBinding> timers;

	for (const auto &b : initial) {
		if (b.actionId.startsWith(QLatin1String("field_")))
			fields.push_back(b);
		else if (b.actionId.startsWith(QLatin1String("timer_")))
			timers.push_back(b);
		else
			scoreboard.push_back(b);
	}

	QWidget *scorePage = createSectionPage(tr("Scoreboard"), scoreboard, 0);
	QWidget *fieldsPage = createSectionPage(tr("Match stats"), fields, 1);
	QWidget *timersPage = createSectionPage(tr("Timers"), timers, 2);

	stack_->addWidget(scorePage);
	stack_->addWidget(fieldsPage);
	stack_->addWidget(timersPage);

	connect(btnNavScore_, &QPushButton::clicked, this, &FlyHotkeysDialog::onShowScoreboard);
	connect(btnNavFields_, &QPushButton::clicked, this, &FlyHotkeysDialog::onShowFields);
	connect(btnNavTimers_, &QPushButton::clicked, this, &FlyHotkeysDialog::onShowTimers);

	onShowScoreboard();

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

	btnResetAll_ = buttons->addButton(tr("Reset to Defaults"), QDialogButtonBox::ResetRole);
	btnResetAll_->setCursor(Qt::PointingHandCursor);

	connect(btnResetAll_, &QAbstractButton::clicked, this, [this]() {
		for (auto &row : rows_) {
			if (row.edit)
				row.edit->clear();
		}
	});

	connect(buttons, &QDialogButtonBox::accepted, this, &FlyHotkeysDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &FlyHotkeysDialog::reject);

	root->addWidget(buttons);
}

QWidget *FlyHotkeysDialog::createSectionPage(const QString &title, const QVector<FlyHotkeyBinding> &items,
					     int sectionIndex)
{
	auto *page = new QWidget(this);
	auto *v = new QVBoxLayout(page);
	v->setContentsMargins(4, 4, 4, 4);
	v->setSpacing(8);

	auto *group = new QGroupBox(title, page);
	group->setObjectName(QStringLiteral("sectionGroup"));

	auto *cardLayout = new QVBoxLayout(group);
	cardLayout->setContentsMargins(10, 10, 10, 10);
	cardLayout->setSpacing(6);

	if (items.isEmpty()) {
		auto *emptyLbl = new QLabel(tr("No actions available in this category yet."), group);
		emptyLbl->setWordWrap(true);
		cardLayout->addWidget(emptyLbl);
	} else {
		for (const auto &b : items) {
			auto *rowWidget = new QWidget(group);
			auto *h = new QHBoxLayout(rowWidget);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(8);

			auto *lbl = new QLabel(b.label, rowWidget);
			lbl->setMinimumWidth(220);

			auto *edit = new QKeySequenceEdit(b.sequence, rowWidget);
			edit->setClearButtonEnabled(true);
			edit->setMaximumWidth(260);

			auto *clearBtn = new QPushButton(tr("Clear"), rowWidget);
			clearBtn->setAutoDefault(false);
			clearBtn->setDefault(false);
			clearBtn->setCursor(Qt::PointingHandCursor);

			h->addWidget(lbl, 1);
			h->addWidget(edit, 0);
			h->addWidget(clearBtn, 0);

			rowWidget->setLayout(h);
			cardLayout->addWidget(rowWidget);

			RowWidgets rw;
			rw.actionId = b.actionId;
			rw.label = b.label;
			rw.edit = edit;
			rw.section = sectionIndex;
			rows_.push_back(rw);

			connect(clearBtn, &QPushButton::clicked, edit, &QKeySequenceEdit::clear);
		}
	}

	cardLayout->addStretch(1);
	group->setLayout(cardLayout);
	v->addWidget(group);
	v->addStretch(1);

	return page;
}

void FlyHotkeysDialog::setActiveSectionButton(int index)
{
	btnNavScore_->setChecked(index == 0);
	btnNavFields_->setChecked(index == 1);
	btnNavTimers_->setChecked(index == 2);
}

void FlyHotkeysDialog::onShowScoreboard()
{
	stack_->setCurrentIndex(0);
	setActiveSectionButton(0);
}

void FlyHotkeysDialog::onShowFields()
{
	stack_->setCurrentIndex(1);
	setActiveSectionButton(1);
}

void FlyHotkeysDialog::onShowTimers()
{
	stack_->setCurrentIndex(2);
	setActiveSectionButton(2);
}

QVector<FlyHotkeyBinding> FlyHotkeysDialog::bindings() const
{
	QVector<FlyHotkeyBinding> out;
	out.reserve(rows_.size());

	for (const auto &row : rows_) {
		QKeySequence seq;
		if (row.edit)
			seq = row.edit->keySequence();

		FlyHotkeyBinding b;
		b.actionId = row.actionId;
		b.label = row.label;
		b.sequence = seq;
		out.push_back(b);
	}

	return out;
}

void FlyHotkeysDialog::onAccept()
{
	auto current = bindings();
	emit bindingsChanged(current);
	accept();
}

static QString hotkeysFilePath(const QString &dataDir)
{
	QDir dir(dataDir);
	return dir.filePath(QStringLiteral("hotkeys.json"));
}

QVector<FlyHotkeyBinding> fly_hotkeys_load(const QString &dataDir)
{
	QVector<FlyHotkeyBinding> out;

	const QString path = hotkeysFilePath(dataDir);
	QFile f(path);
	if (!f.exists() || !f.open(QIODevice::ReadOnly))
		return out;

	const auto doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isArray())
		return out;

	const auto arr = doc.array();
	out.reserve(arr.size());

	for (const auto &v : arr) {
		const auto o = v.toObject();

		FlyHotkeyBinding b;
		b.actionId = o.value(QStringLiteral("id")).toString();
		b.label = o.value(QStringLiteral("label")).toString();
		const QString seqStr = o.value(QStringLiteral("seq")).toString();

		if (!seqStr.isEmpty())
			b.sequence = QKeySequence(seqStr, QKeySequence::PortableText);

		if (!b.actionId.isEmpty())
			out.push_back(b);
	}

	return out;
}

bool fly_hotkeys_save(const QString &dataDir, const QVector<FlyHotkeyBinding> &bindings)
{
	QJsonArray arr;
	for (const auto &b : bindings) {
		if (b.actionId.isEmpty())
			continue;

		QJsonObject o;
		o[QStringLiteral("id")] = b.actionId;
		o[QStringLiteral("label")] = b.label;
		o[QStringLiteral("seq")] = b.sequence.toString(QKeySequence::PortableText);
		arr.append(o);
	}

	const QJsonDocument doc(arr);
	const QString path = hotkeysFilePath(dataDir);

	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	f.write(doc.toJson(QJsonDocument::Compact));
	return true;
}
