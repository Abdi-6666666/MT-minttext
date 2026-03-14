#include "common.h"

// 全局数据（加锁保证线程安全）
std::map<std::string, User> g_users;
std::map<std::string, std::vector<std::string>> g_friends;
std::mutex g_mutex;

// 加载用户数据
void load_users() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ifstream ifs(USER_FILE);
    std::string line;
    while (std::getline(ifs, line)) {
        auto parts = split(line, ',');
        if (parts.size() >= 2) {
            User u;
            u.id = parts[0];
            u.password = parts[1];
            u.isOnline = false;
            u.sock = INVALID_SOCKET;
            g_users[u.id] = u;
        }
    }
    ifs.close();
}

// 保存用户数据
void save_user(const User& user) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ofstream ofs(USER_FILE, std::ios::app);
    ofs << user.id << "," << user.password << std::endl;
    ofs.close();
}

// 加载好友数据
void load_friends() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ifstream ifs(FRIEND_FILE);
    std::string line;
    while (std::getline(ifs, line)) {
        auto parts = split(line, ',');
        if (parts.size() >= 2) {
            g_friends[parts[0]].push_back(parts[1]);
            g_friends[parts[1]].push_back(parts[0]); // 双向好友
        }
    }
    ifs.close();
}

// 保存好友关系
void save_friend(const std::string& user1, const std::string& user2) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ofstream ofs(FRIEND_FILE, std::ios::app);
    ofs << user1 << "," << user2 << std::endl;
    ofs.close();
}

// 解析消息
Message parse_message(const std::string& data) {
    auto parts = split(data, '|');
    Message msg;
    if (parts.size() >= 4) {
        msg.type = parts[0];
        msg.from = parts[1];
        msg.to = parts[2];
        msg.content = parts[3];
    }
    return msg;
}

// 构建消息
std::string build_message(const Message& msg) {
    return msg.type + "|" + msg.from + "|" + msg.to + "|" + msg.content;
}

// 处理客户端连接
void handle_client(SOCKET client_sock, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE] = {0};
    std::string current_user_id;
    int dh_key = 0; // DH协商后的密钥

    // 1. DH密钥交换
    // 服务器生成私钥
    int server_private = rand() % 10 + 1;
    int server_public = dh_calculate(server_private);
    // 发送服务器公钥
    send(client_sock, std::to_string(server_public).c_str(), std::to_string(server_public).size(), 0);
    // 接收客户端公钥
    int recv_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (recv_len <= 0) {
        closesocket(client_sock);
        return;
    }
    int client_public = std::stoi(std::string(buffer, recv_len));
    // 计算共享密钥
    dh_key = 1;
    for (int i = 0; i < server_private; ++i) {
        dh_key = (dh_key * client_public) % DH_P;
    }

    // 2. 处理客户端消息
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        recv_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (recv_len <= 0) {
            // 客户端断开
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!current_user_id.empty() && g_users.count(current_user_id)) {
                g_users[current_user_id].isOnline = false;
                g_users[current_user_id].sock = INVALID_SOCKET;
                std::cout << "用户 " << current_user_id << " 离线" << std::endl;
            }
            closesocket(client_sock);
            break;
        }

        // 解密数据
        std::string recv_data = decrypt(std::string(buffer, recv_len), dh_key);
        Message msg = parse_message(recv_data);

        // 处理不同类型消息
        if (msg.type == "login") {
            std::lock_guard<std::mutex> lock(g_mutex);
            Message resp;
            resp.type = "login_resp";
            resp.to = msg.from;

            // 检查用户是否存在
            if (g_users.count(msg.from)) {
                // 检查密码
                if (g_users[msg.from].password == msg.content) {
                    // 检查是否已登录
                    if (g_users[msg.from].isOnline) {
                        resp.content = "fail|该账号已在线";
                    } else {
                        g_users[msg.from].isOnline = true;
                        g_users[msg.from].sock = client_sock;
                        current_user_id = msg.from;
                        resp.content = "success|登录成功";
                        std::cout << "用户 " << msg.from << " 登录成功" << std::endl;
                    }
                } else {
                    resp.content = "fail|密码错误";
                }
            } else {
                // 新用户自动注册
                User new_user;
                new_user.id = msg.from;
                new_user.password = msg.content;
                new_user.isOnline = true;
                new_user.sock = client_sock;
                g_users[new_user.id] = new_user;
                save_user(new_user);
                current_user_id = new_user.id;
                resp.content = "success|新用户注册并登录成功";
                std::cout << "新用户 " << new_user.id << " 注册并登录" << std::endl;
            }

            // 发送响应
            std::string send_data = encrypt(build_message(resp), dh_key);
            send(client_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (msg.type == "add_friend") {
            Message resp;
            resp.type = "add_friend_resp";
            resp.to = msg.from;

            std::lock_guard<std::mutex> lock(g_mutex);
            // 检查好友是否存在
            if (!g_users.count(msg.content)) {
                resp.content = "fail|好友账号不存在";
            }
            // 检查是否是自己
            else if (msg.content == msg.from) {
                resp.content = "fail|不能添加自己为好友";
            }
            // 检查是否已添加
            else if (std::find(g_friends[msg.from].begin(), g_friends[msg.from].end(), msg.content) != g_friends[msg.from].end()) {
                resp.content = "fail|已添加该好友";
            }
            else {
                // 添加好友关系
                g_friends[msg.from].push_back(msg.content);
                g_friends[msg.content].push_back(msg.from);
                save_friend(msg.from, msg.content);
                resp.content = "success|添加好友成功";
                std::cout << "用户 " << msg.from << " 添加 " << msg.content << " 为好友" << std::endl;
            }

            // 发送响应
            std::string send_data = encrypt(build_message(resp), dh_key);
            send(client_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (msg.type == "switch") {
            std::lock_guard<std::mutex> lock(g_mutex);
            Message resp;
            resp.type = "switch_resp";
            resp.to = msg.from;

            // 先登出当前用户
            if (!current_user_id.empty() && g_users.count(current_user_id)) {
                g_users[current_user_id].isOnline = false;
                g_users[current_user_id].sock = INVALID_SOCKET;
                std::cout << "用户 " << current_user_id << " 切换账号" << std::endl;
            }

            // 登录新账号
            std::string new_id = msg.to;
            std::string new_pwd = msg.content;
            if (g_users.count(new_id)) {
                if (g_users[new_id].password == new_pwd) {
                    if (g_users[new_id].isOnline) {
                        resp.content = "fail|该账号已在线";
                    } else {
                        g_users[new_id].isOnline = true;
                        g_users[new_id].sock = client_sock;
                        current_user_id = new_id;
                        resp.content = "success|切换账号成功";
                        std::cout << "用户 " << new_id << " 切换登录成功" << std::endl;
                    }
                } else {
                    resp.content = "fail|密码错误";
                }
            } else {
                resp.content = "fail|账号不存在";
            }

            // 发送响应
            std::string send_data = encrypt(build_message(resp), dh_key);
            send(client_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (msg.type == "chat") {
            Message resp;
            resp.type = "chat_resp";
            resp.from = msg.from;
            resp.to = msg.to;

            std::lock_guard<std::mutex> lock(g_mutex);
            // 检查好友是否在线
            if (g_users.count(msg.to) && g_users[msg.to].isOnline) {
                // 转发消息给好友
                resp.content = "msg|" + msg.from + "：" + msg.content;
                std::string send_data = encrypt(build_message(resp), dh_key);
                send(g_users[msg.to].sock, send_data.c_str(), send_data.size(), 0);

                // 回复发送方成功
                resp.to = msg.from;
                resp.content = "success|消息发送成功";
                send_data = encrypt(build_message(resp), dh_key);
                send(client_sock, send_data.c_str(), send_data.size(), 0);
            } else {
                resp.to = msg.from;
                resp.content = "fail|好友不在线或不存在";
                std::string send_data = encrypt(build_message(resp), dh_key);
                send(client_sock, send_data.c_str(), send_data.size(), 0);
            }
        }
    }
}

int main() {
    // 设置CMD颜色
    set_cmd_color();
    std::cout << "=== 聊天服务器（C++17）===" << std::endl;

    // 初始化数据目录
    init_data_dir();

    // 加载用户和好友数据
    load_users();
    load_friends();
    std::cout << "加载用户数：" << g_users.size() << std::endl;
    std::cout << "加载好友关系数：" << g_friends.size() << std::endl;

    // 初始化WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSA初始化失败：" << WSAGetLastError() << std::endl;
        return 1;
    }

    // 创建监听套接字
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "创建套接字失败：" << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 绑定地址和端口
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡
    server_addr.sin_port = htons(PORT);

    if (bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "绑定失败：" << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    // 开始监听
    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "监听失败：" << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    std::cout << "服务器启动成功，端口：" << PORT << "，等待客户端连接..." << std::endl;

    // 接受客户端连接
    while (true) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            std::cerr << "接受连接失败：" << WSAGetLastError() << std::endl;
            continue;
        }

        // 打印客户端信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "客户端连接：" << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

        // 启动线程处理客户端
        std::thread client_thread(handle_client, client_sock, client_addr);
        client_thread.detach(); // 分离线程，自动回收资源
    }

    // 清理资源（实际不会执行到这里）
    closesocket(listen_sock);
    WSACleanup();
    return 0;
}