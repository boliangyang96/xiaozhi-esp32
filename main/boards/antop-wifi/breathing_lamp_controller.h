#ifndef __BREATHING_LAMP_CONTROLLER_H__
#define __BREATHING_LAMP_CONTROLLER_H__

#include "mcp_server.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

class BreathingLampController {
private:
    bool power_ = false;              // LED开关状态
    bool breathing_mode_ = false;     // 呼吸灯模式状态
    int on_duration_ = 1000;          // 开灯持续时间（毫秒）
    int off_duration_ = 1000;         // 关灯持续时间（毫秒）
    gpio_num_t gpio_num_;             // GPIO引脚编号
    TaskHandle_t breathing_task_handle_ = nullptr; // 呼吸灯任务句柄

    // 呼吸灯任务函数（静态函数，用于FreeRTOS任务）
    static void BreathingTask(void* pvParameters);
    
    // 内部控制LED状态的函数
    void SetLedState(bool state);

public:
    BreathingLampController(gpio_num_t gpio_num);
    ~BreathingLampController();
    
    // 基础控制功能
    bool GetState() const { return power_ || breathing_mode_; }
    void TurnOn();
    void TurnOff();
    
    // 呼吸灯功能
    void SetBreathingMode(int on_duration_ms, int off_duration_ms);
    void StopBreathingMode();
    bool IsBreathingMode() const { return breathing_mode_; }
    
    // 获取当前呼吸灯参数
    int GetOnDuration() const { return on_duration_; }
    int GetOffDuration() const { return off_duration_; }
};

#endif // __BREATHING_LAMP_CONTROLLER_H__
