#pragma once
#include <QString>

int fly_server_start(const QString &base_dir, int preferred_port);
void fly_server_stop();
int fly_server_port();
bool fly_server_is_running();
