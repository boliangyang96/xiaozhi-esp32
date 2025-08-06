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
        mcp_server.AddTool("self.air_purifier.get_status", 
            "Get the current status of the air purifier including switch state, PM2.5, mode, fan speed, filter life, etc.",
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
        
        // 设置童锁
        mcp_server.AddTool("self.air_purifier.set_child_lock", 
            "Enable or disable child lock",
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
            "Turn on or off the UV sterilization light",
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
            "Set the countdown timer (1h: 0, 2h: 1, 4h: 2, 6h: 3, cancel: 4)",
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
        
        // 查询PM2.5
        mcp_server.AddTool("self.air_purifier.get_pm25", 
            "Get the current PM2.5 value",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!RefreshDeviceStatus()) {
                    return "{\"success\": false, \"message\": \"Failed to get PM2.5 data\"}";
                }
                return "{\"success\": true, \"pm25\": " + std::to_string(device_status_.pm25) + "}";
            });
        
        // 查询室内温度
        mcp_server.AddTool("self.air_purifier.get_temperature", 
            "Get the current indoor temperature",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!RefreshDeviceStatus()) {
                    return "{\"success\": false, \"message\": \"Failed to get temperature data\"}";
                }
                return "{\"success\": true, \"temperature\": " + std::to_string(device_status_.indoor_temp) + "}";
            });
        
        // 查询室内湿度
        mcp_server.AddTool("self.air_purifier.get_humidity", 
            "Get the current indoor humidity",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!RefreshDeviceStatus()) {
                    return "{\"success\": false, \"message\": \"Failed to get humidity data\"}";
                }
                return "{\"success\": true, \"humidity\": " + std::to_string(device_status_.indoor_humidity) + "}";
            });
        
        // 查询空气质量
        mcp_server.AddTool("self.air_purifier.get_air_quality", 
            "Get the current air quality (great: 0, medium: 1, severe: 2)",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!RefreshDeviceStatus()) {
                    return "{\"success\": false, \"message\": \"Failed to get air quality data\"}";
                }
                return "{\"success\": true, \"air_quality\": " + std::to_string(device_status_.air_quality) + "}";
            });
        
        // 查询滤网剩余
        mcp_server.AddTool("self.air_purifier.get_filter_life", 
            "Get the remaining filter life percentage",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!RefreshDeviceStatus()) {
                    return "{\"success\": false, \"message\": \"Failed to get filter life data\"}";
                }
                return "{\"success\": true, \"filter_life\": " + std::to_string(device_status_.filter_life) + "}";
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
    int frame_count = 0;
    
    while (offset + 7 <= all_data.size()) {
        // 查找帧头 0x55 0xAA
        if (all_data[offset] != ((FRAME_HEADER >> 8) & 0xFF) || 
            all_data[offset + 1] != (FRAME_HEADER & 0xFF)) {
            ESP_LOGE(TAG, "Invalid frame header at offset %u: 0x%02X 0x%02X", 
                     (unsigned int)offset, all_data[offset], all_data[offset + 1]);
            return false;
        }
        
        // 验证版本号 (0x03)
        if (all_data[offset + 2] != VERSION_MCU_REPORT) {
            ESP_LOGE(TAG, "Invalid version at offset %u: 0x%02X", (unsigned int)offset, all_data[offset + 2]);
            return false;
        }
        
        // 验证命令字 (0x07)
        if (all_data[offset + 3] != CMD_REPORT) {
            ESP_LOGE(TAG, "Invalid command at offset %u: 0x%02X", (unsigned int)offset, all_data[offset + 3]);
            return false;
        }
        
        // 获取数据长度
        uint16_t frame_data_len = (all_data[offset + 4] << 8) | all_data[offset + 5];
        uint16_t total_frame_len = 7 + frame_data_len;
        
        ESP_LOGD(TAG, "Frame %d: offset=%u, data_len=%d, total_len=%d", 
                 frame_count, (unsigned int)offset, frame_data_len, total_frame_len);
        
        // 检查是否有足够的数据
        if (offset + total_frame_len > all_data.size()) {
            ESP_LOGE(TAG, "Incomplete frame at offset %u, need %d bytes, have %u bytes", 
                     (unsigned int)offset, total_frame_len, (unsigned int)(all_data.size() - offset));
            return false;
        }
        
        // 验证校验和
        uint8_t calculated_checksum = CalculateChecksum(&all_data[offset], total_frame_len - 1);
        if (calculated_checksum != all_data[offset + total_frame_len - 1]) {
            ESP_LOGE(TAG, "Checksum mismatch at frame %d, calculated: 0x%02X, received: 0x%02X", 
                     frame_count, calculated_checksum, all_data[offset + total_frame_len - 1]);
            return false;
        }
        
        // 解析这个数据帧中的DP数据
        if (frame_data_len > 0) {
            ESP_LOGD(TAG, "Parsing DP data in frame %d, length: %d", frame_count, frame_data_len);
            if (!ParseMcuReport(&all_data[offset + 6], frame_data_len)) {
                ESP_LOGE(TAG, "Failed to parse MCU report data in frame %d", frame_count);
                return false;
            }
        }
        
        ESP_LOGD(TAG, "Successfully parsed frame %d, length: %d", frame_count, total_frame_len);
        
        // 移动到下一个帧
        offset += total_frame_len;
        frame_count++;
    }
    
    ESP_LOGI(TAG, "Successfully parsed %d frames", frame_count);
    return true;
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
        case DPID_SWITCH:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.switch_state = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Switch state: %s", device_status_.switch_state ? "ON" : "OFF");
            }
            break;
            
        case DPID_PM25:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.pm25 = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "PM2.5: %d", device_status_.pm25);
            }
            break;
            
        case DPID_MODE:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.mode = dp_value[0];
                ESP_LOGI(TAG, "Mode: %d", device_status_.mode);
            }
            break;
            
        case DPID_FAN_SPEED:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.fan_speed = dp_value[0];
                ESP_LOGI(TAG, "Fan speed: %d", device_status_.fan_speed);
            }
            break;
            
        case DPID_FILTER_LIFE:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.filter_life = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Filter life: %d%%", device_status_.filter_life);
            }
            break;
            
        case DPID_CHILD_LOCK:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.child_lock = (dp_value[0] != 0);
                ESP_LOGI(TAG, "Child lock: %s", device_status_.child_lock ? "ON" : "OFF");
            }
            break;
            
        case DPID_UV:
            if (dp_type == DP_TYPE_BOOL && dp_len == 1) {
                device_status_.uv_light = (dp_value[0] != 0);
                ESP_LOGI(TAG, "UV light: %s", device_status_.uv_light ? "ON" : "OFF");
            }
            break;
            
        case DPID_TEMP_INDOOR:
            if (dp_type == DP_TYPE_VALUE && dp_len == 4) {
                device_status_.indoor_temp = (dp_value[0] << 24) | (dp_value[1] << 16) | (dp_value[2] << 8) | dp_value[3];
                ESP_LOGI(TAG, "Indoor temperature: %d°C", device_status_.indoor_temp);
            }
            break;
            
        case DPID_HUMIDITY:
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
            
        case DPID_AIR_QUALITY:
            if (dp_type == DP_TYPE_ENUM && dp_len == 1) {
                device_status_.air_quality = dp_value[0];
                ESP_LOGI(TAG, "Air quality: %d", device_status_.air_quality);
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
    // 根据协议，MCU需要时间处理查询命令并上报所有DP点数据
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
    return SendControlCommand(DPID_SWITCH, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetMode(int mode) {
    uint8_t value = mode & 0xFF;
    return SendControlCommand(DPID_MODE, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetFanSpeed(int speed) {
    uint8_t value = speed & 0xFF;
    return SendControlCommand(DPID_FAN_SPEED, DP_TYPE_ENUM, &value, 1);
}

bool UartController::SetChildLock(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_CHILD_LOCK, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetUvLight(bool state) {
    uint8_t value = state ? 0x01 : 0x00;
    return SendControlCommand(DPID_UV, DP_TYPE_BOOL, &value, 1);
}

bool UartController::SetCountdown(int timer) {
    uint8_t value = timer & 0xFF;
    return SendControlCommand(DPID_COUNTDOWN_SET, DP_TYPE_ENUM, &value, 1);
}

bool UartController::RefreshDeviceStatus() {
    return QueryMcuStatus();
}

std::string UartController::GetStatusJson() const {
    cJSON* json = cJSON_CreateObject();
    
    cJSON_AddBoolToObject(json, "switch", device_status_.switch_state);
    cJSON_AddNumberToObject(json, "pm25", device_status_.pm25);
    cJSON_AddNumberToObject(json, "mode", device_status_.mode);
    cJSON_AddNumberToObject(json, "fan_speed", device_status_.fan_speed);
    cJSON_AddNumberToObject(json, "filter_life", device_status_.filter_life);
    cJSON_AddBoolToObject(json, "child_lock", device_status_.child_lock);
    cJSON_AddBoolToObject(json, "uv_light", device_status_.uv_light);
    cJSON_AddNumberToObject(json, "indoor_temp", device_status_.indoor_temp);
    cJSON_AddNumberToObject(json, "indoor_humidity", device_status_.indoor_humidity);
    cJSON_AddNumberToObject(json, "countdown_set", device_status_.countdown_set);
    cJSON_AddNumberToObject(json, "air_quality", device_status_.air_quality);
    cJSON_AddBoolToObject(json, "status_initialized", status_initialized_);
    
    char* json_str = cJSON_PrintUnformatted(json);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);
    
    return result;
} 