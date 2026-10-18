//
// Created by luna on 3/1/26.
//
#include "src/tcp/TcpServer.h"
#include "src/tcp/HttpParser.h"
#include "src/net/EventLoop.h"
#include "src/net/InetAddress.h"
#include "src/net/TimerQueue.h"
#include "src/base/Logger.h"
#include "src/base/AsyncLogging.h"

#include <set>
#include <map>
#include <mutex>
#include <csignal>
#include <vector>
#include <unistd.h>  // 🔥 getpid
#include <functional>

// ========================================================

// ========================================================
// Route log records to the background writer.
void asyncOutput(const char* msg, int len) {

    AsyncLogging::getInstance().append(msg, len);
}

EventLoop* g_loop = nullptr;

void signalHandler(int signum) {
    printf("\n🚨 [Signal] Received signal %d. Shutting down gracefully...\n", signum);
    if (g_loop) {
        g_loop->quit();
    }
}

// ========================================================

// ========================================================
// Serve the demonstration UI from an embedded HTML document.
const char* g_chat_html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>LunaNet WebChat</title>
</head>
<body style="font-family: Arial, sans-serif; background-color: #f4f4f9; padding: 20px;">
    <h2 style="color: #333;">🚀 LunaNet 生产级 WebChat 演示</h2>
    <div id="chat-box" style="height:400px; border:1px solid #ccc; overflow-y:scroll; background:white; padding:10px; border-radius: 8px;"></div>
    <div style="margin-top: 15px;">
        <input type="text" id="msg-input" placeholder="输入聊天内容..." style="width:70%; padding: 10px; border-radius: 4px; border: 1px solid #ccc;">
        <button onclick='sendMsg()' style="padding: 10px 20px; background-color: #007BFF; color: white; border: none; border-radius: 4px; cursor: pointer;">发送</button>
    </div>
    <script>
        function sendMsg() {
            const input = document.getElementById('msg-input');
            const val = input.value;
            if(!val) return;
            fetch('/chat', { method: 'POST', body: val });

            const box = document.getElementById('chat-box');
            box.innerHTML += '<div style="margin-bottom:8px;"><b>我:</b> ' + val + '</div>';
            box.scrollTop = box.scrollHeight;
            input.value = '';
        }
    </script>
</body>
</html>
)";

// ========================================================

// ========================================================
// Implements the chat demo and tracks connection activity.
class WebChatServer {
public:
    WebChatServer(EventLoop* loop, const InetAddress& listenAddr)
        : loop_(loop), server_(loop, listenAddr, "WebChatServer")
    {

        server_.setConnectionCallback(
            std::bind(&WebChatServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(
            std::bind(&WebChatServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        server_.setThreadNum(4);
    }

    void start() {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        TimerId idleTimer = 0;
        TimerId forceCloseTimer = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (conn->connected()) {
                LOG_INFO << "✅ [New User] " << conn->peerAddress().toIpPort() << " online.";
                connections_.insert(conn);
                idleTimers_[conn] = ConnectionTimers{};
            } else {
                LOG_INFO << "❌ [User Left] " << conn->peerAddress().toIpPort() << " offline.";
                connections_.erase(conn);
                const auto timers = idleTimers_.find(conn);
                if (timers != idleTimers_.end()) {
                    idleTimer = timers->second.idleTimer;
                    forceCloseTimer = timers->second.forceCloseTimer;
                    idleTimers_.erase(timers);
                }
            }
        }

        if (idleTimer != 0) conn->getLoop()->cancel(idleTimer);
        if (forceCloseTimer != 0) conn->getLoop()->cancel(forceCloseTimer);
        if (conn->connected()) refreshIdleTimer(conn);

        {
            std::lock_guard<std::mutex> lock(parserMutex_);
            if (conn->connected()) {
                parsers_.try_emplace(conn);
            } else {
                parsers_.erase(conn);
            }
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        if (conn->connected()) refreshIdleTimer(conn);

        // Keep parser state per connection so split requests survive callbacks.
        std::vector<std::pair<std::string, std::string>> requests;
        bool malformedRequest = false;
        {
            std::lock_guard<std::mutex> lock(parserMutex_);
            HttpParser& parser = parsers_[conn];

            while (true) {
                if (!parser.parseRequest(buf)) {
                    malformedRequest = true;
                    break;
                }
                if (!parser.gotAll()) break;

                requests.emplace_back(parser.getPath(), parser.getBody());
                parser.reset();
                if (buf->readableBytes() == 0) break;
            }
        }

        if (malformedRequest) {
            conn->send("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            conn->shutdown();
            return;
        }

        for (const auto& request : requests) {
            const std::string& path = request.first;
            if (path == "/") {
                LOG_INFO << "📄 " << conn->peerAddress().toIpPort() << " requested Chat UI.";
                sendHttpResponse(conn, "text/html; charset=utf-8", g_chat_html);
            }
            else if (path == "/chat") {
                const std::string& msg = request.second;
                LOG_INFO << "💬 [" << conn->peerAddress().toIpPort() << "] says: " << msg;

                std::string broadcastMsg = "[" + conn->peerAddress().toIpPort() + "]: " + msg;
                broadcast(broadcastMsg);

                sendHttpResponse(conn, "text/plain", "OK");
            }
        }
    }

    void broadcast(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto const& conn : connections_) {

            conn->send("Broadcast: " + msg + "\r\n");
        }
    }

    struct ConnectionTimers {
        TimerId idleTimer = 0;
        TimerId forceCloseTimer = 0;
    };

    void refreshIdleTimer(const TcpConnectionPtr& conn) {
        if (!conn->connected()) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto state = idleTimers_.find(conn);
        if (state == idleTimers_.end()) return;

        if (state->second.idleTimer != 0) {
            conn->getLoop()->cancel(state->second.idleTimer);
        }
        state->second.idleTimer = conn->getLoop()->runAfter(
            15.0, [this, conn]() { handleIdleTimeout(conn); });
    }

    void handleIdleTimeout(const TcpConnectionPtr& conn) {
        if (!conn->connected()) return;

        LOG_INFO << "Connection idle for 15 seconds; starting graceful shutdown: "
                 << conn->peerAddress().toIpPort();
        conn->shutdown();

        std::lock_guard<std::mutex> lock(mutex_);
        auto state = idleTimers_.find(conn);
        if (state == idleTimers_.end()) return;
        state->second.idleTimer = 0;
        state->second.forceCloseTimer = conn->getLoop()->runAfter(
            15.0, [this, conn]() { handleForceCloseTimeout(conn); });
    }

    void handleForceCloseTimeout(const TcpConnectionPtr& conn) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto state = idleTimers_.find(conn);
            if (state == idleTimers_.end()) return;
            state->second.forceCloseTimer = 0;
        }

        if (!conn->disconnected()) {
            LOG_INFO << "Connection did not close within 15 seconds after shutdown; "
                        "forcing close: "
                     << conn->peerAddress().toIpPort();
            conn->forceClose();
        }
    }

    void sendHttpResponse(const TcpConnectionPtr& conn, const std::string& contentType, const std::string& content) {
        std::string response;
        response += "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: " + contentType + "\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
        response += "Connection: Keep-Alive\r\n";
        response += "\r\n";
        response += content;
        conn->send(response);
    }

    EventLoop* loop_;
    std::mutex mutex_;
    std::mutex parserMutex_;
    std::set<TcpConnectionPtr> connections_;
    std::map<TcpConnectionPtr, ConnectionTimers> idleTimers_;
    std::map<TcpConnectionPtr, HttpParser> parsers_;
    // Destroy worker loops before the state used by their callbacks.
    TcpServer server_;
};

// ========================================================

// ========================================================
// Configure process hooks, then run the server on the base EventLoop.
int main() {

    ::signal(SIGPIPE, SIG_IGN);

    ::signal(SIGINT, signalHandler);
    ::signal(SIGTERM, signalHandler);

    AsyncLogging::getInstance().start();
    Logger::setOutput(asyncOutput);

    LOG_INFO << "========================================";
    LOG_INFO << "  LunaNet WebChat Server is Booting...  ";
    LOG_INFO << "  Pid = " << getpid();
    LOG_INFO << "========================================";

    EventLoop loop;
    g_loop = &loop;

    InetAddress listenAddr("127.0.0.1", 8080);
    WebChatServer server(&loop, listenAddr);

    server.start();

    loop.loop();

    AsyncLogging::getInstance().stop();
    printf("\n✅ LunaNet WebChatServer exited gracefully. See you next time!\n");

    return 0;
}
