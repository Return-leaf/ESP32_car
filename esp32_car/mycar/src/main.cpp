#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

// ==========================================
// 1. 硬件引脚定义
// ==========================================
// 方向控制引脚：对应 L298N/TB6612 驱动板上的 IN1, IN2, IN3, IN4 (两组，共8个引脚)
// 控制电机的正转、反转、停止
const int dirPins[] = {4, 5, 6, 7, 15, 16, 17, 18}; 

// 速度控制引脚 (PWM)：对应驱动板上的 ENA, ENB
// 通过输出 0-255 的脉冲信号来控制电机转速
const int enPins[] = {1, 2, 42, 41}; 

// ==========================================
// 2. WiFi 联网配置
// ==========================================
const char* ssid = "OPPO_car";        // 手机热点名称
const char* password = "1379258046";  // 热点密码

// 创建 WebSocket 服务器实例，监听 81 端口
WebSocketsServer webSocket = WebSocketsServer(81);

/**
 * 紧急停止函数
 * 将所有控制引脚拉低，PWM 输出清零，使电机完全静止
 */
void stopAll() {
  for (int i = 0; i < 8; i++) digitalWrite(dirPins[i], LOW);
  for (int i = 0; i < 4; i++) analogWrite(enPins[i], 0);
}

/**
 * 核心驱动逻辑：控制运动方向与速度
 * @param dir   方向字符串 (来自小程序指令)
 * @param speed 速度值 (0-255)
 */
void drive(String dir, int speed) {
  if (dir == "forward") {
    // 【前进】：左侧正转，右侧正转
    digitalWrite(4, HIGH); digitalWrite(5, LOW); digitalWrite(6, HIGH); digitalWrite(7, LOW);
    digitalWrite(15, HIGH); digitalWrite(16, LOW); digitalWrite(17, HIGH); digitalWrite(18, LOW);
  } 
  else if (dir == "backward") {
    // 【后退】：左侧反转，右侧反转
    digitalWrite(4, LOW); digitalWrite(5, HIGH); digitalWrite(6, LOW); digitalWrite(7, HIGH);
    digitalWrite(15, LOW); digitalWrite(16, HIGH); digitalWrite(17, LOW); digitalWrite(18, HIGH);
  } 
  else if (dir == "left") {
    // 【普通左转】：左侧停止，右侧正转 (划弧转弯)
    digitalWrite(4, LOW); digitalWrite(5, LOW); digitalWrite(6, LOW); digitalWrite(7, LOW);
    digitalWrite(15, HIGH); digitalWrite(16, LOW); digitalWrite(17, HIGH); digitalWrite(18, LOW);
  } 
  else if (dir == "right") {
    // 【普通右转】：左侧正转，右侧停止 (划弧转弯)
    digitalWrite(4, HIGH); digitalWrite(5, LOW); digitalWrite(6, HIGH); digitalWrite(7, LOW);
    digitalWrite(15, LOW); digitalWrite(16, LOW); digitalWrite(17, LOW); digitalWrite(18, LOW);
  } 
  else if (dir == "drift_l") {
    // 【左漂移】：左侧电机反转，右侧电机满速正转 (原地坦克转向，产生甩尾)
    digitalWrite(4, LOW); digitalWrite(5, HIGH); digitalWrite(6, LOW); digitalWrite(7, HIGH); 
    digitalWrite(15, HIGH); digitalWrite(16, LOW); digitalWrite(17, HIGH); digitalWrite(18, LOW);
    speed = 255; // 漂移强制爆发满功耗
  } 
  else if (dir == "drift_r") {
    // 【右漂移】：左侧电机满速正转，右侧电机反转 (原地坦克转向，产生甩尾)
    digitalWrite(4, HIGH); digitalWrite(5, LOW); digitalWrite(6, HIGH); digitalWrite(7, LOW);
    digitalWrite(15, LOW); digitalWrite(16, HIGH); digitalWrite(17, LOW); digitalWrite(18, HIGH);
    speed = 255; // 漂移强制爆发满功耗
  } 
  else {
    // 指令未知或为 "stop" 时停止
    stopAll();
    return;
  }

  // 将计算好的速度应用到 4 个使能引脚 (PWM 输出)
  for (int i = 0; i < 4; i++) analogWrite(enPins[i], speed);
}

/**
 * WebSocket 事件监听
 * 负责接收小程序通过网络发来的文本指令
 */
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    // 将收到的原始数据转换为字符串 (格式如 "forward:180")
    String input = (char*)payload;
    // 查找冒号位置，用于分割方向和速度
    int colonIdx = input.indexOf(':');
    if (colonIdx != -1) {
      String cmd = input.substring(0, colonIdx);             // 提取指令: "forward"
      int speed = input.substring(colonIdx + 1).toInt();     // 提取速度: 180
      drive(cmd, speed); // 执行电机控制
    }
  } else if (type == WStype_DISCONNECTED) {
    // 如果 WebSocket 断开连接 (比如手机切后台或信号丢失)，立即停车，防止撞墙
    stopAll();
  }
}

// ==========================================
// 3. 系统初始化
// ==========================================
void setup() {
  // 初始化串口，用于电脑调试查看 IP
  Serial.begin(115200);

  // 设置所有控制引脚为输出模式
  for (int i = 0; i < 8; i++) pinMode(dirPins[i], OUTPUT);
  for (int i = 0; i < 4; i++) pinMode(enPins[i], OUTPUT);
  
  // 上电默认停止，防止乱跑
  stopAll();

  // 开始连接手机热点 (STA 模式)
  WiFi.begin(ssid, password);
  Serial.print("正在连接到热点: ");
  Serial.println(ssid);

  // 等待连接成功
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // 打印成功信息，非常关键：需要把这个 IP 填写到小程序 index.js 中
  Serial.println("\nWiFi 已连接！");
  Serial.print("小车 IP 地址: ");
  Serial.println(WiFi.localIP()); 

  // 启动服务器
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

// ==========================================
// 4. 主循环
// ==========================================
void loop() {
  // 保持 WebSocket 服务运行，处理实时消息
  webSocket.loop();
}