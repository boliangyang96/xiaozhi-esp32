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
#define DPID_SWITCH 0x01
#define DPID_PM25 0x02
#define DPID_MODE 0x03
#define DPID_FAN_SPEED 0x04
#define DPID_FILTER_LIFE 0x05
#define DPID_CHILD_LOCK 0x07
#define DPID_UV 0x09
#define DPID_TEMP_INDOOR 0x0C
#define DPID_HUMIDITY 0x0D
#define DPID_COUNTDOWN_SET 0x12
#define DPID_AIR_QUALITY 0x15

// 数据类型定义
#define DP_TYPE_BOOL 0x01
#define DP_TYPE_VALUE 0x02
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

struct DeviceStatus {
    bool switch_state;
    int pm25;
    int mode;
    int fan_speed;
    int filter_life;
    bool child_lock;
    bool uv_light;
    int indoor_temp;
    int indoor_humidity;
    int countdown_set;
    int air_quality;
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
    
    // 设置童锁
    bool SetChildLock(bool state);
    
    // 设置杀菌灯
    bool SetUvLight(bool state);
    
    // 设置定时
    bool SetCountdown(int timer);
    
    // 刷新设备状态
    bool RefreshDeviceStatus();
    
    // 获取状态JSON字符串
    std::string GetStatusJson() const;
};

#endif // __UART_CONTROLLER_H__ 