#include "breathing_lamp_controller.h"

#define TAG "BreathingLampController"

BreathingLampController::BreathingLampController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
    // 配置GPIO为输出模式
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << gpio_num_),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(gpio_num_, 0);  // 初始状态为关闭
    
    ESP_LOGI(TAG, "BreathingLampController initialized on GPIO %d", gpio_num_);

    // 注册MCP工具
    auto& mcp_server = McpServer::GetInstance();
    
    // 获取LED状态
    mcp_server.AddTool("self.lamp.get_state", 
        "Get the current state of the lamp including power status, breathing mode, and timing parameters", 
        PropertyList(), 
        [this](const PropertyList& properties) -> ReturnValue {
            std::string state = "{\"power\": ";
            state += power_ ? "true" : "false";
            state += ", \"breathing_mode\": ";
            state += breathing_mode_ ? "true" : "false";
            state += ", \"on_duration_ms\": ";
            state += std::to_string(on_duration_);
            state += ", \"off_duration_ms\": ";
            state += std::to_string(off_duration_);
            state += "}";
            return state;
        });

    // 开灯
    mcp_server.AddTool("self.lamp.turn_on", 
        "Turn on the lamp. This will stop breathing mode if it's currently active.", 
        PropertyList(), 
        [this](const PropertyList& properties) -> ReturnValue {
            TurnOn();
            return true;
        });

    // 关灯
    mcp_server.AddTool("self.lamp.turn_off", 
        "Turn off the lamp. This will stop breathing mode if it's currently active.", 
        PropertyList(), 
        [this](const PropertyList& properties) -> ReturnValue {
            TurnOff();
            return true;
        });

    // 设置呼吸灯模式
    mcp_server.AddTool("self.lamp.set_breathing_mode", 
        "Set the lamp to breathing mode with custom on/off durations. The lamp will cycle between on and off states continuously.", 
        PropertyList({
            Property("on_duration_ms", kPropertyTypeInteger, 1000, 100, 10000),    // 开灯时间100ms-10s，默认1s
            Property("off_duration_ms", kPropertyTypeInteger, 1000, 100, 10000)    // 关灯时间100ms-10s，默认1s
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            int on_duration = properties["on_duration_ms"].value<int>();
            int off_duration = properties["off_duration_ms"].value<int>();
            SetBreathingMode(on_duration, off_duration);
            std::string result = "{\"breathing_mode\": true, \"on_duration_ms\": ";
            result += std::to_string(on_duration);
            result += ", \"off_duration_ms\": ";
            result += std::to_string(off_duration);
            result += "}";
            return result;
        });

    // 停止呼吸灯模式
    mcp_server.AddTool("self.lamp.stop_breathing_mode", 
        "Stop the breathing mode and keep the lamp in its current state", 
        PropertyList(), 
        [this](const PropertyList& properties) -> ReturnValue {
            StopBreathingMode();
            return "{\"breathing_mode\": false}";
        });
}

BreathingLampController::~BreathingLampController() {
    StopBreathingMode();  // 确保停止呼吸灯任务
}

void BreathingLampController::SetLedState(bool state) {
    gpio_set_level(gpio_num_, state ? 1 : 0);
    power_ = state;
}

void BreathingLampController::TurnOn() {
    ESP_LOGI(TAG, "Turning lamp on");
    StopBreathingMode();  // 停止呼吸灯模式
    SetLedState(true);
}

void BreathingLampController::TurnOff() {
    ESP_LOGI(TAG, "Turning lamp off");
    StopBreathingMode();  // 停止呼吸灯模式
    SetLedState(false);
}

void BreathingLampController::SetBreathingMode(int on_duration_ms, int off_duration_ms) {
    ESP_LOGI(TAG, "Setting breathing mode: on=%dms, off=%dms", on_duration_ms, off_duration_ms);
    
    // 更新参数
    on_duration_ = on_duration_ms;
    off_duration_ = off_duration_ms;
    
    // 如果已经在呼吸模式，先停止当前任务
    StopBreathingMode();
    
    // 设置呼吸模式标志
    breathing_mode_ = true;
    
    // 创建呼吸灯任务
    BaseType_t result = xTaskCreate(
        BreathingTask,               // 任务函数
        "breathing_lamp_task",       // 任务名称
        2048,                        // 堆栈大小
        this,                        // 传递给任务的参数
        1,                           // 任务优先级
        &breathing_task_handle_      // 任务句柄
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create breathing task");
        breathing_mode_ = false;
    }
}

void BreathingLampController::StopBreathingMode() {
    if (breathing_task_handle_ != nullptr) {
        ESP_LOGI(TAG, "Stopping breathing mode");
        breathing_mode_ = false;
        
        // 删除任务
        vTaskDelete(breathing_task_handle_);
        breathing_task_handle_ = nullptr;
    }
}

void BreathingLampController::BreathingTask(void* pvParameters) {
    BreathingLampController* controller = static_cast<BreathingLampController*>(pvParameters);
    
    ESP_LOGI(TAG, "Breathing task started");
    
    while (controller->breathing_mode_) {
        // 开灯
        controller->SetLedState(true);
        vTaskDelay(pdMS_TO_TICKS(controller->on_duration_));
        
        // 检查是否应该继续
        if (!controller->breathing_mode_) {
            break;
        }
        
        // 关灯
        controller->SetLedState(false);
        vTaskDelay(pdMS_TO_TICKS(controller->off_duration_));
    }
    
    ESP_LOGI(TAG, "Breathing task ended");
    
    // 任务结束前清理
    controller->breathing_task_handle_ = nullptr;
    vTaskDelete(nullptr);  // 删除当前任务
}
