#pragma once
#include <string>
#include <map>
#include <functional>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET SocketType;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int SocketType;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define closesocket    close
#endif

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "text/plain";
    std::string body;

    static HttpResponse json(const std::string& b)  { return {200, "application/json", b}; }
    static HttpResponse html(const std::string& b)   { return {200, "text/html; charset=utf-8", b}; }
    static HttpResponse notFound()                    { return {404, "text/plain", "Not Found"}; }
};

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    HttpServer();
    ~HttpServer();

    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    void serveStatic(const std::string& directory);
    void listen(int port);

private:
    void handleClient(SocketType clientSocket);
    HttpRequest parseRequest(const std::string& raw);
    std::string buildResponse(const HttpResponse& res);
    std::string getMimeType(const std::string& path);
    std::string readFile(const std::string& path);
    void sendAll(SocketType sock, const std::string& data);

    SocketType listenSocket_;
    std::map<std::string, Handler> getRoutes_;
    std::map<std::string, Handler> postRoutes_;
    std::string staticDir_;
    bool initialized_;
};
