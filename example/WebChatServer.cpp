//
// Created by luna on 3/1/26.
//
#include "../tcp/TcpServer.h"
#include "../tcp/HttpParser.h"
#include "../net/EventLoop.h"
#include "../net/InetAddress.h"
#include "../net/TimerQueue.h"
#include "../base/Logger.h"
#include "../base/AsyncLogging.h" // 🔥 引入异步日志引擎

#include <set>
#include <map>
#include <mutex>
#include <csignal>   // 🔥 优雅退出
#include <unistd.h>  // 🔥 getpid
#include <functional>

// ========================================================
// 🛡️ 全局基建：异步日志输出钩子 & 优雅退出指针
// ========================================================
void asyncOutput(const char* msg, int len) {
    // 呼叫后台日志线程，避免磁盘 I/O 阻塞主 Reactor
    AsyncLogging::getInstance().append(msg, len);
}

EventLoop* g_loop = nullptr;

void signalHandler(int signum) {
    printf("\n🚨 [Signal] Received signal %d. Shutting down gracefully...\n", signum);
    if (g_loop) {
        g_loop->quit(); // 安全唤醒 EventLoop 并退出死循环
    }
}

// ========================================================
// 🌐 前端 UI 页面 (直接嵌入 C++ 作为静态资源)
// ========================================================
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

            // 本地回显
            const box = document.getElementById('chat-box');
            box.innerHTML += '<div style="margin-bottom:8px;"><b>我:</b> ' + val + '</div>';
            box.scrollTop = box.scrollHeight; // 自动滚动到底部
            input.value = '';
        }
    </script>
</body>
</html>
)";

// ========================================================
// 🚀 核心业务服务器
// ========================================================
class WebChatServer {
public:
    WebChatServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "WebChatServer"),
          loop_(loop)
    {
        // 1. 注册网络事件回调
        server_.setConnectionCallback(
            std::bind(&WebChatServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(
            std::bind(&WebChatServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 2. 开启多线程池 (处理并发请求)
        server_.setThreadNum(4);

        // 3. 启动心跳定时器：每 5 秒扫描一次，清理死连接
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
        // 更新该连接的最后活跃时间（心跳保活）
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastActiveTime_[conn] = Timestamp::now();
        }

        // 解析 HTTP 协议
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

                // 广播给所有人（此处简单模拟，正式环境可以走 WebSocket）
                std::string broadcastMsg = "[" + conn->peerAddress().toIpPort() + "]: " + msg;
                broadcast(broadcastMsg);

                sendHttpResponse(conn, "text/plain", "OK");
            }
        }
    }

    void broadcast(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto const& conn : connections_) {
            // 这里我们粗暴地往所有建立的 TCP 链接塞数据，虽然 HTTP 短链接收不到，但长链接/WS 可以。
            // 作为底层测试，这证明了你的连接集 (connections_) 是生效的。
            conn->send("Broadcast: " + msg + "\r\n");
        }
    }


    // 完美的“两段式”心跳清理逻辑
    void checkIdleConnections() {
        Timestamp now = Timestamp::now();
        std::vector<TcpConnectionPtr> toShutdown;
        std::vector<TcpConnectionPtr> toForceClose;

        {
            // 加锁遍历，找出超时的连接
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto const& [conn, lastTime] : lastActiveTime_) {
                int64_t idleMicroSeconds = now.microSecondsSinceEpoch() - lastTime.microSecondsSinceEpoch();

                // 💀 阶段 2：超过 30 秒，终极死刑（拔网线）
                if (idleMicroSeconds > 30.0 * Timestamp::kMicroSecondsPerSecond) {
                    toForceClose.push_back(conn);
                }
                // ⚠️ 阶段 1：超过 15 秒，触发半关闭（优雅挥手）
                else if (idleMicroSeconds > 15.0 * Timestamp::kMicroSecondsPerSecond) {
                    toShutdown.push_back(conn);
                }
            }
        } // 🔒 锁在这里自动释放！

        // 移出锁的作用域后再执行动作，完美避开死锁风险！

        for (auto& conn : toShutdown) {
            if (conn->connected()) { // 确保它还没有自己断开
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
        response += "Connection: Keep-Alive\r\n"; // 开启长连接
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
// 🏁 引擎启动总闸 (Main)
// ========================================================
int main() {
    // 1. 终极防御：忽略 SIGPIPE（防止向已关闭的 socket 写数据导致服务器崩溃）
    ::signal(SIGPIPE, SIG_IGN);

    // 2. 优雅退出：注册信号捕获
    ::signal(SIGINT, signalHandler);  // 拦截 Ctrl+C
    ::signal(SIGTERM, signalHandler); // 拦截 kill

    // 3. 点火启动异步日志引擎，并接管全局输出
    AsyncLogging::getInstance().start();
    Logger::setOutput(asyncOutput);

    LOG_INFO << "========================================";
    LOG_INFO << "  LunaNet WebChat Server is Booting...  ";
    LOG_INFO << "  Pid = " << getpid();
    LOG_INFO << "========================================";

    EventLoop loop;
    g_loop = &loop; // 让信号处理器能找到主 Reactor

    // 4. 组装跑车：绑定 0.0.0.0，这样你不仅能在电脑看，手机连上同一 WiFi 也能访问！
    InetAddress listenAddr("127.0.0.1", 8080);
    WebChatServer server(&loop, listenAddr);

    server.start();

    // 5. 引擎持续轰鸣... (程序挂起于此)
    loop.loop();

    // 6. 收到退出信号，打扫战场
    AsyncLogging::getInstance().stop();
    printf("\n✅ LunaNet WebChatServer exited gracefully. See you next time!\n");

    return 0;
}