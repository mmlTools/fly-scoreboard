#include "fly_score_kofi_widget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QSizePolicy>

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
    kofiText->setText(
        "<b>Enjoying this plugin?</b><br>You can support development on Ko-fi.");

    auto *kofiBtn = new QPushButton(QStringLiteral("☕ Buy me a Ko-fi"), kofiCard);
    kofiBtn->setCursor(Qt::PointingHandCursor);
    kofiBtn->setMinimumHeight(28);
    kofiBtn->setStyleSheet(
        "QPushButton { background: #29abe0; color:white; border:none; "
        "border-radius:6px; padding:6px 10px; font-weight:600; }"
        "QPushButton:hover { background: #1e97c6; }"
        "QPushButton:pressed { background: #1984ac; }");

    QObject::connect(kofiBtn, &QPushButton::clicked, kofiCard, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://ko-fi.com/mmltech")));
    });

    kofiLay->addWidget(kofiIcon, 0, Qt::AlignVCenter);
    kofiLay->addWidget(kofiText, 1);
    kofiLay->addWidget(kofiBtn, 0, Qt::AlignVCenter);

    kofiCard->setStyleSheet(
        "#kofiCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "    stop:0 #2a2d30, stop:1 #1e2124);"
        "  border:1px solid #3a3d40; border-radius:10px; padding:6px; }"
        "#kofiCard QLabel { color:#ffffff; }");

    return kofiCard;
}
