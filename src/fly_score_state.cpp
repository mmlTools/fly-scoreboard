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
	t.visible = true;
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


static void ensureDefaultSingleStats(FlyState &st)
{
	// Single stats are independent from Home/Guests (e.g., Period/Round).
	auto ensureAt = [&](int index, const QString &label) {
		if (st.single_stats.size() <= index)
			st.single_stats.resize(index + 1);

		FlySingleStat &ss = st.single_stats[index];
		if (ss.label.isEmpty())
			ss.label = label;
	};

	ensureAt(0, QStringLiteral("PERIOD"));
	ensureAt(1, QStringLiteral("ROUND"));
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
	o["visible"] = t.visible;
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
	t.visible = o.value("visible").toBool(true);
	return t;
}

static QJsonObject toJson(const FlyState &stIn)
{
    FlyState st = stIn;

    ensureDefaultCustomFields(st);
    ensureDefaultSingleStats(st);

    QJsonObject j;
    j["version"] = 4;

    // ---------------------------------------------------------------------
    // Server
    // ---------------------------------------------------------------------
    // Teams
    // ---------------------------------------------------------------------
    auto teamToJson = [](const FlyTeam &tm) {
        QJsonObject o;
        o["title"] = tm.title;
        o["subtitle"] = tm.subtitle;
        o["logo"]     = tm.logo;

        // Store as number (best practice for OBS integration)
        o["color"]    = QString::number(tm.color);

        return o;
    };

    j["home"] = teamToJson(st.home);
    j["away"] = teamToJson(st.away);

    // ---------------------------------------------------------------------
    // Flags
    // ---------------------------------------------------------------------
    j["swap_sides"]      = st.swap_sides;
    j["show_scoreboard"] = st.show_scoreboard;

    // ---------------------------------------------------------------------
    // Custom Fields
    // ---------------------------------------------------------------------
    QJsonArray cfArr;
    for (const auto &cf : st.custom_fields) {
        QJsonObject o;
        o["label"]   = cf.label;
        o["home"]    = cf.home;
        o["away"]    = cf.away;
        o["visible"] = cf.visible;
        cfArr.append(o);
    }
    j["custom_fields"] = cfArr;

    // -----------------------------
    // Single stats
    // -----------------------------
    QJsonArray ssArr;
    for (const auto &ss : st.single_stats) {
        QJsonObject o;
        o["label"] = ss.label;
        o["value"] = ss.value;
        o["visible"] = ss.visible;
        ssArr.append(o);
    }
    j["single_stats"] = ssArr;

    // ---------------------------------------------------------------------
    // Timers
    // ---------------------------------------------------------------------
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
    // ---------------------------------------------------------------------
    // Server
    // ---------------------------------------------------------------------
    // Helper: robust color reader
    // ---------------------------------------------------------------------
    auto readColor = [](const QJsonObject &o, const char *key, uint32_t def = 0xFFFFFF) -> uint32_t {
        const QJsonValue v = o.value(key);

        if (v.isDouble())
            return static_cast<uint32_t>(v.toInt());

        if (v.isString()) {
            QString s = v.toString().trimmed();
            if (s.startsWith("0x", Qt::CaseInsensitive))
                return s.mid(2).toUInt(nullptr, 16);
            return s.toUInt(nullptr, 10);
        }

        return def;
    };

    // ---------------------------------------------------------------------
    // Teams
    // ---------------------------------------------------------------------
    auto readTeam = [&](const QJsonObject &o) {
        FlyTeam tm;
        tm.title    = o.value("title").toString();
        tm.subtitle = o.value("subtitle").toString();
        tm.logo     = o.value("logo").toString();
        tm.color    = readColor(o, "color", 0xFFFFFF);
        return tm;
    };

    st.home = readTeam(j.value("home").toObject());
    st.away = readTeam(j.value("away").toObject());

    // ---------------------------------------------------------------------
    // Flags
    // ---------------------------------------------------------------------
    st.swap_sides      = j.value("swap_sides").toBool(false);
    st.show_scoreboard = j.value("show_scoreboard").toBool(true);

    // ---------------------------------------------------------------------
    // Custom Fields
    // ---------------------------------------------------------------------
    st.custom_fields.clear();

    const QJsonValue cfVal = j.value("custom_fields");
    if (cfVal.isArray()) {
        const QJsonArray cfArr = cfVal.toArray();
        st.custom_fields.reserve(cfArr.size());

        for (const QJsonValue v : cfArr) {
            if (!v.isObject())
                continue;

            const QJsonObject o = v.toObject();

            FlyCustomField cf;
            cf.label   = o.value("label").toString();
            cf.home    = o.value("home").toInt(0);
            cf.away    = o.value("away").toInt(0);
            cf.visible = o.value("visible").toBool(true);

            st.custom_fields.push_back(cf);
        }
    }

    // ---------------------------------------------------------------------
    // Single Stats
    // ---------------------------------------------------------------------
    st.single_stats.clear();

    const QJsonValue ssVal = j.value("single_stats");
    if (ssVal.isArray()) {
        const QJsonArray ssArr = ssVal.toArray();
        st.single_stats.reserve(ssArr.size());

        for (const QJsonValue v : ssArr) {
            if (!v.isObject())
                continue;

            const QJsonObject o = v.toObject();

            FlySingleStat ss;
            ss.label   = o.value("label").toString();
            ss.value   = o.value("value").toInt(0);
            ss.visible = o.value("visible").toBool(true);

            st.single_stats.push_back(ss);
        }
    }

    ensureDefaultSingleStats(st);

    ensureDefaultCustomFields(st);

    // ---------------------------------------------------------------------
    // Timers
    // ---------------------------------------------------------------------
    st.timers.clear();

    const QJsonValue timersVal = j.value("timers");
    if (timersVal.isArray()) {
        const QJsonArray timersArr = timersVal.toArray();

        if (!timersArr.isEmpty()) {
            st.timers.reserve(timersArr.size());

            for (const QJsonValue v : timersArr) {
                if (!v.isObject())
                    continue;

                st.timers.push_back(timerFromJson(v.toObject()));
            }
        }
    } else {
        const QJsonObject tObj = j.value("timer").toObject();
        if (!tObj.isEmpty())
            st.timers.push_back(timerFromJson(tObj));
    }

    if (st.timers.isEmpty())
        st.timers.push_back(makeDefaultMainTimer());

    FlyTimer &main = st.timers[0];
    if (main.mode.isEmpty())
        main.mode = "countdown";

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
	st.home = FlyTeam{};
	st.away = FlyTeam{};
	st.swap_sides = false;
	st.show_scoreboard = true;

	st.custom_fields.clear();
	st.single_stats.clear();
	st.timers.clear();

	ensureDefaultCustomFields(st);
	ensureDefaultSingleStats(st);

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

bool fly_state_ensure_json_exists(const QString &base_dir, const FlyState *writeState)
{
	const QString path = overlay_plugin_json(base_dir);
	if (QFileInfo::exists(path))
		return true;

	FlyState st = writeState ? *writeState : fly_state_make_defaults();
	const bool ok = fly_state_save(base_dir, st);

	if (ok)
		LOGI("Created default plugin.json: %s", path.toUtf8().constData());
	else
		LOGW("Failed to create default plugin.json: %s", path.toUtf8().constData());

	return ok;
}
