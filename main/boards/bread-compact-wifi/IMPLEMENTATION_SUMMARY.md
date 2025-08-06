# UART串口通信MCP实现总结

## 实现概述

本次实现完成了通过MCP协议实现UART串口通信的功能，用于与外部MCU进行通信控制空气净化器设备。

## 文件结构

### 核心文件
- `uart_controller.h` - UART控制器头文件
- `uart_controller.cc` - UART控制器实现文件
- `uart_test.h` - UART测试头文件
- `uart_test.cc` - UART测试实现文件
- `README_UART.md` - 使用说明文档

### 修改文件
- `compact_wifi_board.cc` - 主板文件，集成了UART控制器

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

#### 控制工具
- `self.air_purifier.set_switch` - 控制开关
- `self.air_purifier.set_mode` - 设置模式
- `self.air_purifier.set_fan_speed` - 设置风速
- `self.air_purifier.set_child_lock` - 设置童锁
- `self.air_purifier.set_uv_light` - 设置杀菌灯
- `self.air_purifier.set_countdown` - 设置定时

#### 查询工具
- `self.air_purifier.get_status` - 获取完整设备状态
- `self.air_purifier.get_pm25` - 查询PM2.5
- `self.air_purifier.get_temperature` - 查询温度
- `self.air_purifier.get_humidity` - 查询湿度
- `self.air_purifier.get_air_quality` - 查询空气质量
- `self.air_purifier.get_filter_life` - 查询滤网寿命

### 4. 硬件配置
- TXD端口: GPIO 17
- RXD端口: GPIO 18
- 波特率: 9600
- 数据位: 8
- 奇偶校验: 无
- 停止位: 1

### 5. 协议支持的功能
- 开关控制 (DPID_SWITCH)
- PM2.5查询 (DPID_PM25)
- 模式设置 (DPID_MODE)
- 风速控制 (DPID_FAN_SPEED)
- 滤网剩余查询 (DPID_FILTER_LIFE)
- 童锁控制 (DPID_CHILD_LOCK)
- 杀菌灯控制 (DPID_UV)
- 室内温度查询 (DPID_TEMP_INDOOR)
- 室内湿度查询 (DPID_HUMIDITY)
- 定时设置 (DPID_COUNTDOWN_SET)
- 空气质量查询 (DPID_AIR_QUALITY)

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

### 4. 内存管理
- 动态内存分配和释放
- 缓冲区大小控制
- 防止内存泄漏
- **新增：vector容器管理多帧数据**

### 5. 线程安全
- 使用FreeRTOS任务
- 串口操作线程安全
- 状态数据保护

## 使用方式

### 1. 自动集成
UART控制器会在主板初始化时自动创建并注册MCP工具，无需额外配置。

### 2. 手动测试
可以通过取消注释以下代码来启动测试任务：
```cpp
// start_uart_test();
```

### 3. MCP调用示例
```json
{
  "tool": "self.air_purifier.set_switch",
  "arguments": {
    "state": true
  }
}
```

## 注意事项

1. **引脚配置**: 使用GPIO 17作为TXD，GPIO 18作为RXD，LAMP_GPIO使用GPIO 19
2. **协议兼容性**: 确保外部MCU支持相同的通信协议
3. **首次使用**: 建议先调用状态查询功能确认通信正常
4. **错误处理**: 所有通信操作都有完整的错误处理机制
5. **调试模式**: 可以通过日志查看详细的通信过程
6. **多帧处理**: MCU会一次性上报所有DP数据，系统会自动分割和处理多个数据帧

## 扩展性

该实现具有良好的扩展性：
- 可以轻松添加新的DP点
- 支持不同的数据类型
- 可以扩展更多的控制功能
- 支持协议版本升级
- **新增：支持任意数量的DP数据帧**

## 测试验证

提供了完整的测试框架：
- 自动测试所有控制功能
- 验证状态查询功能
- 测试错误处理机制
- 性能测试和稳定性验证
- **新增：多帧数据解析测试**
- **新增：模拟MCU上报数据测试** 