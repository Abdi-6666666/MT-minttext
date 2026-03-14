#include "common.h"

// 全局变量
std::string g_current_user;
SOCKET g_sock = INVALID_SOCKET;
int g_dh_key = 0;

// 接收服务器消息的线程函数
void recv_thread() {
    char buffer[BUFFER_SIZE] = {0};
    while (true) {
        int recv_len = recv(g_sock, buffer, BUFFER_SIZE, 0);
        if (recv_len <= 0) {
            std::cout << "\n与服务器断开连接！" << std::endl;
            closesocket(g_sock);
            g_sock = INVALID_SOCKET;
            break;
        }

        // 解密数据
        std::string recv_data = decrypt(std::string(buffer, recv_len), g_dh_key);
        Message msg = parse_message(recv_data);

        // 处理响应
        std::cout << "\n【系统消息】：";
        if (msg.type == "login_resp") {
            auto parts = split(msg.content, '|');
            if (parts[0] == "success") {
                g_current_user = msg.to;
                std::cout << parts[1] << std::endl;
            } else {
                std::cout << parts[1] << std::endl;
            }
        }
        else if (msg.type == "add_friend_resp") {
            auto parts = split(msg.content, '|');
            std::cout << parts[1] << std::endl;
        }
        else if (msg.type == "switch_resp") {
            auto parts = split(msg.content, '|');
            if (parts[0] == "success") {
                g_current_user = msg.to;
                std::cout << parts[1] << std::endl;
            } else {
                std::cout << parts[1] << std::endl;
            }
        }
        else if (msg.type == "chat_resp") {
            auto parts = split(msg.content, '|');
            if (parts[0] == "msg") {
                // 好友发来的消息
                std::cout << parts[1] << std::endl;
            } else {
                // 发送结果
                std::cout << parts[1] << std::endl;
            }
        }

        // 重新显示输入提示
        std::cout << "[" << g_current_user << "] > ";
        std::cout.flush();
    }
}

// 显示菜单
void show_menu() {
    std::cout << "\n===== 聊天客户端菜单 =====" << std::endl;
    std::cout << "1. 登录账号" << std::endl;
    std::cout << "2. 切换账号" << std::endl;
    std::cout << "3. 添加好友" << std::endl;
    std::cout << "4. 发送消息" << std::endl;
    std::cout << "5. 退出程序" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "[" << g_current_user << "] > ";
}

int main() {
    // 设置CMD颜色
    set_cmd_color();
    std::cout << "=== 聊天客户端（C++17）===" << std::endl;

    // 初始化WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSA初始化失败：" << WSAGetLastError() << std::endl;
        return 1;
    }

    // 创建套接字
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) {
        std::cerr << "创建套接字失败：" << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 连接服务器
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    // 连接本地服务器（可修改为远程IP）
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "无效的服务器地址" << std::endl;
        closesocket(g_sock);
        WSACleanup();
        return 1;
    }

    std::cout << "连接服务器 127.0.0.1:" << PORT << "..." << std::endl;
    if (connect(g_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "连接服务器失败：" << WSAGetLastError() << std::endl;
        closesocket(g_sock);
        WSACleanup();
        return 1;
    }
    std::cout << "连接服务器成功！" << std::endl;

    // DH密钥交换
    // 客户端生成私钥
    int client_private = rand() % 10 + 1;
    int client_public = dh_calculate(client_private);
    // 接收服务器公钥
    char buffer[BUFFER_SIZE] = {0};
    int recv_len = recv(g_sock, buffer, BUFFER_SIZE, 0);
    if (recv_len <= 0) {
        std::cerr << "DH密钥交换失败" << std::endl;
        closesocket(g_sock);
        WSACleanup();
        return 1;
    }
    int server_public = std::stoi(std::string(buffer, recv_len));
    // 发送客户端公钥
    send(g_sock, std::to_string(client_public).c_str(), std::to_string(client_public).size(), 0);
    // 计算共享密钥
    g_dh_key = 1;
    for (int i = 0; i < client_private; ++i) {
        g_dh_key = (g_dh_key * server_public) % DH_P;
    }
    std::cout << "DH密钥协商成功，密钥：" << g_dh_key << std::endl;

    // 启动接收线程
    std::thread recv_t(recv_thread);
    recv_t.detach();

    // 主交互循环
    while (true) {
        show_menu();
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1") {
            // 登录
            std::string user_id, password;
            std::cout << "请输入账号：";
            std::getline(std::cin, user_id);
            std::cout << "请输入密码：";
            std::getline(std::cin, password);

            Message msg;
            msg.type = "login";
            msg.from = user_id;
            msg.to = "server";
            msg.content = password;

            std::string send_data = encrypt(build_message(msg), g_dh_key);
            send(g_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (choice == "2") {
            // 切换账号
            if (g_current_user.empty()) {
                std::cout << "【系统消息】：请先登录账号！" << std::endl;
                continue;
            }
            std::string new_id, new_pwd;
            std::cout << "请输入新账号：";
            std::getline(std::cin, new_id);
            std::cout << "请输入新密码：";
            std::getline(std::cin, new_pwd);

            Message msg;
            msg.type = "switch";
            msg.from = g_current_user;
            msg.to = new_id;
            msg.content = new_pwd;

            std::string send_data = encrypt(build_message(msg), g_dh_key);
            send(g_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (choice == "3") {
            // 添加好友
            if (g_current_user.empty()) {
                std::cout << "【系统消息】：请先登录账号！" << std::endl;
                continue;
            }
            std::string friend_id;
            std::cout << "请输入好友账号：";
            std::getline(std::cin, friend_id);

            Message msg;
            msg.type = "add_friend";
            msg.from = g_current_user;
            msg.to = "server";
            msg.content = friend_id;

            std::string send_data = encrypt(build_message(msg), g_dh_key);
            send(g_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (choice == "4") {
            // 发送消息
            if (g_current_user.empty()) {
                std::cout << "【系统消息】：请先登录账号！" << std::endl;
                continue;
            }
            std::string friend_id, content;
            std::cout << "请输入好友账号：";
            std::getline(std::cin, friend_id);
            std::cout << "请输入消息内容：";
            std::getline(std::cin, content);

            Message msg;
            msg.type = "chat";
            msg.from = g_current_user;
            msg.to = friend_id;
            msg.content = content;

            std::string send_data = encrypt(build_message(msg), g_dh_key);
            send(g_sock, send_data.c_str(), send_data.size(), 0);
        }
        else if (choice == "5") {
            // 退出
            std::cout << "退出程序..." << std::endl;
            closesocket(g_sock);
            WSACleanup();
            return 0;
        }
        else {
            std::cout << "【系统消息】：无效的选择，请重新输入！" << std::endl;
        }
    }

    return 0;
}