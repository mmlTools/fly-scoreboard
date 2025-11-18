#pragma once
#include <QString>

QString fly_normalized_ext_from_mime(const QString &path);
QString fly_copy_logo_to_overlay(const QString &dataDir,
                                 const QString &srcAbs,
                                 const QString &baseName);

bool fly_delete_logo_if_exists(const QString &dataDir,
                               const QString &relPath);

/// Delete all files in overlay/ that start with prefix, e.g. "home" or "guest"
void fly_clean_overlay_prefix(const QString &dataDir,
                              const QString &basePrefix);
