#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][state]"
#include "fly_score_state.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

static QString moduleBaseDirFromConfigFile()
{
	char *p = obs_module_config_path("plugin.json");
	QString filePath = p ? QString::fromUtf8(p) : QString();
	if (p)
		bfree(p);
	return filePath.isEmpty() ? QDir::homePath() : QFileInfo(filePath).absolutePath();
}

static QString overlay_dir_path(const QString &base_dir)
{
	return QDir(base_dir).filePath("overlay");
}

static QString overlay_plugin_json(const QString &base_dir)
{
	return QDir(overlay_dir_path(base_dir)).filePath("plugin.json");
}

QString fly_data_dir()
{
	return QDir::cleanPath(moduleBaseDirFromConfigFile());
}

bool fly_ensure_webroot(QString *outBaseDir)
{
	const QString base = fly_data_dir();
	const QString overlay = overlay_dir_path(base);
	QDir().mkpath(overlay);
	if (outBaseDir)
		*outBaseDir = base;
	return true;
}

static QJsonObject toJson(const FlyState &st)
{
	QJsonObject j;
	j["version"] = 1;

	QJsonObject srv;
	srv["port"] = st.server_port;
	j["server"] = srv;
	j["time_label"] = st.time_label;

	QJsonObject t;
	t["mode"] = st.timer.mode;
	t["running"] = st.timer.running;
	t["initial_ms"] = QString::number(st.timer.initial_ms);
	t["remaining_ms"] = QString::number(st.timer.remaining_ms);
	t["last_tick_ms"] = QString::number(st.timer.last_tick_ms);
	j["timer"] = t;

	auto team = [](const FlyTeam &tm) {
		QJsonObject o;
		o["title"] = tm.title;
		o["subtitle"] = tm.subtitle;
		o["logo"] = tm.logo;
		o["score"] = tm.score;
		o["rounds"] = tm.rounds;
		return o;
	};
	j["home"] = team(st.home);
	j["away"] = team(st.away);

	j["swap_sides"] = st.swap_sides;
	j["show_scoreboard"] = st.show_scoreboard;

	return j;
}

static bool fromJson(const QJsonObject &j, FlyState &st)
{
	st.server_port = j.value("server").toObject().value("port").toInt(8089);
	st.time_label = j.value("time_label").toString("First Half");

	const auto t = j.value("timer").toObject();
	st.timer.mode = t.value("mode").toString("countdown");
	st.timer.running = t.value("running").toBool(false);
	st.timer.initial_ms = t.value("initial_ms").toString("0").toLongLong();
	st.timer.remaining_ms = t.value("remaining_ms").toString("0").toLongLong();
	st.timer.last_tick_ms = t.value("last_tick_ms").toString("0").toLongLong();

	auto readTeam = [](const QJsonObject &o) {
		FlyTeam tm;
		tm.title = o.value("title").toString();
		tm.subtitle = o.value("subtitle").toString();
		tm.logo = o.value("logo").toString();
		tm.score = o.value("score").toInt(0);
		tm.rounds = o.value("rounds").toInt(0);
		return tm;
	};
	st.home = readTeam(j.value("home").toObject());
	st.away = readTeam(j.value("away").toObject());

	st.swap_sides = j.value("swap_sides").toBool(false);
	st.show_scoreboard = j.value("show_scoreboard").toBool(true);

	return true;
}

bool fly_state_read_json(const std::string &base_dir_s, std::string &out_json)
{
	const QString base_dir = QString::fromStdString(base_dir_s);
	const QString path = overlay_plugin_json(base_dir);
	QFile f(path);
	if (!f.exists() || !f.open(QIODevice::ReadOnly))
		return false;
	const QByteArray data = f.readAll();
	out_json.assign(data.constData(), data.size());
	return true;
}

bool fly_state_write_json(const std::string &base_dir_s, const std::string &json)
{
	const QString base_dir = QString::fromStdString(base_dir_s);
	const QString path = overlay_plugin_json(base_dir);
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(QByteArray::fromStdString(json));
	return true;
}

static bool write_one_json(const QString &path, const QJsonDocument &doc)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(doc.toJson(QJsonDocument::Compact));
	return true;
}

bool fly_state_load(const QString &base_dir, FlyState &out)
{
	const QString path = overlay_plugin_json(base_dir);
	QFile f(path);
	if (!f.exists() || !f.open(QIODevice::ReadOnly))
		return false;
	const auto doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isObject())
		return false;
	return fromJson(doc.object(), out);
}

bool fly_state_save(const QString &base_dir, const FlyState &st)
{
	const QJsonDocument doc(toJson(st));
	const QString overlayPath = overlay_plugin_json(base_dir);
	return write_one_json(overlayPath, doc);
}

FlyState fly_state_make_defaults()
{
	FlyState st;
	st.server_port = 8089;
	st.time_label = "First Half";
	st.timer.mode = "countdown";
	st.timer.running = false;
	st.timer.initial_ms = 0;
	st.timer.remaining_ms = 0;
	st.timer.last_tick_ms = 0;
	st.home = FlyTeam{};
	st.away = FlyTeam{};
	st.swap_sides = false;
	st.show_scoreboard = true;
	return st;
}

bool fly_state_reset_defaults(const QString &base_dir)
{
	const QString pj = overlay_plugin_json(base_dir);
	QFile::remove(pj);
	const FlyState def = fly_state_make_defaults();
	return fly_state_save(base_dir, def);
}
