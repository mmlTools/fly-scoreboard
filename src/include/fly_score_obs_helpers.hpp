#pragma once
#include <QString>
#include <QStringList>

#include "fly_score_const.hpp"

bool fly_ensure_browser_source_in_current_scene(const QString &urlOrLocalIndex,
                                   const QString &browserSourceName = QString::fromUtf8(kBrowserSourceName));

QStringList fly_list_browser_sources();
