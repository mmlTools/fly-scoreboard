#include "widget.hpp"
#include "config.hpp"
#include "fly_score_i18n.hpp"
#include "fly_score_icons.hpp"

#include <QAbstractAnimation>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSequentialAnimationGroup>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVector>

QWidget *widget_create_support_card(QWidget *parent)
{
	auto *supportCard = new QFrame(parent);
	supportCard->setObjectName(QStringLiteral("supportCard"));
	supportCard->setFrameShape(QFrame::NoFrame);
	supportCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	supportCard->setMinimumHeight(104);

	auto *supportLay = new QHBoxLayout(supportCard);
	supportLay->setContentsMargins(12, 10, 12, 10);
	supportLay->setSpacing(10);

	auto *supportIcon = new QLabel(supportCard);
	supportIcon->setPixmap(fly_icon(FlyIcon::Heart, supportIcon).pixmap(30, 30));
	supportIcon->setStyleSheet(QStringLiteral("font-size:30px;"));

	auto *supportText = new QLabel(supportCard);
	supportText->setTextFormat(Qt::RichText);
	supportText->setOpenExternalLinks(true);
	supportText->setTextInteractionFlags(Qt::TextBrowserInteraction);
	supportText->setWordWrap(true);
	supportText->setText(fly_i18n("Information.SupportMessage"));

	auto *supportBtn = new QPushButton(fly_i18n("Information.SupportButton"), supportCard);
	supportBtn->setCursor(Qt::PointingHandCursor);
	supportBtn->setMinimumHeight(28);
	supportBtn->setStyleSheet("QPushButton { background: #29abe0; color:white; border:none; "
				  "border-radius:6px; padding:6px 10px; font-weight:600; }"
				  "QPushButton:hover { background: #1e97c6; }"
				  "QPushButton:pressed { background: #1984ac; }");

	QObject::connect(supportBtn, &QPushButton::clicked, supportCard, [] {
		QDesktopServices::openUrl(
			QUrl(QStringLiteral("https://www.paypal.com/donate/?hosted_button_id=ZKTLLYY9ADWYQ")));
	});

	supportLay->addWidget(supportIcon, 0, Qt::AlignVCenter);
	supportLay->addWidget(supportText, 1);
	supportLay->addWidget(supportBtn, 0, Qt::AlignVCenter);

	supportCard->setStyleSheet("#supportCard {"
				   "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
				   "    stop:0 #2a2d30, stop:1 #1e2124);"
				   "  border:1px solid #3a3d40; border-radius:10px; padding:6px; }"
				   "#supportCard QLabel { color:#ffffff; }");

	return supportCard;
}

QWidget *widget_create_discord_card(QWidget *parent)
{
	auto *discordCard = new QFrame(parent);
	discordCard->setObjectName(QStringLiteral("discordCard"));
	discordCard->setFrameShape(QFrame::NoFrame);
	discordCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	discordCard->setMinimumHeight(104);

	auto *lay = new QHBoxLayout(discordCard);
	lay->setContentsMargins(12, 10, 12, 10);
	lay->setSpacing(10);

	auto *icon = new QLabel(discordCard);
	icon->setPixmap(fly_icon(FlyIcon::Chat, icon).pixmap(30, 30));
	icon->setStyleSheet(QStringLiteral("font-size:30px;"));

	auto *text = new QLabel(discordCard);
	text->setTextFormat(Qt::RichText);
	text->setWordWrap(true);
	text->setText(fly_i18n("Information.DiscordMessage"));

	auto *btn = new QPushButton(fly_i18n("Information.DiscordButton"), discordCard);
	btn->setCursor(Qt::PointingHandCursor);
	btn->setMinimumHeight(28);
	btn->setStyleSheet("QPushButton { "
			   "  background: #5865F2; "
			   "  color: #FFFFFF; "
			   "  border: none; "
			   "  border-radius: 6px; "
			   "  padding: 6px 12px; "
			   "  font-weight: 600; "
			   "}"
			   "QPushButton:hover { "
			   "  background: #4752C4; "
			   "}"
			   "QPushButton:pressed { "
			   "  background: #3C45A5; "
			   "}");

	QObject::connect(btn, &QPushButton::clicked, discordCard,
			 [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://discord.gg/2yD6B2PTuQ"))); });

	lay->addWidget(icon, 0, Qt::AlignVCenter);
	lay->addWidget(text, 1);
	lay->addWidget(btn, 0, Qt::AlignVCenter);

	discordCard->setStyleSheet("#discordCard {"
				   "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
				   "    stop:0 #23272A, "
				   "    stop:0.40 #3035A5, "
				   "    stop:1 #5865F2);"
				   "  border: 1px solid rgba(88, 101, 242, 0.85);"
				   "  border-radius: 10px; "
				   "  padding: 6px; "
				   "}"
				   "#discordCard QLabel { "
				   "  color: #E3E6FF; "
				   "}");

	return discordCard;
}

QWidget *create_widget_carousel(QWidget *parent)
{
	auto *wrapper = new QWidget(parent);
	wrapper->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto *root = new QVBoxLayout(wrapper);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(4);

	auto *stack = new QStackedWidget(wrapper);
	stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	stack->addWidget(widget_create_discord_card(stack));
	stack->addWidget(widget_create_support_card(stack));

	root->addWidget(stack);

	QVector<QToolButton *> dots;
	auto *dotsRow = new QHBoxLayout();
	dotsRow->setContentsMargins(0, 0, 0, 0);
	dotsRow->setSpacing(6);
	dotsRow->addStretch(1);

	const int count = stack->count();
	for (int i = 0; i < count; ++i) {
		auto *dot = new QToolButton(wrapper);
		dot->setText(QStringLiteral("●"));
		dot->setCheckable(true);
		dot->setAutoExclusive(true);
		dot->setCursor(Qt::PointingHandCursor);
		dot->setToolTip(fly_i18n("Information.ShowCard").arg(i + 1));
		dot->setStyleSheet("QToolButton {"
				   "  border: none;"
				   "  background: transparent;"
				   "  color: #666666;"
				   "  font-size: 13px;"
				   "  padding: 0;"
				   "}"
				   "QToolButton:checked {"
				   "  color: #ffffff;"
				   "}");
		dots << dot;
		dotsRow->addWidget(dot);
	}
	dotsRow->addStretch(1);
	root->addLayout(dotsRow);

	auto *effect = new QGraphicsOpacityEffect(stack);
	effect->setOpacity(1.0);
	stack->setGraphicsEffect(effect);

	auto updateDots = [stack, dots]() {
		const int idx = stack->currentIndex();
		for (int i = 0; i < dots.size(); ++i) {
			if (dots[i])
				dots[i]->setChecked(i == idx);
		}
	};

	updateDots();

	auto *timer = new QTimer(wrapper);
	timer->setInterval(20000);

	auto switchToIndex = [stack, effect, updateDots, wrapper](int targetIndex) {
		if (targetIndex < 0 || targetIndex >= stack->count())
			return;
		if (effect->property("animating").toBool() || targetIndex == stack->currentIndex())
			return;

		effect->setProperty("animating", true);

		auto *fadeOut = new QPropertyAnimation(effect, "opacity", wrapper);
		fadeOut->setDuration(180);
		fadeOut->setStartValue(1.0);
		fadeOut->setEndValue(0.0);

		auto *fadeIn = new QPropertyAnimation(effect, "opacity", wrapper);
		fadeIn->setDuration(180);
		fadeIn->setStartValue(0.0);
		fadeIn->setEndValue(1.0);

		auto *group = new QSequentialAnimationGroup(wrapper);
		group->addAnimation(fadeOut);
		group->addAnimation(fadeIn);

		QObject::connect(fadeOut, &QPropertyAnimation::finished, stack, [stack, targetIndex, updateDots]() {
			stack->setCurrentIndex(targetIndex);
			updateDots();
		});

		QObject::connect(group, &QSequentialAnimationGroup::finished, effect,
				 [effect]() { effect->setProperty("animating", false); });
		group->start(QAbstractAnimation::DeleteWhenStopped);
	};

	for (int i = 0; i < dots.size(); ++i) {
		if (!dots[i])
			continue;

		QObject::connect(dots[i], &QToolButton::clicked, wrapper, [i, timer, switchToIndex]() {
			if (timer)
				timer->start();
			switchToIndex(i);
		});
	}

	QObject::connect(timer, &QTimer::timeout, wrapper, [stack, switchToIndex]() {
		if (stack->count() == 0)
			return;
		int next = stack->currentIndex() + 1;
		if (next >= stack->count())
			next = 0;
		switchToIndex(next);
	});

	timer->start();
	return wrapper;
}

void show_information_dialog(QWidget *parent)
{
	// Reuse the open dialog; create the carousel only when requested.
	if (auto *existing = parent->findChild<QDialog *>(QStringLiteral("flyScoreInformation"))) {
		existing->show();
		existing->raise();
		existing->activateWindow();
		return;
	}
	auto *dialog = new QDialog(parent);
	dialog->setObjectName(QStringLiteral("flyScoreInformation"));
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowTitle(fly_i18n("Information.Title"));
	dialog->resize(560, 360);
	auto *layout = new QVBoxLayout(dialog);
	layout->setContentsMargins(16, 16, 16, 16);
	layout->setSpacing(12);

	auto *title = new QLabel(QStringLiteral("<h2>Fly Score %1</h2>").arg(QStringLiteral(PLUGIN_VERSION)), dialog);
	layout->addWidget(title);
	auto *description = new QLabel(fly_i18n("Information.Description"), dialog);
	description->setWordWrap(true);
	layout->addWidget(description);
	auto *milestone = new QLabel(fly_i18n("Information.Milestone"), dialog);
	milestone->setWordWrap(true);
	milestone->setTextFormat(Qt::RichText);
	milestone->setTextInteractionFlags(Qt::TextBrowserInteraction);
	milestone->setOpenExternalLinks(true);
	layout->addWidget(milestone);
	layout->addWidget(create_widget_carousel(dialog));
	layout->addStretch();
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
	QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
	layout->addWidget(buttons);
	dialog->show();
}
