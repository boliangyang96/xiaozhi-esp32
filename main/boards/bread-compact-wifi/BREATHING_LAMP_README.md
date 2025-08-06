# 呼吸灯控制器使用说明

## 功能概述

呼吸灯控制器（BreathingLampController）为ESP32设备提供了智能的LED灯光控制功能，包括基础开关控制和呼吸灯模式。

## 硬件要求

- ESP32开发板
- LED灯（连接到GPIO 19，可通过config.h中的LAMP_GPIO配置）

## 功能特性

### 1. 基础灯光控制
- **开关控制**：支持基本的开灯和关灯功能
- **状态查询**：可以查询当前灯光状态

### 2. 呼吸灯模式
- **自定义时序**：可以设置开启时间和关闭时间
- **持续循环**：自动重复开启/关闭循环
- **实时控制**：可以随时启动或停止呼吸模式

## MCP工具接口

### 基础控制工具

#### `self.lamp.get_state`
获取灯光当前状态
- **参数**：无
- **返回**：JSON格式的状态信息 `{"power": true/false}`

#### `self.lamp.turn_on`
打开灯光
- **参数**：无
- **返回**：`true`（成功）

#### `self.lamp.turn_off`
关闭灯光
- **参数**：无
- **返回**：`true`（成功）

### 呼吸灯控制工具

#### `self.lamp.start_breathing`
启动呼吸灯模式
- **参数**：
  - `on_duration`：开启持续时间（秒），范围1-60，默认3
  - `off_duration`：关闭持续时间（秒），范围1-60，默认4
- **返回**：`true`（成功）

#### `self.lamp.stop_breathing`
停止呼吸灯模式
- **参数**：无
- **返回**：`true`（成功）

#### `self.lamp.get_breathing_state`
获取呼吸灯模式状态
- **参数**：无
- **返回**：JSON格式的呼吸状态 `{"breathing": true/false}`

## 使用示例

### 基础使用
```json
// 打开灯光
{
  "name": "self.lamp.turn_on",
  "arguments": {}
}

// 关闭灯光
{
  "name": "self.lamp.turn_off",
  "arguments": {}
}

// 查询状态
{
  "name": "self.lamp.get_state",
  "arguments": {}
}
```

### 呼吸灯使用
```json
// 启动呼吸灯模式：开启3秒，关闭4秒
{
  "name": "self.lamp.start_breathing",
  "arguments": {
    "on_duration": 3,
    "off_duration": 4
  }
}

// 停止呼吸灯模式
{
  "name": "self.lamp.stop_breathing",
  "arguments": {}
}

// 查询呼吸灯状态
{
  "name": "self.lamp.get_breathing_state",
  "arguments": {}
}
```

## 技术实现

### 线程安全
- 使用`std::atomic<bool>`确保线程安全的状态控制
- 使用`std::thread`实现异步呼吸灯控制
- 支持优雅的线程停止和资源清理

### GPIO控制
- 使用ESP32的GPIO API进行硬件控制
- 支持高电平/低电平控制LED
- 自动配置GPIO引脚为输出模式

### 参数验证
- 开启/关闭时间范围限制：1-60秒
- 自动参数验证和错误处理
- 防止无效参数导致的异常

## 注意事项

1. **GPIO配置**：确保LED正确连接到配置的GPIO引脚（默认GPIO 19）
2. **电源管理**：呼吸灯模式会持续消耗电力，建议在不需要时停止
3. **线程安全**：多个工具调用会安全地处理，不会产生冲突
4. **资源清理**：控制器析构时会自动清理线程资源

## 配置文件

在`config.h`中可以修改LED的GPIO配置：
```c
#define LAMP_GPIO GPIO_NUM_19
``` 