#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][state]"
#include "fly_score_log.hpp"

#include "fly_score_state.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
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
	return QDir(base_dir).absolutePath();
}

static QString overlay_plugin_json(const QString &base_dir)
{
	return QDir(overlay_dir_path(base_dir)).filePath(QStringLiteral("plugin.json"));
}

QString fly_data_dir()
{
	return QDir::cleanPath(moduleBaseDirFromConfigFile());
}

bool fly_ensure_webroot(QString *outBaseDir)
{
	const QString base = fly_data_dir();
	const QString webroot = overlay_dir_path(base);
	QDir().mkpath(webroot);
	if (outBaseDir)
		*outBaseDir = base;
	return true;
}

static FlyTimer makeDefaultMainTimer()
{
	FlyTimer t;
	t.label = QStringLiteral("First Half");
	t.mode = QStringLiteral("countdown");
	t.running = false;
	t.initial_ms = 0;
	t.remaining_ms = 0;
	t.last_tick_ms = 0;
	return t;
}

static void ensureDefaultCustomFields(FlyState &st)
{
	auto ensureAt = [&](int index, const QString &label) {
		if (st.custom_fields.size() <= index)
			st.custom_fields.resize(index + 1);

		FlyCustomField &cf = st.custom_fields[index];
		if (cf.label.isEmpty())
			cf.label = label;
	};

	ensureAt(0, QStringLiteral("Points"));
	ensureAt(1, QStringLiteral("Score"));
}

static QJsonObject timerToJson(const FlyTimer &t)
{
	QJsonObject o;
	o["label"] = t.label;
	o["mode"] = t.mode;
	o["running"] = t.running;
	o["initial_ms"] = QString::number(t.initial_ms);
	o["remaining_ms"] = QString::number(t.remaining_ms);
	o["last_tick_ms"] = QString::number(t.last_tick_ms);
	return o;
}

static FlyTimer timerFromJson(const QJsonObject &o)
{
	FlyTimer t;
	t.label = o.value("label").toString();
	t.mode = o.value("mode").toString("countdown");
	t.running = o.value("running").toBool(false);
	t.initial_ms = o.value("initial_ms").toString("0").toLongLong();
	t.remaining_ms = o.value("remaining_ms").toString("0").toLongLong();
	t.last_tick_ms = o.value("last_tick_ms").toString("0").toLongLong();
	return t;
}

static QJsonObject toJson(const FlyState &stIn)
{
	FlyState st = stIn;

	ensureDefaultCustomFields(st);

	QJsonObject j;
	j["version"] = 3;

	QJsonObject srv;
	srv["port"] = st.server_port;
	j["server"] = srv;

	auto teamToJson = [](const FlyTeam &tm) {
		QJsonObject o;
		o["title"] = tm.title;
		o["subtitle"] = tm.subtitle;
		o["logo"] = tm.logo;
		return o;
	};

	j["home"] = teamToJson(st.home);
	j["away"] = teamToJson(st.away);

	j["swap_sides"] = st.swap_sides;
	j["show_scoreboard"] = st.show_scoreboard;

	QJsonArray cfArr;
	for (const auto &cf : st.custom_fields) {
		QJsonObject o;
		o["label"] = cf.label;
		o["home"] = cf.home;
		o["away"] = cf.away;
		o["visible"] = cf.visible;
		cfArr.append(o);
	}
	j["custom_fields"] = cfArr;

	QJsonArray timersArr;
	if (st.timers.isEmpty()) {
		timersArr.append(timerToJson(makeDefaultMainTimer()));
	} else {
		for (const auto &tm : st.timers) {
			timersArr.append(timerToJson(tm));
		}
	}
	j["timers"] = timersArr;

	return j;
}

static bool fromJson(const QJsonObject &j, FlyState &st)
{
	st.server_port = j.value("server").toObject().value("port").toInt(8089);

	auto readTeam = [](const QJsonObject &o) {
		FlyTeam tm;
		tm.title = o.value("title").toString();
		tm.subtitle = o.value("subtitle").toString();
		tm.logo = o.value("logo").toString();
		return tm;
	};

	st.home = readTeam(j.value("home").toObject());
	st.away = readTeam(j.value("away").toObject());

	st.swap_sides = j.value("swap_sides").toBool(false);
	st.show_scoreboard = j.value("show_scoreboard").toBool(true);

	st.custom_fields.clear();
	const auto cfArr = j.value("custom_fields").toArray();
	for (const auto &v : cfArr) {
		const auto o = v.toObject();
		FlyCustomField cf;
		cf.label = o.value("label").toString();
		cf.home = o.value("home").toInt(0);
		cf.away = o.value("away").toInt(0);
		cf.visible = o.value("visible").toBool(true);
		st.custom_fields.push_back(cf);
	}

	ensureDefaultCustomFields(st);

	st.timers.clear();
	const auto timersArr = j.value("timers").toArray();

	if (!timersArr.isEmpty()) {
		st.timers.reserve(timersArr.size());
		for (const auto &v : timersArr) {
			const auto o = v.toObject();
			st.timers.push_back(timerFromJson(o));
		}
	} else {
		const auto tObj = j.value("timer").toObject();
		if (!tObj.isEmpty()) {
			FlyTimer main = timerFromJson(tObj);
			st.timers.push_back(main);
		}
	}

	if (st.timers.isEmpty()) {
		st.timers.push_back(makeDefaultMainTimer());
	}

	FlyTimer &main = st.timers[0];
	if (main.mode.isEmpty())
		main.mode = QStringLiteral("countdown");

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
	const QString path = overlay_plugin_json(base_dir);
	return write_one_json(path, doc);
}

FlyState fly_state_make_defaults()
{
	FlyState st;
	st.server_port = 8089;
	st.home = FlyTeam{};
	st.away = FlyTeam{};
	st.swap_sides = false;
	st.show_scoreboard = true;

	st.custom_fields.clear();
	st.timers.clear();

	ensureDefaultCustomFields(st);

	st.timers.push_back(makeDefaultMainTimer());

	return st;
}

bool fly_state_reset_defaults(const QString &base_dir)
{
	const QString pj = overlay_plugin_json(base_dir);
	QFile::remove(pj);

	const FlyState def = fly_state_make_defaults();
	return fly_state_save(base_dir, def);
}
