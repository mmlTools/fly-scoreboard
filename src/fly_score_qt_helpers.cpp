#include "fly_score_qt_helpers.hpp"

#include <QDateTime>
#include <QRegularExpression>
#include <QWidget>
#include <QStyle>
#include <QUrl>
#include <QUrlQuery>
#include <QAbstractButton>
#include <QPushButton>

qint64 fly_now_ms()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QIcon fly_themed_icon(QWidget *w, const char *name, QStyle::StandardPixmap fallback)
{
    QIcon ic = QIcon::fromTheme(QString::fromUtf8(name));
    if (!ic.isNull())
        return ic;
    return w->style()->standardIcon(fallback);
}

void fly_style_icon_only_button(QAbstractButton *b,
                                const QIcon &icon,
                                const QString &tooltip,
                                const QSize &iconSize)
{
    b->setText(QString());
    b->setIcon(icon);
    b->setIconSize(iconSize);
    b->setToolTip(tooltip);
    b->setCursor(Qt::PointingHandCursor);
    if (auto *pb = qobject_cast<QPushButton *>(b))
        pb->setFlat(true);
}

qint64 fly_parse_mmss_to_ms(const QString &txt)
{
    static const QRegularExpression re(R"(^\s*(\d+)\s*:\s*([0-5]\d)\s*$)");
    auto m = re.match(txt);
    if (!m.hasMatch())
        return -1;
    const qint64 minutes = m.captured(1).toLongLong();
    const qint64 seconds = m.captured(2).toLongLong();
    return (minutes * 60 + seconds) * 1000;
}

QString fly_format_ms_mmss(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    qint64 total = ms / 1000;
    qint64 m = total / 60;
    qint64 s = total % 60;
    return QString::number(m) + ":" + QString::number(s).rightJustified(2, '0');
}

QString fly_cache_bust_url(const QString &in)
{
    QUrl u(in);
    QUrlQuery q(u);
    q.removeAllQueryItems(QStringLiteral("cb"));
    q.addQueryItem(QStringLiteral("cb"),
                   QString::number(QDateTime::currentMSecsSinceEpoch()));
    u.setQuery(q);
    return u.toString();
}
