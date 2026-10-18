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
        : server_(loop, listenAddr, "WebChatServer"),
          loop_(loop)
    {

        server_.setConnectionCallback(
            std::bind(&WebChatServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(
            std::bind(&WebChatServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        server_.setThreadNum(4);

        loop_->runEvery(5.0, std::bind(&WebChatServer::checkIdleConnections, this));
    }

    void start() {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (conn->connected()) {
            LOG_INFO << "✅ [New User] " << conn->peerAddress().toIpPort() << " online.";
            connections_.insert(conn);
            lastActiveTime_[conn] = Timestamp::now();
        } else {
            LOG_INFO << "❌ [User Left] " << conn->peerAddress().toIpPort() << " offline.";
            connections_.erase(conn);
            lastActiveTime_.erase(conn);
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {

        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastActiveTime_[conn] = Timestamp::now();
        }

        HttpParser parser;
        if (parser.parseRequest(buf)) {
            const std::string& path = parser.getPath();

            if (path == "/") {
                LOG_INFO << "📄 " << conn->peerAddress().toIpPort() << " requested Chat UI.";
                sendHttpResponse(conn, "text/html; charset=utf-8", g_chat_html);
            }
            else if (path == "/chat") {
                std::string msg = parser.getBody();
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

    void checkIdleConnections() {
        Timestamp now = Timestamp::now();
        std::vector<TcpConnectionPtr> toShutdown;
        std::vector<TcpConnectionPtr> toForceClose;

        {

            std::lock_guard<std::mutex> lock(mutex_);
            for (auto const& [conn, lastTime] : lastActiveTime_) {
                int64_t idleMicroSeconds = now.microSecondsSinceEpoch() - lastTime.microSecondsSinceEpoch();

                if (idleMicroSeconds > 30.0 * Timestamp::kMicroSecondsPerSecond) {
                    toForceClose.push_back(conn);
                }

                else if (idleMicroSeconds > 15.0 * Timestamp::kMicroSecondsPerSecond) {
                    toShutdown.push_back(conn);
                }
            }
        }

        for (auto& conn : toShutdown) {
            if (conn->connected()) {
                LOG_INFO << "⚠️ 连接空闲超过 15 秒，触发半关闭(优雅挥手): " << conn->peerAddress().toIpPort();
                conn->shutdown();
            }
        }

        for (auto& conn : toForceClose) {
            LOG_ERROR << "💀 超过 30 秒未响应，拔网线强制踢出: " << conn->peerAddress().toIpPort();
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

    TcpServer server_;
    EventLoop* loop_;
    std::mutex mutex_;
    std::set<TcpConnectionPtr> connections_;
    std::map<TcpConnectionPtr, Timestamp> lastActiveTime_;
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
