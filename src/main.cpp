#include "game.h"
#include "server.h"
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

/* Minimal JSON string extraction – works for flat {"key":"value"} objects */
static std::string jsonStr(const std::string& json, const std::string& key) {
    std::string patterns[2] = {
        "\"" + key + "\":\"",
        "\"" + key + "\": \""
    };
    for (const auto& pat : patterns) {
        auto pos = json.find(pat);
        if (pos != std::string::npos) {
            pos += pat.size();
            auto end = json.find('"', pos);
            if (end != std::string::npos) return json.substr(pos, end - pos);
        }
    }
    return "";
}

int main() {
    Game game;
    std::mutex mtx;
    HttpServer server;

    server.serveStatic("frontend");

    /* GET  /api/state ------------------------------------------------ */
    server.get("/api/state", [&](const HttpRequest&) -> HttpResponse {
        std::lock_guard<std::mutex> lock(mtx);
        return HttpResponse::json(game.toJson());
    });

    /* POST /api/new-game --------------------------------------------- */
    server.post("/api/new-game", [&](const HttpRequest&) -> HttpResponse {
        std::lock_guard<std::mutex> lock(mtx);
        game.newGame();
        return HttpResponse::json(game.toJson());
    });

    /* POST /api/select ----------------------------------------------- */
    server.post("/api/select", [&](const HttpRequest& req) -> HttpResponse {
        std::lock_guard<std::mutex> lock(mtx);
        std::string category = jsonStr(req.body, "category");
        std::string value    = jsonStr(req.body, "value");
        game.selectIngredient(category, value);
        return HttpResponse::json(game.toJson());
    });

    /* POST /api/undo ------------------------------------------------- */
    server.post("/api/undo", [&](const HttpRequest&) -> HttpResponse {
        std::lock_guard<std::mutex> lock(mtx);
        game.undo();
        return HttpResponse::json(game.toJson());
    });

    /* POST /api/submit ----------------------------------------------- */
    server.post("/api/submit", [&](const HttpRequest&) -> HttpResponse {
        std::lock_guard<std::mutex> lock(mtx);
        bool ok = game.submit();
        std::string j = game.toJson();
        /* Inject "correct" field at the front */
        j = "{\"correct\":" + std::string(ok ? "true" : "false") + "," + j.substr(1);
        return HttpResponse::json(j);
    });

    std::cout << "========================================" << std::endl;
    std::cout << "  Comfy Factory" << std::endl;
    std::cout << "  http://127.0.0.1:9000" << std::endl;
    std::cout << "  Ctrl+C to stop" << std::endl;
    std::cout << "========================================" << std::endl;

    /* Open browser automatically after a short delay */
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
#ifdef _WIN32
        std::system("start http://127.0.0.1:9000");
#elif __APPLE__
        std::system("open http://127.0.0.1:9000");
#else
        std::system("xdg-open http://127.0.0.1:9000 2>/dev/null");
#endif
    }).detach();

    server.listen(9000);
    return 0;
}
