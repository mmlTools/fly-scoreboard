#include "fly_score_kofi_widget.hpp"

#include <QStackedWidget>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QAbstractAnimation>
#include <QToolButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVector>

// -----------------------------------------------------------------------------
// Ko-fi card
// -----------------------------------------------------------------------------

QWidget *fly_create_kofi_card(QWidget *parent)
{
	auto *kofiCard = new QFrame(parent);
	kofiCard->setObjectName(QStringLiteral("kofiCard"));
	kofiCard->setFrameShape(QFrame::NoFrame);
	kofiCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto *kofiLay = new QHBoxLayout(kofiCard);
	kofiLay->setContentsMargins(12, 10, 12, 10);
	kofiLay->setSpacing(10);

	auto *kofiIcon = new QLabel(kofiCard);
	kofiIcon->setText(QString::fromUtf8("☕"));
	kofiIcon->setStyleSheet(QStringLiteral("font-size:30px;"));

	auto *kofiText = new QLabel(kofiCard);
	kofiText->setTextFormat(Qt::RichText);
	kofiText->setOpenExternalLinks(true);
	kofiText->setTextInteractionFlags(Qt::TextBrowserInteraction);
	kofiText->setWordWrap(true);
	kofiText->setText("<b>Enjoying this plugin?</b><br>"
			  "You can support<br>development on Ko-fi.");

	auto *kofiBtn = new QPushButton(QStringLiteral("☕ Buy me a Ko-fi"), kofiCard);
	kofiBtn->setCursor(Qt::PointingHandCursor);
	kofiBtn->setMinimumHeight(28);
	kofiBtn->setStyleSheet("QPushButton { background: #29abe0; color:white; border:none; "
			       "border-radius:6px; padding:6px 10px; font-weight:600; }"
			       "QPushButton:hover { background: #1e97c6; }"
			       "QPushButton:pressed { background: #1984ac; }");

	QObject::connect(kofiBtn, &QPushButton::clicked, kofiCard,
			 [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://ko-fi.com/mmltech"))); });

	kofiLay->addWidget(kofiIcon, 0, Qt::AlignVCenter);
	kofiLay->addWidget(kofiText, 1);
	kofiLay->addWidget(kofiBtn, 0, Qt::AlignVCenter);

	kofiCard->setStyleSheet("#kofiCard {"
				"  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
				"    stop:0 #2a2d30, stop:1 #1e2124);"
				"  border:1px solid #3a3d40; border-radius:10px; padding:6px; }"
				"#kofiCard QLabel { color:#ffffff; }");

	return kofiCard;
}

// -----------------------------------------------------------------------------
// Custom overlay order card
// -----------------------------------------------------------------------------

QWidget *fly_create_custom_overlay_card(QWidget *parent)
{
	auto *overlayCard = new QFrame(parent);
	overlayCard->setObjectName(QStringLiteral("customOverlayCard"));
	overlayCard->setFrameShape(QFrame::NoFrame);
	overlayCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto *overlayLay = new QHBoxLayout(overlayCard);
	overlayLay->setContentsMargins(12, 10, 12, 10);
	overlayLay->setSpacing(10);

	auto *overlayIcon = new QLabel(overlayCard);
	overlayIcon->setText(QString::fromUtf8("🎨"));
	overlayIcon->setStyleSheet(QStringLiteral("font-size:30px;"));

	auto *overlayText = new QLabel(overlayCard);
	overlayText->setTextFormat(Qt::RichText);
	overlayText->setWordWrap(true);
	overlayText->setText("<b>Need a custom overlay?</b><br>"
			     "Order custom overlays<br>starting at <b>50&nbsp;$/overlay</b>.");

	auto *overlayBtn = new QPushButton(QStringLiteral("📧 Order via Email"), overlayCard);
	overlayBtn->setCursor(Qt::PointingHandCursor);
	overlayBtn->setMinimumHeight(28);
	overlayBtn->setStyleSheet("QPushButton { background: #ffb347; color:#1b1b1b; border:none; "
				  "border-radius:6px; padding:6px 12px; font-weight:600; }"
				  "QPushButton:hover { background: #ffa726; }"
				  "QPushButton:pressed { background: #fb8c00; }");

	QObject::connect(overlayBtn, &QPushButton::clicked, overlayCard, [] {
		QDesktopServices::openUrl(
			QUrl(QStringLiteral("mailto:contact@obscountdown.com?subject=Custom%%20Overlay%%20Request")));
	});

	overlayLay->addWidget(overlayIcon, 0, Qt::AlignVCenter);
	overlayLay->addWidget(overlayText, 1);
	overlayLay->addWidget(overlayBtn, 0, Qt::AlignVCenter);

	overlayCard->setStyleSheet("#customOverlayCard {"
				   "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
				   "    stop:0 #3b2154, stop:1 #1f3c5a);"
				   "  border:1px solid #503864; border-radius:10px; padding:6px; }"
				   "#customOverlayCard QLabel { color:#f8f8ff; }");

	return overlayCard;
}

// -----------------------------------------------------------------------------
// Discord card
// -----------------------------------------------------------------------------

QWidget *fly_create_discord_card(QWidget *parent)
{
	auto *discordCard = new QFrame(parent);
	discordCard->setObjectName(QStringLiteral("discordCard"));
	discordCard->setFrameShape(QFrame::NoFrame);
	discordCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto *lay = new QHBoxLayout(discordCard);
	lay->setContentsMargins(12, 10, 12, 10);
	lay->setSpacing(10);

	auto *icon = new QLabel(discordCard);
	icon->setText(QString::fromUtf8("💬"));
	icon->setStyleSheet(QStringLiteral("font-size:30px;"));

	auto *text = new QLabel(discordCard);
	text->setTextFormat(Qt::RichText);
	text->setWordWrap(true);
	text->setText("<b>Join the Discord server</b><br>"
		      "Get help, share ideas,<br>"
		      "and chat with other users.");

	auto *btn = new QPushButton(QStringLiteral("💬 Join Discord"), discordCard);
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

// -----------------------------------------------------------------------------
// Carousel: auto-rotating stack of cards
// -----------------------------------------------------------------------------

QWidget *fly_create_widget_carousel(QWidget *parent)
{
	// Wrapper widget that you add to the dock
	auto *wrapper = new QWidget(parent);
	wrapper->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto *root = new QVBoxLayout(wrapper);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(4);

	// Stacked widget with the cards
	auto *stack = new QStackedWidget(wrapper);
	stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	stack->addWidget(fly_create_discord_card(stack));
	stack->addWidget(fly_create_custom_overlay_card(stack));
	stack->addWidget(fly_create_kofi_card(stack));

	root->addWidget(stack);

	// Dot indicators row (use buttons so they are clickable)
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
		dot->setToolTip(QStringLiteral("Show card %1").arg(i + 1));
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

	// Make sure there is always some graphics effect for animation
	auto *effect = new QGraphicsOpacityEffect(stack);
	effect->setOpacity(1.0);
	stack->setGraphicsEffect(effect);

	// Helper to update dot checked state
	auto updateDots = [stack, dots]() {
		const int idx = stack->currentIndex();
		for (int i = 0; i < dots.size(); ++i) {
			if (!dots[i])
				continue;
			dots[i]->setChecked(i == idx);
		}
	};

	updateDots();

	// Auto-advance timer
	auto *timer = new QTimer(wrapper);
	timer->setInterval(10000); // 10 seconds

	// Smooth fade transition
	auto switchToIndex = [stack, effect, updateDots, wrapper](int targetIndex) {
		if (targetIndex < 0 || targetIndex >= stack->count())
			return;
		if (targetIndex == stack->currentIndex())
			return;

		// Fade-out animation
		auto *fadeOut = new QPropertyAnimation(effect, "opacity", wrapper);
		fadeOut->setDuration(180);
		fadeOut->setStartValue(1.0);
		fadeOut->setEndValue(0.0);

		// Fade-in animation
		auto *fadeIn = new QPropertyAnimation(effect, "opacity", wrapper);
		fadeIn->setDuration(180);
		fadeIn->setStartValue(0.0);
		fadeIn->setEndValue(1.0);

		auto *group = new QSequentialAnimationGroup(wrapper);
		group->addAnimation(fadeOut);
		group->addAnimation(fadeIn);

		// When fade-out ends, swap the page, then fade back in
		QObject::connect(fadeOut, &QPropertyAnimation::finished, stack, [stack, targetIndex, updateDots]() {
			stack->setCurrentIndex(targetIndex);
			updateDots();
		});

		// Start and auto-delete when done
		group->start(QAbstractAnimation::DeleteWhenStopped);
	};

	// Connect dots to direct navigation
	for (int i = 0; i < dots.size(); ++i) {
		auto *dot = dots[i];
		if (!dot)
			continue;

		QObject::connect(dot, &QToolButton::clicked, wrapper, [i, timer, switchToIndex]() {
			if (timer)
				timer->start(); // reset auto-rotate
			switchToIndex(i);
		});
	}

	// Auto-advance handler
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
