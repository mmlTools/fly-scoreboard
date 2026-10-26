#pragma once

#include <QIcon>

class QWidget;

enum class FlyIcon { Trash, Keyboard, Information, Folder, FolderAdd, Plus, Minus, Play, Pause, Reset, Heart, Chat };

QIcon fly_icon(FlyIcon icon, QWidget *context = nullptr);
