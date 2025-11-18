#pragma once
#include <QString>
#include <QVector>
#include <string>

struct FlyTeam {
	QString title;
	QString subtitle;
	QString logo;
	int score = 0;
	int rounds = 0;
};

struct FlyTimer {
	QString mode;
	bool running      = false;
	long long initial_ms   = 0;
	long long remaining_ms = 0;
	long long last_tick_ms = 0;
};

struct FlyCustomField {
	QString label;
	int home   = 0;
	int away   = 0;
	bool visible = true;
};

struct FlyState {
	int server_port = 8089;
	QString time_label = "First Half";
	FlyTimer timer;
	FlyTeam home;
	FlyTeam away;

	bool swap_sides      = false;
	bool show_scoreboard = true;

	// Show/hide wins (rounds)
	bool show_rounds = true;

	// Custom numeric fields
	QVector<FlyCustomField> custom_fields;
};

bool fly_state_read_json(const std::string &base_dir, std::string &out_json);
bool fly_state_write_json(const std::string &base_dir, const std::string &json);
bool fly_state_load(const QString &base_dir, FlyState &out);
bool fly_state_save(const QString &base_dir, const FlyState &st);
QString fly_data_dir();
bool fly_ensure_webroot(QString *outBaseDir = nullptr);
FlyState fly_state_make_defaults();
bool fly_state_reset_defaults(const QString &base_dir);
