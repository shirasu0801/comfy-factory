#include "server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cstring>
#include <cstdio>

HttpServer::HttpServer() : listenSocket_(INVALID_SOCKET), initialized_(false) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[ERROR] WSAStartup failed" << std::endl;
        return;
    }
#endif
    initialized_ = true;
}

HttpServer::~HttpServer() {
    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpServer::get(const std::string& path, Handler handler) {
    getRoutes_[path] = handler;
}

void HttpServer::post(const std::string& path, Handler handler) {
    postRoutes_[path] = handler;
}

void HttpServer::serveStatic(const std::string& directory) {
    staticDir_ = directory;
}

void HttpServer::listen(int port) {
    if (!initialized_) {
        std::cerr << "[ERROR] Server not initialized" << std::endl;
        return;
    }

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        std::cerr << "[ERROR] socket() failed" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_short>(port));

    if (bind(listenSocket_, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] bind() failed – port " << port
                  << " may already be in use" << std::endl;
        closesocket(listenSocket_);
        return;
    }

    if (::listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[ERROR] listen() failed" << std::endl;
        closesocket(listenSocket_);
        return;
    }

    std::cout << "Listening on port " << port << " ..." << std::endl;

    while (true) {
        SocketType client = accept(listenSocket_, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        std::thread(&HttpServer::handleClient, this, client).detach();
    }
}

/* ------------------------------------------------------------------ */

void HttpServer::handleClient(SocketType clientSocket) {
    std::string raw;
    char buf[4096];

    /* Read until we have the full header block */
    while (true) {
        int n = recv(clientSocket, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { closesocket(clientSocket); return; }
        buf[n] = '\0';
        raw += buf;
        if (raw.find("\r\n\r\n") != std::string::npos) break;
    }

    HttpRequest req = parseRequest(raw);

    /* Read body if Content-Length is present */
    auto clIt = req.headers.find("content-length");
    if (clIt != req.headers.end()) {
        int contentLength = std::stoi(clIt->second);
        size_t headerEnd = raw.find("\r\n\r\n") + 4;
        std::string body = (headerEnd < raw.size()) ? raw.substr(headerEnd) : "";

        while (static_cast<int>(body.size()) < contentLength) {
            int n = recv(clientSocket, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = '\0';
            body += buf;
        }
        req.body = body;
    }

    /* Route -------------------------------------------------------- */
    HttpResponse res;

    if (req.method == "GET") {
        auto it = getRoutes_.find(req.path);
        if (it != getRoutes_.end()) {
            res = it->second(req);
        } else if (!staticDir_.empty()) {
            std::string filePath = req.path;
            if (filePath == "/") filePath = "/index.html";

            /* Security: reject paths with ".." */
            if (filePath.find("..") != std::string::npos) {
                res = HttpResponse::notFound();
            } else {
                std::string fullPath = staticDir_ + filePath;
                std::string content = readFile(fullPath);
                if (!content.empty()) {
                    res.status = 200;
                    res.contentType = getMimeType(fullPath);
                    res.body = content;
                } else {
                    res = HttpResponse::notFound();
                }
            }
        } else {
            res = HttpResponse::notFound();
        }
    } else if (req.method == "POST") {
        auto it = postRoutes_.find(req.path);
        if (it != postRoutes_.end()) {
            res = it->second(req);
        } else {
            res = HttpResponse::notFound();
        }
    } else if (req.method == "OPTIONS") {
        res.status = 204;
        res.body = "";
    } else {
        res = HttpResponse::notFound();
    }

    sendAll(clientSocket, buildResponse(res));
    closesocket(clientSocket);
}

/* ------------------------------------------------------------------ */

HttpRequest HttpServer::parseRequest(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;

    /* Request line */
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream rl(line);
        std::string version;
        rl >> req.method >> req.path >> version;
    }

    /* Strip query string */
    auto qpos = req.path.find('?');
    if (qpos != std::string::npos) req.path = req.path.substr(0, qpos);

    /* Headers */
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            /* trim leading whitespace */
            val.erase(0, val.find_first_not_of(" \t"));
            /* key to lower */
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = val;
        }
    }
    return req;
}

std::string HttpServer::buildResponse(const HttpResponse& res) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << res.status;
    switch (res.status) {
        case 200: ss << " OK"; break;
        case 204: ss << " No Content"; break;
        case 404: ss << " Not Found"; break;
        default:  ss << " Unknown"; break;
    }
    ss << "\r\n";
    ss << "Content-Type: "   << res.contentType     << "\r\n";
    ss << "Content-Length: " << res.body.size()      << "\r\n";
    ss << "Connection: close\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type\r\n";
    ss << "\r\n";
    ss << res.body;
    return ss.str();
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string HttpServer::getMimeType(const std::string& path) {
    if (endsWith(path, ".html")) return "text/html; charset=utf-8";
    if (endsWith(path, ".css"))  return "text/css; charset=utf-8";
    if (endsWith(path, ".js"))   return "application/javascript; charset=utf-8";
    if (endsWith(path, ".json")) return "application/json";
    if (endsWith(path, ".png"))  return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".svg"))  return "image/svg+xml";
    if (endsWith(path, ".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

std::string HttpServer::readFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "";
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    if (size <= 0) { std::fclose(f); return ""; }
    std::fseek(f, 0, SEEK_SET);
    std::string content(static_cast<size_t>(size), '\0');
    std::fread(&content[0], 1, static_cast<size_t>(size), f);
    std::fclose(f);
    return content;
}

void HttpServer::sendAll(SocketType sock, const std::string& data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        int sent = send(sock, data.c_str() + totalSent,
                        static_cast<int>(data.size() - totalSent), 0);
        if (sent <= 0) break;
        totalSent += static_cast<size_t>(sent);
    }
}
