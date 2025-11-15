#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][hotkeys]"
#include "fly_score_log.hpp"

#include "fly_score_hotkeys.hpp"

#include <obs-module.h>
#include <obs.h>

#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QMetaObject>
#include <QCoreApplication>
#include <functional>
#include <cstring>

#include "fly_score_dock.hpp"

static inline void ui_invoke(std::function<void()> fn)
{
	QObject *ctx = reinterpret_cast<QObject *>(fly_get_dock());
	if (!ctx)
		ctx = QCoreApplication::instance();

	QMetaObject::invokeMethod(ctx, [f = std::move(fn)]() { f(); }, Qt::QueuedConnection);
}

#ifdef ENABLE_FRONTEND_API

static obs_hotkey_id hk_home_score_inc = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_home_score_dec = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_away_score_inc = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_away_score_dec = OBS_INVALID_HOTKEY_ID;

static obs_hotkey_id hk_home_rounds_inc = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_home_rounds_dec = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_away_rounds_inc = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_away_rounds_dec = OBS_INVALID_HOTKEY_ID;

static obs_hotkey_id hk_toggle_swap = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_toggle_show = OBS_INVALID_HOTKEY_ID;

static constexpr const char *kSaveRoot = "fly_score";
static constexpr const char *kHomeScoreIncKey = "home_score_inc";
static constexpr const char *kHomeScoreDecKey = "home_score_dec";
static constexpr const char *kAwayScoreIncKey = "away_score_inc";
static constexpr const char *kAwayScoreDecKey = "away_score_dec";
static constexpr const char *kHomeWinsIncKey = "home_rounds_inc";
static constexpr const char *kHomeWinsDecKey = "home_rounds_dec";
static constexpr const char *kAwayWinsIncKey = "away_rounds_inc";
static constexpr const char *kAwayWinsDecKey = "away_rounds_dec";
static constexpr const char *kToggleSwapKey = "toggle_swap";
static constexpr const char *kToggleShowKey = "toggle_show";

static obs_data_t *g_deferred = nullptr;

#endif // ENABLE_FRONTEND_API

// -----------------------------------------------------------------------------
// Callbacks (frontend-agnostic: they just talk to the dock)
// -----------------------------------------------------------------------------

static void cb_home_score_inc(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: home score +1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpHomeScore(+1);
	});
}

static void cb_home_score_dec(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: home score -1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpHomeScore(-1);
	});
}

static void cb_away_score_inc(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: guests score +1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpAwayScore(+1);
	});
}

static void cb_away_score_dec(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: guests score -1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpAwayScore(-1);
	});
}

static void cb_home_rounds_inc(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: home wins +1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpHomeRounds(+1);
	});
}

static void cb_home_rounds_dec(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: home wins -1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpHomeRounds(-1);
	});
}

static void cb_away_rounds_inc(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: guests wins +1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpAwayRounds(+1);
	});
}

static void cb_away_rounds_dec(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: guests wins -1 (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->bumpAwayRounds(-1);
	});
}

static void cb_toggle_swap(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: toggle swap (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->toggleSwap();
	});
}

static void cb_toggle_show(void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	LOGI("Hotkey pressed: toggle show (id=%lld)", (long long)id);
	ui_invoke([] {
		if (auto *d = fly_get_dock())
			d->toggleShow();
	});
}

// -----------------------------------------------------------------------------
// Frontend API–backed helpers
// -----------------------------------------------------------------------------

static void register_ids_once()
{
#ifdef ENABLE_FRONTEND_API
	if (hk_home_score_inc != OBS_INVALID_HOTKEY_ID)
		return;

	hk_home_score_inc = obs_hotkey_register_frontend("fly_score_home_score_inc", "Fly Score: Home Score +1",
							 cb_home_score_inc, nullptr);
	hk_home_score_dec = obs_hotkey_register_frontend("fly_score_home_score_dec", "Fly Score: Home Score -1",
							 cb_home_score_dec, nullptr);

	hk_away_score_inc = obs_hotkey_register_frontend("fly_score_away_score_inc", "Fly Score: Guests Score +1",
							 cb_away_score_inc, nullptr);
	hk_away_score_dec = obs_hotkey_register_frontend("fly_score_away_score_dec", "Fly Score: Guests Score -1",
							 cb_away_score_dec, nullptr);

	hk_home_rounds_inc = obs_hotkey_register_frontend("fly_score_home_rounds_inc", "Fly Score: Home Wins +1",
							  cb_home_rounds_inc, nullptr);
	hk_home_rounds_dec = obs_hotkey_register_frontend("fly_score_home_rounds_dec", "Fly Score: Home Wins -1",
							  cb_home_rounds_dec, nullptr);

	hk_away_rounds_inc = obs_hotkey_register_frontend("fly_score_away_rounds_inc", "Fly Score: Guests Wins +1",
							  cb_away_rounds_inc, nullptr);
	hk_away_rounds_dec = obs_hotkey_register_frontend("fly_score_away_rounds_dec", "Fly Score: Guests Wins -1",
							  cb_away_rounds_dec, nullptr);

	hk_toggle_swap = obs_hotkey_register_frontend("fly_score_toggle_swap", "Fly Score: Swap Home/Guests",
						      cb_toggle_swap, nullptr);
	hk_toggle_show = obs_hotkey_register_frontend("fly_score_toggle_show", "Fly Score: Show/Hide Scoreboard",
						      cb_toggle_show, nullptr);

	LOGI("Registered frontend hotkeys (IDs created).");
#else
	LOGI("Frontend API not available; skipping hotkey registration.");
#endif
}

static void save_all(obs_data_t *root)
{
	if (!root)
		return;

#ifdef ENABLE_FRONTEND_API
	obs_data_t *out = obs_data_create();

	auto put = [&](const char *key, obs_hotkey_id id) {
		if (id == OBS_INVALID_HOTKEY_ID)
			return;
		obs_data_array_t *arr = obs_hotkey_save(id);
		if (!arr)
			return;
		obs_data_set_array(out, key, arr);
		obs_data_array_release(arr);
	};

	put(kHomeScoreIncKey, hk_home_score_inc);
	put(kHomeScoreDecKey, hk_home_score_dec);
	put(kAwayScoreIncKey, hk_away_score_inc);
	put(kAwayScoreDecKey, hk_away_score_dec);
	put(kHomeWinsIncKey, hk_home_rounds_inc);
	put(kHomeWinsDecKey, hk_home_rounds_dec);
	put(kAwayWinsIncKey, hk_away_rounds_inc);
	put(kAwayWinsDecKey, hk_away_rounds_dec);
	put(kToggleSwapKey, hk_toggle_swap);
	put(kToggleShowKey, hk_toggle_show);

	obs_data_set_obj(root, kSaveRoot, out);
	obs_data_release(out);

	LOGI("Hotkey bindings saved to profile.");
#else
	UNUSED_PARAMETER(root);
#endif
}

static obs_data_t *deep_copy_bindings_obj(obs_data_t *in)
{
	if (!in)
		return nullptr;

#ifdef ENABLE_FRONTEND_API
	obs_data_t *copy = obs_data_create();

	auto copy_array = [&](const char *key) {
		obs_data_array_t *a = obs_data_get_array(in, key);
		if (!a)
			return;
		obs_data_array_t *out_a = obs_data_array_create();
		const size_t n = obs_data_array_count(a);
		for (size_t i = 0; i < n; ++i) {
			obs_data_t *item = obs_data_array_item(a, i);
			obs_data_array_push_back(out_a, item);
			obs_data_release(item);
		}
		obs_data_set_array(copy, key, out_a);
		obs_data_array_release(out_a);
		obs_data_array_release(a);
	};

	copy_array(kHomeScoreIncKey);
	copy_array(kHomeScoreDecKey);
	copy_array(kAwayScoreIncKey);
	copy_array(kAwayScoreDecKey);
	copy_array(kHomeWinsIncKey);
	copy_array(kHomeWinsDecKey);
	copy_array(kAwayWinsIncKey);
	copy_array(kAwayWinsDecKey);
	copy_array(kToggleSwapKey);
	copy_array(kToggleShowKey);

	return copy;
#else
	UNUSED_PARAMETER(in);
	return nullptr;
#endif
}

static void capture_for_deferred_restore(obs_data_t *root)
{
	if (!root)
		return;

#ifdef ENABLE_FRONTEND_API
	if (g_deferred) {
		obs_data_release(g_deferred);
		g_deferred = nullptr;
	}

	obs_data_t *src = obs_data_get_obj(root, kSaveRoot);
	if (!src)
		return;

	g_deferred = deep_copy_bindings_obj(src);
	obs_data_release(src);

	if (g_deferred)
		LOGI("Captured hotkey bindings for deferred restore.");
#else
	UNUSED_PARAMETER(root);
#endif
}

#ifdef ENABLE_FRONTEND_API
static void frontend_save_cb(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		save_all(save_data);
	} else {
		capture_for_deferred_restore(save_data);
	}
}
#endif

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void fly_hotkeys_init()
{
#ifdef ENABLE_FRONTEND_API
	register_ids_once();
	obs_frontend_add_save_callback(frontend_save_cb, nullptr);
#else
	LOGI("fly_hotkeys_init: frontend API not available; hotkeys disabled.");
#endif
}

void fly_hotkeys_apply_deferred_restore()
{
#ifdef ENABLE_FRONTEND_API
	if (!g_deferred)
		return;

	auto apply = [&](const char *key, obs_hotkey_id id) {
		if (id == OBS_INVALID_HOTKEY_ID)
			return;
		obs_data_array_t *arr = obs_data_get_array(g_deferred, key);
		if (!arr)
			return;
		obs_hotkey_load(id, arr);
		obs_data_array_release(arr);
	};

	apply(kHomeScoreIncKey, hk_home_score_inc);
	apply(kHomeScoreDecKey, hk_home_score_dec);
	apply(kAwayScoreIncKey, hk_away_score_inc);
	apply(kAwayScoreDecKey, hk_away_score_dec);
	apply(kHomeWinsIncKey, hk_home_rounds_inc);
	apply(kHomeWinsDecKey, hk_home_rounds_dec);
	apply(kAwayWinsIncKey, hk_away_rounds_inc);
	apply(kAwayWinsDecKey, hk_away_rounds_dec);
	apply(kToggleSwapKey, hk_toggle_swap);
	apply(kToggleShowKey, hk_toggle_show);

	obs_data_release(g_deferred);
	g_deferred = nullptr;

	LOGI("Deferred hotkey bindings applied.");
#else
	LOGI("fly_hotkeys_apply_deferred_restore: frontend API not available; nothing to do.");
#endif
}

void fly_hotkeys_shutdown()
{
#ifdef ENABLE_FRONTEND_API
	if (g_deferred) {
		obs_data_release(g_deferred);
		g_deferred = nullptr;
	}

	obs_frontend_remove_save_callback(frontend_save_cb, nullptr);

	if (hk_home_score_inc != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_home_score_inc);
	if (hk_home_score_dec != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_home_score_dec);
	if (hk_away_score_inc != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_away_score_inc);
	if (hk_away_score_dec != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_away_score_dec);
	if (hk_home_rounds_inc != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_home_rounds_inc);
	if (hk_home_rounds_dec != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_home_rounds_dec);
	if (hk_away_rounds_inc != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_away_rounds_inc);
	if (hk_away_rounds_dec != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_away_rounds_dec);
	if (hk_toggle_swap != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_toggle_swap);
	if (hk_toggle_show != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_toggle_show);

	hk_home_score_inc = hk_home_score_dec = OBS_INVALID_HOTKEY_ID;
	hk_away_score_inc = hk_away_score_dec = OBS_INVALID_HOTKEY_ID;
	hk_home_rounds_inc = hk_home_rounds_dec = OBS_INVALID_HOTKEY_ID;
	hk_away_rounds_inc = hk_away_rounds_dec = OBS_INVALID_HOTKEY_ID;
	hk_toggle_swap = hk_toggle_show = OBS_INVALID_HOTKEY_ID;

	LOGI("Hotkey system shut down.");
#else
	LOGI("fly_hotkeys_shutdown: frontend API not available; nothing to clean up.");
#endif
}
