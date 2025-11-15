#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif

#define LOG_TAG "[fly-scoreboard][server]"
#include "fly_score_log.hpp"
#include "fly_score_server.hpp"

#include <thread>
#include <atomic>
#include <memory>
#include <cstring>

#include <QString>
#include <QFileInfo>

#include "httplib.h"

static std::unique_ptr<httplib::Server> g_srv;
static std::thread g_thread;
static std::atomic<int> g_port{0};
static std::string g_doc_root;

static void register_routes(httplib::Server &srv)
{
	srv.set_logger([](const httplib::Request &req, const httplib::Response &res) {
		LOGI("%s %s -> %d", req.method.c_str(), req.path.c_str(), res.status);
	});
	srv.set_error_handler([](const httplib::Request &req, httplib::Response &res) {
		LOGW("ERROR %d for %s %s", res.status, req.method.c_str(), req.path.c_str());
	});

	srv.Get("/__ow/health", [](const httplib::Request &, httplib::Response &res) {
		res.set_content("ok", "text/plain; charset=utf-8");
	});

	if (!srv.set_mount_point("/", g_doc_root.c_str())) {
		LOGE("Failed to mount docRoot '%s' at '/'", g_doc_root.c_str());
	}

	srv.set_post_routing_handler([](const httplib::Request &req, httplib::Response &res) {
		const auto &p = req.path;
		auto ends = [&](const char *s) {
			size_t n = strlen(s);
			return p.size() >= n && p.compare(p.size() - n, n, s) == 0;
		};
		if (ends(".js") || ends(".json"))
			res.set_header("Cache-Control", "no-store, must-revalidate");
		else
			res.set_header("Cache-Control", "no-cache");
	});
}

int fly_server_start(const QString &base_dir_q, int preferred_port)
{
	if (g_srv)
		return g_port.load();

	g_doc_root = QFileInfo(base_dir_q).absoluteFilePath().toStdString();
	LOGI("server docRoot: %s", g_doc_root.c_str());

	auto srv = std::make_unique<httplib::Server>();
	srv->set_tcp_nodelay(true);
	srv->set_read_timeout(5, 0);
	srv->set_write_timeout(5, 0);

	register_routes(*srv);

	const char *hosts[] = {"127.0.0.1", "0.0.0.0"};
	int port = 0;
	std::string host_bound;

	for (auto host : hosts) {
		for (int p = preferred_port; p < preferred_port + 10; ++p) {
			if (srv->bind_to_port(host, p)) {
				port = p;
				host_bound = host;
				break;
			}
		}
		if (port)
			break;
	}

	if (!port) {
		LOGW("HTTP server failed to bind any port in [%d..%d] on 127.0.0.1/0.0.0.0", preferred_port,
		     preferred_port + 9);
		return 0;
	}

	g_port = port;
	g_srv = std::move(srv);

	g_thread = std::thread([host_bound]() {
		LOGI("Web server START on http://%s:%d (doc_root=%s)", host_bound.c_str(), g_port.load(),
		     g_doc_root.c_str());
		g_srv->listen_after_bind();
		LOGI("Web server LOOP ended (port %d)", g_port.load());
	});

	bool ok = false;
	for (int i = 0; i < 10; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		httplib::Client cli("127.0.0.1", g_port.load());
		cli.set_keep_alive(false);
		cli.set_connection_timeout(0, 200 * 1000);
		cli.set_read_timeout(0, 200 * 1000);
		cli.set_write_timeout(0, 200 * 1000);
		if (auto res = cli.Get("/__ow/health")) {
			if (res->status == 200) {
				ok = true;
				break;
			}
		}
	}
	if (!ok)
		LOGW("Self-test could not reach /__ow/health on :%d", g_port.load());
	else
		LOGI("Self-test OK on :%d", g_port.load());

	return port;
}

void fly_server_stop()
{
	if (!g_srv)
		return;
	LOGI("Web server STOP requested (port %d)", g_port.load());
	g_srv->stop();
	if (g_thread.joinable())
		g_thread.join();
	g_srv.reset();
	g_port = 0;
}

int fly_server_port()
{
	return g_port.load();
}

bool fly_server_is_running()
{
	return g_srv && g_srv->is_running();
}
