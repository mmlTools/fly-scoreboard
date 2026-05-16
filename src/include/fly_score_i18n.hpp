#pragma once

#include <obs-module.h>

#include <QString>

inline QString fly_i18n(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

