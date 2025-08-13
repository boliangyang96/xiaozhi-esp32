#include "uart_controller.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "UartTest"

// 测试数据：模拟MCU上报的所有DP数据
// 包含开关、PM2.5、模式、风速、滤网寿命、童锁、杀菌灯、温度、湿度、定时、空气质量
const uint8_t test_multi_frame_data[] = {
    // 开关控制 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x01, 0x01, 0x00, 0x01, 0x01, 0x12,
    // PM2.5数值 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x08, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x19,
    // 模式设置状态 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x03, 0x04, 0x00, 0x01, 0x00, 0x16,
    // 风速状态 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x04, 0x04, 0x00, 0x01, 0x00, 0x17,
    // 滤网寿命 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x08, 0x05, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x1C,
    // 童锁状态 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x07, 0x01, 0x00, 0x01, 0x01, 0x18,
    // 杀菌灯状态 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x09, 0x01, 0x00, 0x01, 0x01, 0x1A,
    // 室内温度 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x08, 0x0C, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x23,
    // 室内湿度 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x08, 0x0D, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x24,
    // 定时状态 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x12, 0x04, 0x00, 0x01, 0x00, 0x25,
    // 空气质量 DP
    0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x15, 0x04, 0x00, 0x01, 0x00, 0x28
};

void uart_test_task(void* parameter) {
    ESP_LOGI(TAG, "Starting UART test task");
    
    // 创建UART控制器实例
    UartController* controller = new UartController();
    
    if (!controller->Initialize()) {
        ESP_LOGE(TAG, "Failed to initialize UART controller");
        delete controller;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "UART controller initialized successfully");
    
    // 测试多帧数据解析
    std::vector<uint8_t> test_data;
    test_data.assign(test_multi_frame_data, test_multi_frame_data + sizeof(test_multi_frame_data));
    
    ESP_LOGI(TAG, "Testing multi-frame data parsing with %u bytes", (unsigned int)test_data.size());
    
    // 使用公有的RefreshDeviceStatus方法来测试
    if (controller->RefreshDeviceStatus()) {
        ESP_LOGI(TAG, "Device status refresh test passed");
        
        // 获取解析后的设备状态
        const DeviceStatus& status = controller->GetDeviceStatus();
        ESP_LOGI(TAG, "Device status after parsing:");
        ESP_LOGI(TAG, "  Switch: %s", status.switch_state ? "ON" : "OFF");
        ESP_LOGI(TAG, "  PM2.5: %d", status.pm25);
        ESP_LOGI(TAG, "  Mode: %d", status.mode);
        ESP_LOGI(TAG, "  Fan Speed: %d", status.fan_speed);
        ESP_LOGI(TAG, "  Filter Life: %d%%", status.filter_life);
        ESP_LOGI(TAG, "  Child Lock: %s", status.child_lock ? "ON" : "OFF");
        ESP_LOGI(TAG, "  UV Light: %s", status.uv_light ? "ON" : "OFF");
        ESP_LOGI(TAG, "  Indoor Temp: %d°C", status.indoor_temp);
        ESP_LOGI(TAG, "  Indoor Humidity: %d%%", status.indoor_humidity);
        ESP_LOGI(TAG, "  Countdown: %d", status.countdown_set);
        ESP_LOGI(TAG, "  Air Quality: %d", status.air_quality);
        
        // 测试JSON输出
        std::string json_status = controller->GetStatusJson();
        ESP_LOGI(TAG, "JSON status: %s", json_status.c_str());
    } else {
        ESP_LOGE(TAG, "Device status refresh test failed");
    }
    
    // 测试控制命令
    ESP_LOGI(TAG, "Testing control commands...");
    
    if (controller->SetSwitch(true)) {
        ESP_LOGI(TAG, "Set switch ON command sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send set switch command");
    }
    
    if (controller->SetMode(1)) {
        ESP_LOGI(TAG, "Set mode command sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send set mode command");
    }
    
    if (controller->SetFanSpeed(2)) {
        ESP_LOGI(TAG, "Set fan speed command sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send set fan speed command");
    }
    
    // 清理
    delete controller;
    ESP_LOGI(TAG, "UART test completed");
    vTaskDelete(NULL);
}

void start_uart_test() {
    xTaskCreate(uart_test_task, "uart_test", 4096, NULL, 5, NULL);
} 