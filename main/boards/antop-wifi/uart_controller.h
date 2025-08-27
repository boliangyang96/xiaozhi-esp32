#ifndef __UART_CONTROLLER_H__
#define __UART_CONTROLLER_H__

#include "mcp_server.h"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <string>
#include <vector>
#include <map>
#include <cJSON.h>

// 串口配置
#define UART_TXD_PIN GPIO_NUM_17
#define UART_RXD_PIN GPIO_NUM_18
#define UART_PORT UART_NUM_1
#define UART_BAUD_RATE 9600
#define UART_BUFFER_SIZE 1024

// 协议相关定义
#define FRAME_HEADER 0x55AA
#define VERSION_MODULE_SEND 0x00
#define VERSION_MCU_REPORT 0x03
#define CMD_CONTROL 0x06
#define CMD_REPORT 0x07
#define CMD_QUERY_STATUS 0x08

// DP ID定义
#define DPID_PURIFIER_SWITCH 0x01
#define DPID_INDOOR_PM25 0x02
#define DPID_PURIFIER_MODE 0x03
#define DPID_PURIFIER_FAN_SPEED 0x04
#define DPID_PURIFIER_FILTER_LIFE 0x05
#define DPID_PURIFIER_ANION 0x06
#define DPID_PURIFIER_CHILD_LOCK 0x07
#define DPID_PURIFIER_UV 0x09
#define DPID_INDOOR_TEMP 0x0C
#define DPID_INDOOR_HUMIDITY 0x0D
#define DPID_COUNTDOWN_SET 0x12
#define DPID_INDOOR_AIR_QUALITY 0x15
#define DPID_LIGHT_BRIGHTNESS 0x65
#define DPID_LIGHT_LED_SWITCH 0x66
#define DPID_LIGHT_LED_BREATH_SWITCH 0x67
#define DPID_LIGHT_LED_SCENE 0x68
#define DPID_LIGHT_LED_COLOUR 0x69
#define DPID_LIGHT_LASER_MODE 0x6A
#define DPID_LIGHT_PROJECTION_MODE 0x6B

// 数据类型定义
#define DP_TYPE_BOOL 0x01
#define DP_TYPE_VALUE 0x02
#define DP_TYPE_STRING 0x03
#define DP_TYPE_ENUM 0x04

// 模式定义
#define MODE_SLEEP 0x00
#define MODE_AUTO 0x01
#define MODE_FAST 0x02
#define MODE_MANUAL 0x03

// 风速定义
#define FAN_SPEED_LOW 0x00
#define FAN_SPEED_MID 0x01
#define FAN_SPEED_HIGH 0x02

// 空气质量定义
#define AIR_QUALITY_GREAT 0x00
#define AIR_QUALITY_MEDIUM 0x01
#define AIR_QUALITY_SEVERE 0x02

// 定时定义
#define TIMER_1H 0x00
#define TIMER_2H 0x01
#define TIMER_4H 0x02
#define TIMER_6H 0x03
#define TIMER_CANCEL 0x04

// 灯光场景定义
#define LIGHT_SCENE_MOON_SHADOW 0x00
#define LIGHT_SCENE_AURORA 0x01
#define LIGHT_SCENE_DUSK 0x02
#define LIGHT_SCENE_DEEP_BLUE 0x03
#define LIGHT_SCENE_FOREST 0x04
#define LIGHT_SCENE_BONFIRE 0x05
#define LIGHT_SCENE_EARLY_DAWN 0x06
#define LIGHT_SCENE_STARRY_SKY 0x07
#define LIGHT_SCENE_SUNSET 0x08
#define LIGHT_SCENE_TEMPLE_CANDLE 0x09
#define LIGHT_SCENE_INK_WASH 0x0A
#define LIGHT_SCENE_CYBERPUNK 0x0B
#define LIGHT_SCENE_ROMANCE 0x0C
#define LIGHT_SCENE_HEALING 0x0D
#define LIGHT_SCENE_FOCUS 0x0E
#define LIGHT_SCENE_RAINBOW 0x0F
#define LIGHT_SCENE_CUSTOM 0x10

// 激光灯和投影灯模式定义
#define LIGHT_MODE_ON 0x00
#define LIGHT_MODE_BREATH 0x01
#define LIGHT_MODE_OFF 0x02

struct DeviceStatus {
    // 净化器相关
    bool purifier_switch;
    int indoor_pm25;
    int purifier_mode;
    int purifier_fan_speed;
    int purifier_filter_life;
    bool purifier_anion;
    bool purifier_child_lock;
    bool purifier_uv;
    int indoor_temp;
    int indoor_humidity;
    int countdown_set;
    int indoor_air_quality;
    
    // 灯光相关
    int light_brightness;
    bool light_led_switch;
    bool light_led_breath_switch;
    int light_led_scene;
    int led_hue;        // HSV中的H值 (0-360度)
    int led_saturation; // HSV中的S值 (0-100%)
    int led_value;      // HSV中的V值 (0-100%)
    int light_laser_mode;
    int light_projection_mode;
};

class UartController {
private:
    uart_port_t uart_port_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    int baud_rate_;
    DeviceStatus device_status_;
    bool status_initialized_;
    
    // 发送数据帧
    bool SendFrame(uint8_t version, uint8_t cmd, uint8_t* data, uint16_t data_len);
    
    // 接收多帧数据
    bool ReceiveMultiFrames(std::vector<uint8_t>& all_data);
    
    // 分割和验证多个数据帧
    bool ParseMultiFrames(const std::vector<uint8_t>& all_data);
    
    // 计算校验和
    uint8_t CalculateChecksum(const uint8_t* data, uint16_t length);
    
    // 解析MCU上报数据
    bool ParseMcuReport(const uint8_t* data, uint16_t length);
    
    // 查询MCU工作状态
    bool QueryMcuStatus();
    
    // 发送控制指令
    bool SendControlCommand(uint8_t dp_id, uint8_t dp_type, const uint8_t* value, uint16_t value_len);

public:
    UartController(gpio_num_t tx_pin = UART_TXD_PIN, 
                   gpio_num_t rx_pin = UART_RXD_PIN,
                   uart_port_t uart_port = UART_PORT,
                   int baud_rate = UART_BAUD_RATE);
    
    ~UartController();
    
    // 初始化UART
    bool Initialize();
    
    // 获取设备状态
    const DeviceStatus& GetDeviceStatus() const { return device_status_; }
    
    // 控制开关
    bool SetSwitch(bool state);
    
    // 设置模式
    bool SetMode(int mode);
    
    // 设置风速
    bool SetFanSpeed(int speed);

    // 设置负离子
    bool SetAnion(bool state);
    
    // 设置童锁
    bool SetChildLock(bool state);
    
    // 设置杀菌灯
    bool SetUvLight(bool state);
    
    // 设置定时
    bool SetCountdown(int timer);
    
    // 设置灯光亮度
    bool SetLightBrightness(int brightness);
    
    // 设置LED灯开关
    bool SetLedSwitch(bool state);
    
    // 设置LED呼吸开关
    bool SetLedBreathSwitch(bool state);
    
    // 设置LED灯光场景
    bool SetLedScene(int scene);
    
    // 设置LED颜色 (HSV格式)
    bool SetLedColour(int hue, int saturation, int value);
    
    // 设置激光灯模式
    bool SetLaserMode(int mode);
    
    // 设置投影灯模式
    bool SetProjectionMode(int mode);
    
    // 刷新设备状态
    bool RefreshDeviceStatus();
    
    // 获取状态JSON字符串
    std::string GetStatusJson() const;

private:
    // HSV字符串解析
    bool ParseHsvString(const std::string& hsv_str, int& hue, int& saturation, int& value);
    
    // HSV转换为字符串
    std::string FormatHsvString(int hue, int saturation, int value);
};

#endif // __UART_CONTROLLER_H__ 