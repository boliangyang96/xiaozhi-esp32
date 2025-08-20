#include "uart_controller.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string.h>
#include <vector>

#define TAG "UartController"

UartController::UartController(gpio_num_t tx_pin, gpio_num_t rx_pin, uart_port_t uart_port, int baud_rate)
    : uart_port_(uart_port), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate), status_initialized_(false) {
    
    // 初始化设备状态
    memset(&device_status_, 0, sizeof(device_status_));
    
    // 初始化UART
    if (Initialize()) {
        ESP_LOGI(TAG, "UART controller initialized successfully");
        
        // 添加MCP工具
        auto& mcp_server = McpServer::GetInstance();
        
        // 获取设备状态
        mcp_server.AddTool("self.air_purifier_or_light.get_status", 
            "Provides the real-time information of the air purifier and lighting system, including all status of the air purifier (purifier switch, indoor PM2.5 level, purifier mode, fan speed, filter life, anion, child lock, UV sterilization light, indoor temperature, indoor humidity, countdown timer, indoor air quality), and all lighting status (brightness, LED switch, LED breath switch, LED scene, LED color HSV values, laser light mode, projection light mode). The LED color is represented in HSV format (led_hue, led_saturation, led_value).\n"
            "Return value number meanings:\n"
            "- purifier_mode: sleep (0), auto (1), fast (2), manual (3)\n"
            "- purifier_fan_speed: low (0), mid (1), high (2)\n"
            "- countdown_set: 1 hour (0), 2 hours (1), 4 hours (2), 6 hours (3), cancel timer (4)\n"
            "- indoor_air_quality: great (0), medium (1), severe (2)\n"
            "- light_led_scene: moon_shadow (0), aurora (1), dusk (2), deep_blue (3), forest (4), bonfire (5), early_dawn (6), starry_sky (7), sunset (8), temple_candle (9), ink_wash (10), cyberpunk (11), romance (12), healing (13), focus (14), rainbow (15), custom (16)\n"
            "- led_hue: 0-360 degrees\n"
            "- led_saturation: 0-100%\n"
            "- led_value: 0-100%\n"
            "- light_laser_mode: on (0), breath (1), off (2)\n"
            "- light_projection_mode: on (0), breath (1), off (2)\n"
            "Use this tool for: \n"
            "1. Answering questions about current air purifier or lighting condition (e.g. what is the current PM2.5 level? Is the air purifier on? What is the current LED scene?)\n"
            "2. As the first step to control the air purifier or lighting (e.g. check current settings before changing)",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!RefreshDeviceStatus()) {
                    return "{\"success\": false, \"message\": \"Failed to get device status\"}";
                }
                return GetStatusJson();
            });
        
        // 控制开关
        mcp_server.AddTool("self.air_purifier.set_switch", 
            "Turn on or off the air purifier",
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                if (SetSwitch(state)) {
                    return "{\"success\": true, \"switch\": " + std::string(state ? "true" : "false") + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set switch state\"}";
                }
            });
        
        // 设置模式
        mcp_server.AddTool("self.air_purifier.set_mode", 
            "Set the operation mode of the air purifier (sleep: 0, auto: 1, fast: 2, manual: 3)",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 0, 3)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int mode = properties["mode"].value<int>();
                if (SetMode(mode)) {
                    return "{\"success\": true, \"mode\": " + std::to_string(mode) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set mode\"}";
                }
            });
        
        // 设置风速
        mcp_server.AddTool("self.air_purifier.set_fan_speed", 
            "Set the fan speed of the air purifier (low: 0, mid: 1, high: 2)",
            PropertyList({
                Property("speed", kPropertyTypeInteger, 0, 2)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int speed = properties["speed"].value<int>();
                if (SetFanSpeed(speed)) {
                    return "{\"success\": true, \"fan_speed\": " + std::to_string(speed) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set fan speed\"}";
                }
            });
        
        // 设置负离子
        mcp_server.AddTool("self.air_purifier.set_anion", 
            "Turn on or off the anion (negative ion) function of the air purifier",
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                if (SetAnion(state)) {
                    return "{\"success\": true, \"anion\": " + std::string(state ? "true" : "false") + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set anion\"}";
                }
            });
        
        // 设置童锁
        mcp_server.AddTool("self.air_purifier.set_child_lock", 
            "Enable or disable child lock (童锁) of the air purifier",
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                if (SetChildLock(state)) {
                    return "{\"success\": true, \"child_lock\": " + std::string(state ? "true" : "false") + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set child lock\"}";
                }
            });
        
        // 设置杀菌灯
        mcp_server.AddTool("self.air_purifier.set_uv_light", 
            "Turn on or off the UV sterilization light of the air purifier",
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                if (SetUvLight(state)) {
                    return "{\"success\": true, \"uv_light\": " + std::string(state ? "true" : "false") + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set UV light\"}";
                }
            });
        
        // 设置定时
        mcp_server.AddTool("self.air_purifier.set_countdown", 
            "Set the countdown timer of the air purifier. ONLY supports the following specific timer settings: 1 hour (0), 2 hours (1), 4 hours (2), 6 hours (3), or cancel timer (4). Any other timer values are NOT supported and DO NOT use this tool if you receive an unsupported timer value.",
            PropertyList({
                Property("timer", kPropertyTypeInteger, 0, 4)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int timer = properties["timer"].value<int>();
                if (SetCountdown(timer)) {
                    return "{\"success\": true, \"countdown\": " + std::to_string(timer) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set countdown\"}";
                }
            });
        
        // 设置灯光亮度
        mcp_server.AddTool("self.light.set_brightness", 
            "Set the brightness of the LED light (1-100). Always use this tool to set the brightness of the LED light.",
            PropertyList({
                Property("brightness", kPropertyTypeInteger, 1, 100)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int brightness = properties["brightness"].value<int>();
                if (SetLightBrightness(brightness)) {
                    ESP_LOGI(TAG, "Setting LED brightness to: %d", brightness);
                    return "{\"success\": true, \"brightness\": " + std::to_string(brightness) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set light brightness\"}";
                }
            });
        
        // 设置LED灯开关
        mcp_server.AddTool("self.light.set_led_switch", 
            "Turn on or off the LED light",
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                if (SetLedSwitch(state)) {
                    return "{\"success\": true, \"led_switch\": " + std::string(state ? "true" : "false") + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set LED switch\"}";
                }
            });
        
        // 设置LED呼吸开关
        mcp_server.AddTool("self.light.set_led_breath_switch", 
            "Turn on or off the breathing effect of the LED light",
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                if (SetLedBreathSwitch(state)) {
                    return "{\"success\": true, \"led_breath_switch\": " + std::string(state ? "true" : "false") + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set LED breath switch\"}";
                }
            });
        
        // 设置LED灯光场景，当前该工具将 custom 排除在外，因为 custom 场景需要使用 'self.light.set_led_colour' 工具来单独设置颜色
        mcp_server.AddTool("self.light.set_led_scene", 
            "Set the lighting scene of the LED light. ONLY supports the following specific scenes: moon_shadow (0), aurora (1), dusk (2), deep_blue (3), forest (4), bonfire (5), early_dawn (6), starry_sky (7), sunset (8), temple_candle (9), ink_wash (10), cyberpunk (11), romance (12), healing (13), focus (14), rainbow (15). DO NOT use this tool if you receive an unsupported scene value, USE 'self.light.set_led_colour' instead.",
            PropertyList({
                Property("scene", kPropertyTypeInteger, 0, 15)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int scene = properties["scene"].value<int>();
                if (SetLedScene(scene)) {
                    ESP_LOGI(TAG, "Setting LED scene to: %d", scene);
                    return "{\"success\": true, \"led_scene\": " + std::to_string(scene) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set LED scene\"}";
                }
            });
        
        // 设置LED颜色
        mcp_server.AddTool("self.light.set_led_colour", 
            "Set the color of the LED light using HSV values. Hue: 0-360 degrees, Saturation: 0-100%, Value: 0-100%",
            PropertyList({
                Property("hue", kPropertyTypeInteger, 0, 360),
                Property("saturation", kPropertyTypeInteger, 0, 100),
                Property("value", kPropertyTypeInteger, 0, 100)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                SetLedScene(16); // 发送指令，设置为 custom 灯光场景
                int hue = properties["hue"].value<int>();
                int saturation = properties["saturation"].value<int>();
                int value = properties["value"].value<int>();
                if (SetLedColour(hue, saturation, value)) {
                    return "{\"success\": true, \"hue\": " + std::to_string(hue) + ", \"saturation\": " + std::to_string(saturation) + ", \"value\": " + std::to_string(value) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set LED colour\"}";
                }
            });
        
        // 设置激光灯模式
        mcp_server.AddTool("self.light.set_laser_mode", 
            "Set the laser light mode (on: 0, breath: 1, off: 2)",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 0, 2)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int mode = properties["mode"].value<int>();
                if (SetLaserMode(mode)) {
                    return "{\"success\": true, \"laser_mode\": " + std::to_string(mode) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set laser mode\"}";
                }
            });
        
        // 设置投影灯模式
        mcp_server.AddTool("self.light.set_projection_mode", 
            "Set the projection light mode (on: 0, breath: 1, off: 2)",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 0, 2)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int mode = properties["mode"].value<int>();
                if (SetProjectionMode(mode)) {
                    return "{\"success\": true, \"projection_mode\": " + std::to_string(mode) + "}";
                } else {
                    return "{\"success\": false, \"message\": \"Failed to set projection mode\"}";
                }
            });
        
    } else {
        ESP_LOGE(TAG, "Failed to initialize UART controller");
    }
}

UartController::~UartController() {
    uart_driver_delete(uart_port_);
}

bool UartController::Initialize() {
    uart_config_t uart_config = {
        .baud_rate = baud_rate_,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(uart_port_, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_port_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(uart_port_, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 0, NULL, 0));
    
    return true;
}

bool UartController::SendFrame(uint8_t version, uint8_t cmd, uint8_t* data, uint16_t data_len) {
    // 构建数据帧: 帧头(2字节) + 版本(1字节) + 命令字(1字节) + 数据长度(2字节) + 数据 + 校验和(1字节)
    uint16_t frame_len = 7 + data_len;
    uint8_t* frame = new uint8_t[frame_len];
    
    // 帧头
    frame[0] = (FRAME_HEADER >> 8) & 0xFF;
    frame[1] = FRAME_HEADER & 0xFF;
    
    // 版本
    frame[2] = version;
    
    // 命令字
    frame[3] = cmd;
    
    // 数据长度
    frame[4] = (data_len >> 8) & 0xFF;
    frame[5] = data_len & 0xFF;
    
    // 数据
    if (data && data_len > 0) {
        memcpy(&frame[6], data, data_len);
    }
    
    // 校验和
    frame[frame_len - 1] = CalculateChecksum(frame, frame_len - 1);
    
    // 发送数据
    int written = uart_write_bytes(uart_port_, frame, frame_len);
    delete[] frame;
    
    if (written != frame_len) {
        ESP_LOGE(TAG, "Failed to send frame, expected %d bytes, written %d bytes", frame_len, written);
        return false;
    }
    
    ESP_LOGD(TAG, "Frame sent successfully, length: %d", frame_len);
    return true;
}

bool UartController::ReceiveMultiFrames(std::vector<uint8_t>& all_data) {
    uint8_t buffer[UART_BUFFER_SIZE];
    int length = uart_read_bytes(uart_port_, buffer, UART_BUFFER_SIZE, pdMS_TO_TICKS(1000));
    
    if (length <= 0) {
        ESP_LOGE(TAG, "No data received from UART");
        return false;
    }
    
    ESP_LOGD(TAG, "Received %d bytes from UART", length);
    
    // 将接收到的数据复制到vector中
    all_data.clear();
    all_data.insert(all_data.end(), buffer, buffer + length);
    
    return true;
}

bool UartController::ParseMultiFrames(const std::vector<uint8_t>& all_data) {
    if (all_data.size() < 7) {
        ESP_LOGE(TAG, "Received data too short: %u bytes", (unsigned int)all_data.size());
        return false;
    }
    
    ESP_LOGI(TAG, "Starting to parse multi-frame data, total bytes: %u", (unsigned int)all_data.size());
    
    size_t offset = 0;
    int frame_count = 0;  // 潜在的frame计数，用于区分有效frame和潜在frame
    int valid_frame_count = 0;  // 有效的frame计数，用于判断是否解析成功
    
    while (offset < all_data.size()) {
        // 寻找帧头 0x55 0xAA
        bool frame_header_found = false;
        while (offset + 1 < all_data.size()) {
            if (all_data[offset] == ((FRAME_HEADER >> 8) & 0xFF) && 
                all_data[offset + 1] == (FRAME_HEADER & 0xFF)) {
                frame_header_found = true;
                break;
            }
            offset++;
        }
        
        if (!frame_header_found) {
            ESP_LOGW(TAG, "No more valid frame headers found, stopping parse");
            break;
        }
        
        // 找到潜在的frame头，增加计数
        frame_count++;
        
        // 检查是否有足够的数据读取完整的帧头部
        if (offset + 7 > all_data.size()) {
            ESP_LOGW(TAG, "Potential frame %d: Insufficient data for complete frame header at offset %u", 
                     frame_count, (unsigned int)offset);
            break;
        }
        
        // 验证版本号 (0x03) - 在帧头正确的基础上检查版本号
        if (all_data[offset + 2] != VERSION_MCU_REPORT) {
            ESP_LOGW(TAG, "Potential frame %d: Invalid version at offset %u: 0x%02X, skipping to next potential frame", 
                     frame_count, (unsigned int)offset, all_data[offset + 2]);
            offset += 2; // 版本号错误时跳过帧头
            continue;
        }
        
        // 验证命令字 (0x07) - 在版本号正确的基础上检查命令字
        if (all_data[offset + 3] != CMD_REPORT) {
            ESP_LOGW(TAG, "Potential frame %d: Invalid command at offset %u: 0x%02X, skipping to next potential frame", 
                     frame_count, (unsigned int)offset, all_data[offset + 3]);
            offset += 3; // 命令字错误时跳过帧头+版本号
            continue;
        }
        
        // 获取数据长度
        uint16_t frame_data_len = (all_data[offset + 4] << 8) | all_data[offset + 5];
        uint16_t total_frame_len = 7 + frame_data_len;
        
        ESP_LOGD(TAG, "Potential frame %d: offset=%u, data_len=%d, total_len=%d", 
                 frame_count, (unsigned int)offset, frame_data_len, total_frame_len);
        
        // 检查是否有足够的数据
        if (offset + total_frame_len > all_data.size()) {
            ESP_LOGW(TAG, "Potential frame %d: Incomplete frame at offset %u, need %d bytes, have %u bytes, skipping", 
                     frame_count, (unsigned int)offset, total_frame_len, (unsigned int)(all_data.size() - offset));
            offset += 3;
            continue;
        }
        
        // 验证校验和
        uint8_t calculated_checksum = CalculateChecksum(&all_data[offset], total_frame_len - 1);
        if (calculated_checksum != all_data[offset + total_frame_len - 1]) {
            ESP_LOGW(TAG, "Potential frame %d: Checksum mismatch, calculated: 0x%02X, received: 0x%02X, skipping", 
                     frame_count, calculated_checksum, all_data[offset + total_frame_len - 1]);
            offset += 3;
            continue;
        }
        
        // 解析这个数据帧中的DP数据
        if (frame_data_len > 0) {
            ESP_LOGD(TAG, "Potential frame %d: Parsing DP data, length: %d", frame_count, frame_data_len);
            if (!ParseMcuReport(&all_data[offset + 6], frame_data_len)) {
                ESP_LOGW(TAG, "Potential frame %d: Failed to parse MCU report data, skipping", frame_count);
                offset += 3;
                continue;
            }
        }
        
        // 到这里说明frame解析成功，增加有效frame计数
        valid_frame_count++;
        ESP_LOGD(TAG, "Valid frame %d (potential frame %d): Successfully parsed, length: %d", 
                 valid_frame_count, frame_count, total_frame_len);
        
        // 移动到下一个帧
        offset += total_frame_len;
    }
    
    ESP_LOGI(TAG, "Parse completed: found %d potential frames, successfully parsed %d valid frames", 
             frame_count, valid_frame_count);
    
    // 只要解析到至少一个有效帧，就认为成功
    return valid_frame_count > 0;
}

uint8_t UartController::CalculateChecksum(const uint8_t* data, uint16_t length) {
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

bool UartController::ParseMcuReport(const uint8_t* data, uint16_t length) {
    if (length < 5) {
        ESP_LOGE(TAG, "MCU report data too short");
        return false;
    }
    
    uint8_t dp_id = data[0];
    uint8_t dp_type = data[1];
    uint16_t dp_len = (data[2] << 8) | data[3];
    
    if (length < (4 + dp_len)) {
        ESP_LOGE(TAG, "MCU report data length mismatch");
        return false;
    }
    
    uint8_t* dp_value = (uint8_t*)&data[4];
    
    switch (dp_id) {
        case DPID_PURIFIER_SWITCH:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.purifier_switch = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Purifier switch: %s", device_status_.purifier_switch ? "ON" : "OFF");
            }
            break;
            
        case DPID_INDOOR_PM25:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.indoor_pm25 = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Indoor PM2.5: %d", device_status_.indoor_pm25);
            }
            break;
            
        case DPID_PURIFIER_MODE:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.purifier_mode = dp_value[0];
                ESP_LOGI(TAG, "Purifier mode: %d", device_status_.purifier_mode);
            }
            break;
            
        case DPID_PURIFIER_FAN_SPEED:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.purifier_fan_speed = dp_value[0];
                ESP_LOGI(TAG, "Purifier fan speed: %d", device_status_.purifier_fan_speed);
            }
            break;
            
        case DPID_PURIFIER_FILTER_LIFE:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.purifier_filter_life = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Purifier filter life: %d%%", device_status_.purifier_filter_life);
            }
            break;
            
        case DPID_PURIFIER_ANION:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.purifier_anion = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Purifier anion: %s", device_status_.purifier_anion ? "ON" : "OFF");
            }
            break;
            
        case DPID_PURIFIER_CHILD_LOCK:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.purifier_child_lock = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Purifier child lock: %s", device_status_.purifier_child_lock ? "ON" : "OFF");
            }
            break;
            
        case DPID_PURIFIER_UV:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.purifier_uv = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Purifier UV: %s", device_status_.purifier_uv ? "ON" : "OFF");
            }
            break;
            
        case DPID_INDOOR_TEMP:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.indoor_temp = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Indoor temperature: %d°C", device_status_.indoor_temp);
            }
            break;
            
        case DPID_INDOOR_HUMIDITY:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.indoor_humidity = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Indoor humidity: %d%%", device_status_.indoor_humidity);
            }
            break;
            
        case DPID_COUNTDOWN_SET:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.countdown_set = dp_value[0];
                ESP_LOGI(TAG, "Countdown set: %d", device_status_.countdown_set);
            }
            break;
            
        case DPID_INDOOR_AIR_QUALITY:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.indoor_air_quality = dp_value[0];
                ESP_LOGI(TAG, "Indoor air quality: %d", device_status_.indoor_air_quality);
            }
            break;
            
        case DPID_LIGHT_BRIGHTNESS:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.light_brightness = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Light brightness: %d", device_status_.light_brightness);
            }
            break;
            
        case DPID_LIGHT_LED_SWITCH:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.light_led_switch = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Light LED switch: %s", device_status_.light_led_switch ? "ON" : "OFF");
            }
            break;
            
        case DPID_LIGHT_LED_BREATH_SWITCH:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.light_led_breath_switch = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Light LED breath switch: %s", device_status_.light_led_breath_switch ? "ON" : "OFF");
            }
            break;
            
        case DPID_LIGHT_LED_SCENE:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.light_led_scene = dp_value[0];
                ESP_LOGI(TAG, "Light LED scene: %d", device_status_.light_led_scene);
            }
            break;
            
        case DPID_LIGHT_LED_COLOUR:
            if (dp_type == DP_TYPE_STRING && dp_len > 0) {
                // 解析HSV字符串
                std::string hsv_str((char*)dp_value, dp_len);
                int hue, saturation, value;
                if (ParseHsvString(hsv_str, hue, saturation, value)) {
                    device_status_.led_hue = hue;
                    device_status_.led_saturation = saturation;
                    device_status_.led_value = value;
                    ESP_LOGI(TAG, "Light LED colour - Hue: %d°, Saturation: %d%%, Value: %d%%", hue, saturation, value);
                } else {
                    ESP_LOGW(TAG, "Failed to parse HSV string: %s", hsv_str.c_str());
                }
            }
            break;
            
        case DPID_LIGHT_LASER_MODE:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.light_laser_mode = dp_value[0];
                ESP_LOGI(TAG, "Light laser mode: %d", device_status_.light_laser_mode);
            }
            break;
            
        case DPID_LIGHT_PROJECTION_MODE:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.light_projection_mode = dp_value[0];
                ESP_LOGI(TAG, "Light projection mode: %d", device_status_.light_projection_mode);
            }
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown DP ID: 0x%02X", dp_id);
            break;
    }
    
    return true;
}

bool UartController::QueryMcuStatus() {
    // 发送查询MCU工作状态指令
    if (!SendFrame(VERSION_MODULE_SEND, CMD_QUERY_STATUS, nullptr, 0)) {
        ESP_LOGE(TAG, "Failed to send query status command");
        return false;
    }

    // 等待MCU处理和响应时间
    // MCU需要时间处理查询命令并上报所有DP点数据
    ESP_LOGI(TAG, "Waiting for MCU response (delay: 200ms)...");
    vTaskDelay(pdMS_TO_TICKS(200));  // 等待200ms让MCU处理并响应
    
    // 接收MCU上报数据
    std::vector<uint8_t> all_data;
    
    if (!ReceiveMultiFrames(all_data)) {
        ESP_LOGE(TAG, "Failed to receive MCU status report");
        return false;
    }
    
    // 解析上报数据
    if (!ParseMultiFrames(all_data)) {
        ESP_LOGE(TAG, "Failed to parse MCU report");
        return false;
    }
    
    status_initialized_ = true;
    return true;
}

bool UartController::SendControlCommand(uint8_t dp_id, uint8_t dp_type, const uint8_t* value, uint16_t value_len) {
    // 构建控制数据: dp_id(1字节) + dp_type(1字节) + dp_len(2字节) + dp_value
    uint16_t data_len = 4 + value_len;
    uint8_t* data = new uint8_t[data_len];
    
    data[0] = dp_id;
    data[1] = dp_type;
    data[2] = (value_len >> 8) & 0xFF;
    data[3] = value_len & 0xFF;
    
    if (value && value_len > 0) {
        memcpy(&data[4], value, value_len);
    }
    
    bool result = SendFrame(VERSION_MODULE_SEND, CMD_CONTROL, data, data_len);
    delete[] data;
    
    return result;
}

bool UartController::SetSwitch(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_PURIFIER_SWITCH, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetMode(int mode) {
    uint8_t value = mode & 0xFF;
    return SendControlCommand(DPID_PURIFIER_MODE, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetFanSpeed(int speed) {
    uint8_t value = speed & 0xFF;
    return SendControlCommand(DPID_PURIFIER_FAN_SPEED, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetAnion(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_PURIFIER_ANION, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetChildLock(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_PURIFIER_CHILD_LOCK, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetUvLight(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_PURIFIER_UV, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetCountdown(int timer) {
    uint8_t value = timer & 0xFF;
    return SendControlCommand(DPID_COUNTDOWN_SET, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetLightBrightness(int brightness) {
    uint8_t value[4];
    value[0] = (brightness >> 24) & 0xFF;
    value[1] = (brightness >> 16) & 0xFF;
    value[2] = (brightness >> 8) & 0xFF;
    value[3] = brightness & 0xFF;
    return SendControlCommand(DPID_LIGHT_BRIGHTNESS, DP_TYPE_VALUE, value, 4);
}

bool UartController::SetLedSwitch(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_LIGHT_LED_SWITCH, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetLedBreathSwitch(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_LIGHT_LED_BREATH_SWITCH, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetLedScene(int scene) {
    uint8_t value = scene & 0xFF;
    return SendControlCommand(DPID_LIGHT_LED_SCENE, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetLedColour(int hue, int saturation, int value) {
    std::string hsv_str = FormatHsvString(hue, saturation, value);
    return SendControlCommand(DPID_LIGHT_LED_COLOUR, DP_TYPE_STRING, (const uint8_t*)hsv_str.c_str(), hsv_str.length());
}

bool UartController::SetLaserMode(int mode) {
    uint8_t value = mode & 0xFF;
    return SendControlCommand(DPID_LIGHT_LASER_MODE, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetProjectionMode(int mode) {
    uint8_t value = mode & 0xFF;
    return SendControlCommand(DPID_LIGHT_PROJECTION_MODE, DP_TYPE_ENUM, &value, 1);
}

bool UartController::RefreshDeviceStatus() {
    return QueryMcuStatus();
}

bool UartController::ParseHsvString(const std::string& hsv_str, int& hue, int& saturation, int& value) {
    if (hsv_str.length() != 12) {
        ESP_LOGE(TAG, "HSV string length should be 12, got %d", (int)hsv_str.length());
        return false;
    }
    
    try {
        // 解析HSV格式：HHHHSSSSVVVV (每个值4位16进制)
        std::string hue_str = hsv_str.substr(0, 4);
        std::string sat_str = hsv_str.substr(4, 4);
        std::string val_str = hsv_str.substr(8, 4);
        
        int hue_raw = std::stoi(hue_str, nullptr, 16);
        int sat_raw = std::stoi(sat_str, nullptr, 16);
        int val_raw = std::stoi(val_str, nullptr, 16);
        
        // 转换为用户友好的值
        hue = hue_raw;  // 0-360度
        saturation = (sat_raw * 100) / 1000;  // 转换为百分比 (0-100%)
        value = (val_raw * 100) / 1000;       // 转换为百分比 (0-100%)
        
        ESP_LOGD(TAG, "Parsed HSV: %s -> H:%d°, S:%d%%, V:%d%%", hsv_str.c_str(), hue, saturation, value);
        return true;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to parse HSV string: %s, error: %s", hsv_str.c_str(), e.what());
        return false;
    }
}

std::string UartController::FormatHsvString(int hue, int saturation, int value) {
    // 将用户友好的值转换为协议格式
    int hue_raw = hue;  // 0-360度
    int sat_raw = (saturation * 1000) / 100;  // 转换为0-1000
    int val_raw = (value * 1000) / 100;       // 转换为0-1000
    
    // 格式化为12位16进制字符串：HHHHSSSSVVVV
    char hsv_str[13];
    snprintf(hsv_str, sizeof(hsv_str), "%04X%04X%04X", hue_raw, sat_raw, val_raw);
    
    ESP_LOGI(TAG, "Formatted HSV: H:%d°, S:%d%%, V:%d%% -> %s", hue, saturation, value, hsv_str);
    return std::string(hsv_str);
}

std::string UartController::GetStatusJson() const {
    cJSON* json = cJSON_CreateObject();
    
    // 净化器相关状态
    cJSON_AddBoolToObject(json, "purifier_switch", device_status_.purifier_switch);
    cJSON_AddNumberToObject(json, "indoor_pm25", device_status_.indoor_pm25);
    cJSON_AddNumberToObject(json, "purifier_mode", device_status_.purifier_mode);
    cJSON_AddNumberToObject(json, "purifier_fan_speed", device_status_.purifier_fan_speed);
    cJSON_AddNumberToObject(json, "purifier_filter_life", device_status_.purifier_filter_life);
    cJSON_AddBoolToObject(json, "purifier_anion", device_status_.purifier_anion);
    cJSON_AddBoolToObject(json, "purifier_child_lock", device_status_.purifier_child_lock);
    cJSON_AddBoolToObject(json, "purifier_uv", device_status_.purifier_uv);
    cJSON_AddNumberToObject(json, "indoor_temp", device_status_.indoor_temp);
    cJSON_AddNumberToObject(json, "indoor_humidity", device_status_.indoor_humidity);
    cJSON_AddNumberToObject(json, "countdown_set", device_status_.countdown_set);
    cJSON_AddNumberToObject(json, "indoor_air_quality", device_status_.indoor_air_quality);
    
    // 灯光相关状态
    cJSON_AddNumberToObject(json, "light_brightness", device_status_.light_brightness);
    cJSON_AddBoolToObject(json, "light_led_switch", device_status_.light_led_switch);
    cJSON_AddBoolToObject(json, "light_led_breath_switch", device_status_.light_led_breath_switch);
    cJSON_AddNumberToObject(json, "light_led_scene", device_status_.light_led_scene);
    cJSON_AddNumberToObject(json, "led_hue", device_status_.led_hue);
    cJSON_AddNumberToObject(json, "led_saturation", device_status_.led_saturation);
    cJSON_AddNumberToObject(json, "led_value", device_status_.led_value);
    cJSON_AddNumberToObject(json, "light_laser_mode", device_status_.light_laser_mode);
    cJSON_AddNumberToObject(json, "light_projection_mode", device_status_.light_projection_mode);
    
    cJSON_AddBoolToObject(json, "status_initialized", status_initialized_);
    
    char* json_str = cJSON_PrintUnformatted(json);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);
    
    return result;
} 