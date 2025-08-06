#include <esp_log.h>
#include <string>
#include <ssid_manager.h>            // 用于管理SSID列表

#define TAG "wifi_pre_connect"

// 测试用例枚举
enum TestCase {
    TEST_NORMAL_WIFI = 0,           // 正常WiFi
    TEST_EMPTY_CREDENTIALS = 1,     // 空凭据
    TEST_INVALID_WIFI = 2,          // 无效WiFi
    TEST_SPECIAL_CHARS = 3,         // 特殊字符
    TEST_CHINESE_SSID = 4,          // 中文SSID
    TEST_LONG_CREDENTIALS = 5,      // 长凭据
    DEFAULT_WIFI = 6,               // 默认WiFi
};

// 当前测试用例（可以通过修改这个值来测试不同场景）
static TestCase current_test_case = DEFAULT_WIFI;               // 默认WiFi
//TEST_NORMAL_WIFI;

// 伪函数：模拟从外部API获取WiFi账号密码
// 实际使用时请替换为真实API调用
void GetWifiCredentialsFromApi(std::string& ssid, std::string& password) {
    // TODO: 替换为实际的API获取逻辑
    // 例如：通过HTTP请求、蓝牙、串口等方式获取

    switch (current_test_case) {
        case TEST_NORMAL_WIFI:
            ssid = "Chrissy";
            password = "11111111";
            ESP_LOGI(TAG, "[测试] 正常WiFi连接");
            break;

        case TEST_EMPTY_CREDENTIALS:
            ssid = "";
            password = "";
            ESP_LOGI(TAG, "[测试] 空凭据 - 应该进入配网模式");
            break;

        case TEST_INVALID_WIFI:
            ssid = "non-existent-wifi";
            password = "wrong-password";
            ESP_LOGI(TAG, "[测试] 无效WiFi - 应该连接失败后进入配网模式");
            break;

        case TEST_SPECIAL_CHARS:
            ssid = "Test-WiFi_123";
            password = "!@#$%^&*()_+{}|:<>?[]\\;'\",./";
            ESP_LOGI(TAG, "[测试] 特殊字符WiFi");
            break;

        case TEST_CHINESE_SSID:
            ssid = "测试WiFi网络";
            password = "中文密码123";
            ESP_LOGI(TAG, "[测试] 中文SSID和密码");
            break;

        case TEST_LONG_CREDENTIALS:
            ssid = "very-long-wifi-ssid-name-that-exceeds-normal-length-for-testing-purposes";
            password = "very-long-password-that-exceeds-normal-length-for-testing-purposes-1234567890";
            ESP_LOGI(TAG, "[测试] 超长SSID和密码");
            break;

        default:
            ssid = "antop-r02";
            password = "antop@88888";
            ESP_LOGI(TAG, "[默认] 使用默认WiFi");
            break;
    }

    ESP_LOGI(TAG, "[模拟] 已从API获取WiFi账号密码: SSID=%s, PASSWORD=%s", ssid.c_str(), password.c_str());
}

extern "C" void WifiPreConnect() {
    ESP_LOGI(TAG, "=== 进入WifiPreConnect，准备自动联网 ===");

    // 从外部API获取WiFi账号密码
    std::string ssid, password;
    GetWifiCredentialsFromApi(ssid, password);

    if (ssid.empty() || password.empty()) {
        ESP_LOGE(TAG, "未获取到有效的WiFi账号或密码，跳过自动联网");
        return;
    }

    ESP_LOGI(TAG, "获取到WiFi凭据: SSID=%s", ssid.c_str());

    // ===== 修改：只使用SsidManager保存凭据，不进行底层WiFi操作 =====
    ESP_LOGI(TAG, "保存WiFi凭据到SsidManager...");
    SsidManager::GetInstance().AddSsid(ssid, password);
    ESP_LOGI(TAG, "WiFi凭据已保存到SsidManager: SSID=%s", ssid.c_str());

    // 清除force_ap标志，确保不会强制进入配网模式
    // 注意：这里不进行WiFi连接，让WifiBoard和WifiStation处理连接
    ESP_LOGI(TAG, "WiFi凭据已保存，继续正常启动流程");
}