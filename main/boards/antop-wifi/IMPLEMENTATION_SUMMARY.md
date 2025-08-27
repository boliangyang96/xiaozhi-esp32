# UART串口通信MCP实现总结

## 实现概述

本次实现完成了通过MCP协议实现UART串口通信的功能，用于与外部MCU进行通信控制AI桌面空净星空灯设备。该设备集成了空气净化器和多种灯光系统（四色灯、激光灯、投影灯），支持完整的设备控制和状态监控。

## 文件结构

### 核心文件
- `uart_controller.h` - UART控制器头文件
- `uart_controller.cc` - UART控制器实现文件
- `README_UART.md` - 使用说明文档

### 修改文件
- `antop-wifi.cc` - 主板文件，集成了UART控制器

## 实现功能

### 1. 协议实现
- 完整实现了涂鸦协议的数据帧格式
- 支持模块发送控制指令和MCU上报数据
- 实现了校验和计算和验证
- 支持查询MCU工作状态功能
- **新增：支持多帧数据处理，MCU一次性上报所有DP数据**

### 2. 多帧数据处理
- **ReceiveMultiFrames**: 接收MCU上报的所有数据
- **ParseMultiFrames**: 解析多个连续的数据帧
- 支持按帧头(0x55 0xAA)和版本号(0x03)分割多个数据帧
- 每个数据帧独立验证校验和
- **使用协议常量**: 避免硬编码，使用FRAME_HEADER等常量进行帧头验证

### 3. MCP工具接口
实现了以下MCP工具：

#### 空气净化器控制工具
- `self.air_purifier.set_switch` - 控制净化器开关
- `self.air_purifier.set_mode` - 设置净化器模式
- `self.air_purifier.set_fan_speed` - 设置净化器风速
- `self.air_purifier.set_anion` - 设置负离子开关
- `self.air_purifier.set_child_lock` - 设置童锁
- `self.air_purifier.set_uv_light` - 设置杀菌灯
- `self.air_purifier.set_countdown` - 设置定时器

#### 灯光系统控制工具
- `self.light.set_brightness` - 设置四色灯白光亮度
- `self.light.set_led_switch` - 设置四色LED灯开关
- `self.light.set_led_breath_switch` - 设置四色LED呼吸效果开关
- `self.light.set_led_scene` - 设置四色LED场景模式
- `self.light.set_led_colour` - 设置四色LED颜色（HSV格式）
- `self.light.set_laser_mode` - 设置激光灯模式
- `self.light.set_projection_mode` - 设置投影灯模式

#### 查询工具
- `self.air_purifier_or_light.get_status` - 获取完整设备状态，包括净化器所有状态和灯光系统所有状态信息

### 4. 硬件配置
- TXD端口: GPIO 17
- RXD端口: GPIO 18
- 波特率: 9600
- 数据位: 8
- 奇偶校验: 无
- 停止位: 1

### 5. 协议支持的功能

#### 空气净化器功能
- 净化器开关控制 (DPID_PURIFIER_SWITCH)
- 室内PM2.5查询 (DPID_INDOOR_PM25)
- 净化器模式设置 (DPID_PURIFIER_MODE)
- 净化器风速控制 (DPID_PURIFIER_FAN_SPEED)
- 净化器滤网剩余查询 (DPID_PURIFIER_FILTER_LIFE)
- 负离子控制 (DPID_PURIFIER_ANION)
- 净化器童锁控制 (DPID_PURIFIER_CHILD_LOCK)
- 净化器杀菌灯控制 (DPID_PURIFIER_UV)
- 室内温度查询 (DPID_INDOOR_TEMP)
- 室内湿度查询 (DPID_INDOOR_HUMIDITY)
- 定时设置 (DPID_COUNTDOWN_SET)
- 室内空气质量查询 (DPID_INDOOR_AIR_QUALITY)

#### 灯光系统功能
- 四色灯亮度控制 (DPID_LIGHT_BRIGHTNESS)
- 四色灯开关控制 (DPID_LIGHT_LED_SWITCH)
- 四色灯呼吸效果控制 (DPID_LIGHT_LED_BREATH_SWITCH)
- 四色灯场景模式设置 (DPID_LIGHT_LED_SCENE)
- 四色灯颜色设置 (DPID_LIGHT_LED_COLOUR) - HSV格式
- 激光灯模式控制 (DPID_LIGHT_LASER_MODE)
- 投影灯模式控制 (DPID_LIGHT_PROJECTION_MODE)

## 技术特点

### 1. 多帧数据处理
- **MCU一次性上报**: MCU会一次性上报所有DP数据，包含多个连续的数据帧
- **帧分割算法**: 根据帧头(0x55 0xAA)和版本号(0x03)自动分割多个数据帧
- **独立验证**: 每个数据帧独立验证校验和，确保数据完整性
- **错误恢复**: 支持部分帧损坏的情况，继续处理后续有效帧

### 2. 错误处理
- 完整的串口通信错误检测
- 数据帧校验和验证
- 超时处理机制
- 状态初始化检查
- **新增：多帧数据完整性检查**

### 3. 调试支持
- 详细的日志输出
- 数据帧发送/接收状态记录
- 设备状态变化跟踪
- 错误信息记录
- **新增：多帧解析过程日志**

### 4. HSV颜色处理
- **ParseHsvString**: 解析HSV字符串格式 (HHHHSSSSVVVV)
- **FormatHsvString**: 生成HSV字符串格式
- **用户友好的HSV值**: 色调0-360度，饱和度和亮度0-100%
- **协议格式转换**: 自动转换用户值和协议格式

### 5. 内存管理
- 动态内存分配和释放
- 缓冲区大小控制
- 防止内存泄漏
- **vector容器管理多帧数据**

### 6. 线程安全
- 使用FreeRTOS任务
- 串口操作线程安全
- 状态数据保护

## 使用方式

### 1. 自动集成
UART控制器会在主板初始化时自动创建并注册MCP工具，无需额外配置。

### 2. MCP调用示例

#### 净化器控制示例
```json
{
  "tool": "self.air_purifier.set_switch",
  "arguments": {
    "state": true
  }
}
```

#### 灯光控制示例
```json
{
  "tool": "self.light.set_led_colour",
  "arguments": {
    "hue": 220,
    "saturation": 75,
    "value": 78
  }
}
```

## 注意事项

1. **引脚配置**: 使用GPIO 17作为TXD，GPIO 18作为RXD
2. **协议兼容性**: 确保外部MCU支持相同的通信协议
3. **首次使用**: 建议先调用状态查询功能确认通信正常
4. **错误处理**: 所有通信操作都有完整的错误处理机制
5. **调试模式**: 可以通过日志查看详细的通信过程
6. **多帧处理**: MCU会一次性上报所有DP数据，系统会自动分割和处理多个数据帧
7. **HSV颜色设置**: 使用色调(0-360度)、饱和度(0-100%)、亮度(0-100%)的用户友好格式
8. **灯光场景**: 支持17种预设场景模式，从月影到彩虹等丰富效果

## 扩展性

该实现具有良好的扩展性：
- 可以轻松添加新的DP点
- 支持不同的数据类型
- 可以扩展更多的控制功能
- 支持协议版本升级
- **新增：支持任意数量的DP数据帧**
- **新增：支持灯光系统扩展**

## 验证方式

通过MCP工具接口进行功能验证：
- 使用MCP工具测试所有控制功能
- 通过状态查询验证设备响应
- 错误处理机制验证
- 多帧数据解析验证
- HSV颜色转换验证
- 灯光控制功能验证 