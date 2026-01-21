#pragma once

#include <QString>
#include <QVector>
#include <string>

struct FlyTeam {
	QString title;
	QString subtitle;
	QString logo;
	uint32_t color = 0xFFFFFF;
};

struct FlyTimer {
	QString label;
	QString mode;
	bool running = false;
	long long initial_ms = 0;
	long long remaining_ms = 0;
	long long last_tick_ms = 0;
	bool visible = true;
};

struct FlyCustomField {
	QString label;
	int  home    = 0;
	int  away    = 0;
	bool visible = true;
};

struct FlySingleStat {
	QString label;
	int  value   = 0;
	bool visible = true;
};


struct FlyState {
	FlyTeam home;
	FlyTeam away;

	bool swap_sides      = false;
	bool show_scoreboard = true;

	QVector<FlyCustomField> custom_fields;
	QVector<FlySingleStat> single_stats;
	QVector<FlyTimer> timers;
};

bool     fly_state_read_json(const std::string &base_dir, std::string &out_json);
bool     fly_state_write_json(const std::string &base_dir, const std::string &json);
bool     fly_state_load(const QString &base_dir, FlyState &out);
bool     fly_state_save(const QString &base_dir, const FlyState &st);
QString  fly_data_dir();
bool     fly_ensure_webroot(QString *outBaseDir = nullptr);
FlyState fly_state_make_defaults();
bool     fly_state_reset_defaults(const QString &base_dir);
bool fly_state_ensure_json_exists(const QString &base_dir, const FlyState *writeState = nullptr);
