#ifndef __BREATHING_LAMP_CONTROLLER_H__
#define __BREATHING_LAMP_CONTROLLER_H__

#include "mcp_server.h"
#include <thread>
#include <atomic>
#include <chrono>

class BreathingLampController {
private:
    bool power_ = false;
    bool breathing_mode_ = false;
    gpio_num_t gpio_num_;
    std::atomic<bool> stop_breathing_{false};
    std::thread breathing_thread_;

public:
    BreathingLampController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(gpio_num_, 0);

        auto& mcp_server = McpServer::GetInstance();
        
        // 基础灯光控制工具
        mcp_server.AddTool("self.lamp.get_state", "Get the power state of the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return power_ ? "{\"power\": true}" : "{\"power\": false}";
        });

        mcp_server.AddTool("self.lamp.turn_on", "Turn on the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            StopBreathing();
            power_ = true;
            gpio_set_level(gpio_num_, 1);
            return true;
        });

        mcp_server.AddTool("self.lamp.turn_off", "Turn off the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            StopBreathing();
            power_ = false;
            gpio_set_level(gpio_num_, 0);
            return true;
        });

        // 呼吸灯控制工具
        mcp_server.AddTool("self.lamp.start_breathing", 
            "Start breathing light mode. The lamp will turn on for x seconds, then turn off for y seconds, repeating this cycle continuously.\n"
            "Args:\n"
            "  `on_duration`: Duration in seconds to keep the lamp on (integer, 1-60)\n"
            "  `off_duration`: Duration in seconds to keep the lamp off (integer, 1-60)",
            PropertyList({
                Property("on_duration", kPropertyTypeInteger, 1, 1, 60),
                Property("off_duration", kPropertyTypeInteger, 1, 1, 60)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                int on_duration = properties["on_duration"].value<int>();
                int off_duration = properties["off_duration"].value<int>();
                StartBreathing(on_duration, off_duration);
                return true;
            });

        mcp_server.AddTool("self.lamp.stop_breathing", 
            "Stop breathing light mode and turn off the lamp", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                StopBreathing();
                return true;
            });

        mcp_server.AddTool("self.lamp.get_breathing_state", 
            "Get the current breathing mode state", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                return breathing_mode_ ? "{\"breathing\": true}" : "{\"breathing\": false}";
            });
    }

    ~BreathingLampController() {
        StopBreathing();
    }

private:
    void StartBreathing(int on_duration, int off_duration) {
        StopBreathing(); // 停止之前的呼吸线程
        
        breathing_mode_ = true;
        stop_breathing_ = false;
        
        breathing_thread_ = std::thread([this, on_duration, off_duration]() {
            while (!stop_breathing_) {
                // 打开灯
                power_ = true;
                gpio_set_level(gpio_num_, 1);
                
                // 等待指定的开启时间
                for (int i = 0; i < on_duration && !stop_breathing_; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                
                if (stop_breathing_) break;
                
                // 关闭灯
                power_ = false;
                gpio_set_level(gpio_num_, 0);
                
                // 等待指定的关闭时间
                for (int i = 0; i < off_duration && !stop_breathing_; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        });
        
        breathing_thread_.detach();
    }

    void StopBreathing() {
        if (breathing_mode_) {
            stop_breathing_ = true;
            breathing_mode_ = false;
            
            // 等待线程结束
            if (breathing_thread_.joinable()) {
                breathing_thread_.join();
            }
            
            // 确保灯关闭
            power_ = false;
            gpio_set_level(gpio_num_, 0);
        }
    }
};

#endif // __BREATHING_LAMP_CONTROLLER_H__ 