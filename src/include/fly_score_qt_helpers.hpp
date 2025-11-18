#pragma once

#include <QString>
#include <QIcon>
#include <QSize>
#include <QStyle>

class QWidget;
class QAbstractButton;

qint64 fly_now_ms();

QIcon fly_themed_icon(QWidget *w,
                      const char *name,
                      QStyle::StandardPixmap fallback);

void fly_style_icon_only_button(QAbstractButton *b,
                                const QIcon &icon,
                                const QString &tooltip,
                                const QSize &iconSize = QSize(20, 20));

qint64 fly_parse_mmss_to_ms(const QString &txt);
QString fly_format_ms_mmss(qint64 ms);

/// Adds/updates cb=<timestamp> query param
QString fly_cache_bust_url(const QString &in);
