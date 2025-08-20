# 涂鸦串口通信协议更新总结

## 更新概述

本次更新将原有的空气净化器控制系统升级为AI桌面空净星空灯控制系统，新增了完整的灯光控制功能，包括四色灯、激光灯和投影灯的控制。

## 主要变化

### 1. 设备状态结构更新

#### 变量重命名（按新协议标识名）
- `switch_state` → `purifier_switch`
- `pm25` → `indoor_pm25`
- `mode` → `purifier_mode`
- `fan_speed` → `purifier_fan_speed`
- `filter_life` → `purifier_filter_life`
- `child_lock` → `purifier_child_lock`
- `uv_light` → `purifier_uv`
- `air_quality` → `indoor_air_quality`

#### 新增变量
- `purifier_anion` - 负离子功能状态
- `light_brightness` - 四色灯亮度
- `light_led_switch` - 四色LED灯开关
- `light_led_breath_switch` - 四色LED呼吸效果开关
- `light_led_scene` - 四色LED场景模式
- `led_hue` - LED色调 (0-360度)
- `led_saturation` - LED饱和度 (0-100%)
- `led_value` - LED亮度值 (0-100%)
- `light_laser_mode` - 激光灯模式
- `light_projection_mode` - 投影灯模式

### 2. DP ID定义更新

#### 重命名的DP ID
- `DPID_SWITCH` → `DPID_PURIFIER_SWITCH`
- `DPID_PM25` → `DPID_INDOOR_PM25`
- `DPID_MODE` → `DPID_PURIFIER_MODE`
- `DPID_FAN_SPEED` → `DPID_PURIFIER_FAN_SPEED`
- `DPID_FILTER_LIFE` → `DPID_PURIFIER_FILTER_LIFE`
- `DPID_CHILD_LOCK` → `DPID_PURIFIER_CHILD_LOCK`
- `DPID_UV` → `DPID_PURIFIER_UV`
- `DPID_TEMP_INDOOR` → `DPID_INDOOR_TEMP`
- `DPID_HUMIDITY` → `DPID_INDOOR_HUMIDITY`
- `DPID_AIR_QUALITY` → `DPID_INDOOR_AIR_QUALITY`

#### 新增的DP ID
- `DPID_PURIFIER_ANION` (0x06) - 负离子控制
- `DPID_LIGHT_BRIGHTNESS` (0x65) - 四色灯亮度
- `DPID_LIGHT_LED_SWITCH` (0x66) - 四色LED开关
- `DPID_LIGHT_LED_BREATH_SWITCH` (0x67) - 四色LED呼吸开关
- `DPID_LIGHT_LED_SCENE` (0x68) - 四色LED场景
- `DPID_LIGHT_LED_COLOUR` (0x69) - 四色LED颜色
- `DPID_LIGHT_LASER_MODE` (0x6A) - 激光灯模式
- `DPID_LIGHT_PROJECTION_MODE` (0x6B) - 投影灯模式

### 3. 新增功能实现

#### HSV颜色处理系统
- `ParseHsvString()` - 解析HSV字符串 (格式: HHHHSSSSVVVV)
- `FormatHsvString()` - 生成HSV字符串
- 用户友好的HSV值：色调0-360度，饱和度和亮度0-100%
- 自动转换协议格式和用户格式

#### 灯光控制方法
- `SetLightBrightness()` - 设置四色灯亮度
- `SetLedSwitch()` - 控制四色LED开关
- `SetLedBreathSwitch()` - 控制呼吸效果
- `SetLedScene()` - 设置灯光场景
- `SetLedColour()` - 设置LED颜色（HSV格式）
- `SetLaserMode()` - 设置激光灯模式
- `SetProjectionMode()` - 设置投影灯模式
- `SetAnion()` - 控制负离子功能

### 4. MCP工具接口扩展

#### 新增灯光控制工具
- `self.light.set_brightness` - 设置四色灯亮度 (1-100)
- `self.light.set_led_switch` - 四色LED灯开关控制
- `self.light.set_led_breath_switch` - 四色LED呼吸效果控制
- `self.light.set_led_scene` - 四色LED场景模式设置 (0-16)
- `self.light.set_led_colour` - 四色LED颜色设置（HSV参数）
- `self.light.set_laser_mode` - 激光灯模式控制 (0-2)
- `self.light.set_projection_mode` - 投影灯模式控制 (0-2)

#### 新增净化器控制工具
- `self.air_purifier.set_anion` - 负离子功能控制

#### 更新的查询工具
- `self.air_purifier_or_light.get_status` - 现在返回完整的净化器和灯光系统状态

### 5. 灯光场景支持

支持17种预设灯光场景：
- 0: 月影 (moon_shadow)
- 1: 极光 (aurora)
- 2: 黄昏 (dusk)
- 3: 深蓝 (deep_blue)
- 4: 森林 (forest)
- 5: 篝火 (bonfire)
- 6: 黎明 (early_dawn)
- 7: 星空 (starry_sky)
- 8: 日落 (sunset)
- 9: 寺庙烛光 (temple_candle)
- 10: 水墨 (ink_wash)
- 11: 赛博朋克 (cyberpunk)
- 12: 浪漫 (romance)
- 13: 治愈 (healing)
- 14: 专注 (focus)
- 15: 彩虹 (rainbow)
- 16: 自定义 (custom)

## 使用示例

### 设置LED颜色（蓝色）
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

### 设置灯光场景（星空）
```json
{
  "tool": "self.light.set_led_scene",
  "arguments": {
    "scene": 7
  }
}
```

### 设置激光灯呼吸模式
```json
{
  "tool": "self.light.set_laser_mode",
  "arguments": {
    "mode": 1
  }
}
```

### 启用负离子功能
```json
{
  "tool": "self.air_purifier.set_anion",
  "arguments": {
    "state": true
  }
}
```

## 技术特点

1. **向后兼容**: 保持原有净化器控制功能不变
2. **用户友好**: HSV颜色使用直观的度数和百分比
3. **扩展性强**: 易于添加新的灯光效果和控制功能
4. **完整集成**: 所有功能通过统一的MCP接口访问
5. **错误处理**: 完整的异常处理和状态验证

## 协议兼容性

- 保持原有的涂鸦协议帧格式不变
- 新增字符串数据类型支持（HSV颜色）
- 支持更多DP ID范围（扩展到0x6B）
- 多帧数据处理保持不变

## 文档更新

- `README_UART.md` - 完整更新使用说明
- `IMPLEMENTATION_SUMMARY.md` - 更新实现总结
- `PROTOCOL_UPDATE_SUMMARY.md` - 新增协议更新总结

所有更新已完成并经过测试，代码无编译错误，可以正常使用。
