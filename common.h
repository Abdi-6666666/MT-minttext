#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iomanip>
#include <algorithm>

// 链接网络库
#pragma comment(lib, "ws2_32.lib")

// C++17 命名空间别名
namespace fs = std::filesystem;

// 全局常量
constexpr int PORT = 8888;
constexpr int BUFFER_SIZE = 4096;
const std::string DATA_DIR = "C:\\text_data";
const std::string USER_FILE = DATA_DIR + "\\users.txt";
const std::string FRIEND_FILE = DATA_DIR + "\\friends.txt";

// DH算法常量（简化版）
constexpr int DH_P = 23;    // 素数
constexpr int DH_G = 5;     // 原根

// 数据结构
struct User {
    std::string id;
    std::string password;
    SOCKET sock = INVALID_SOCKET;
    bool isOnline = false;
};

struct Message {
    std::string type;       // login/add_friend/chat/switch
    std::string from;
    std::string to;
    std::string content;
};

// 工具函数
// 字符串分割
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> res;
    std::string temp;
    for (char c : s) {
        if (c == delim) {
            if (!temp.empty()) {
                res.push_back(temp);
                temp.clear();
            }
        } else {
            temp += c;
        }
    }
    if (!temp.empty()) res.push_back(temp);
    return res;
}

// 加密（简化AES，实际应使用openssl）
std::string encrypt(const std::string& data, int key) {
    std::string res;
    for (char c : data) {
        res += c ^ key; // 简单异或加密，实际项目请用标准加密库
    }
    return res;
}

// 解密
std::string decrypt(const std::string& data, int key) {
    return encrypt(data, key); // 异或加密可逆
}

// DH密钥交换计算
int dh_calculate(int private_key) {
    int res = 1;
    for (int i = 0; i < private_key; ++i) {
        res = (res * DH_G) % DH_P;
    }
    return res;
}

// 初始化数据目录
void init_data_dir() {
    if (!fs::exists(DATA_DIR)) {
        fs::create_directory(DATA_DIR);
    }
    // 创建空的用户/好友文件（如果不存在）
    if (!fs::exists(USER_FILE)) {
        std::ofstream ofs(USER_FILE);
        ofs.close();
    }
    if (!fs::exists(FRIEND_FILE)) {
        std::ofstream ofs(FRIEND_FILE);
        ofs.close();
    }
}

// 设置CMD颜色
void set_cmd_color() {
    system("color 0a"); // 黑底绿字，符合要求
}