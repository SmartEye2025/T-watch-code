#define LILYGO_WATCH_2020_V1 
#include <LilyGoWatch.h>
#include <WiFi.h>
#include <PubSubClient.h>

// 配置项
const char* ssid = "SCUNOT";
const char* password = "1346286832LYS";
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char* topic = "classroom/seat/alert";

// 震动电机设置
const int VIBRATOR_PIN = 4;
const int PWM_CHANNEL = 0;
const int PWM_FREQUENCY = 1000;
const int PWM_RESOLUTION = 8;

// 关机按键设置
const int SHUTDOWN_BTN_PIN = 35;  // AXP202_INT 引脚
const long SHUTDOWN_HOLD_TIME = 1500; // 长按1.5秒关机

WiFiClient espClient;
PubSubClient client(espClient);

TTGOClass *ttgo;
TFT_eSPI *tft;

// 关机状态标志
bool isShuttingDown = false;
unsigned long shutdownPressStart = 0;

// 震动模式函数（优化版）
void vibratePattern() {
    // 短震（50%强度）
    ledcWrite(PWM_CHANNEL, 12);
    delay(200);
    ledcWrite(PWM_CHANNEL, 0);
    delay(300);
    
    // 长震（100%强度）
    ledcWrite(PWM_CHANNEL, 25);
    delay(200);
    ledcWrite(PWM_CHANNEL, 0);
}

// 关机函数
void shutdownDevice() {
    Serial.println("Shutting down...");
    
    // 关闭震动
    ledcWrite(PWM_CHANNEL, 0);
    
    // 断开网络连接
    client.disconnect();
    WiFi.disconnect();
    
    // 显示关机画面
    ttgo->openBL();
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(2);
    tft->setCursor(50, 110);
    tft->print("Power Off");
    delay(1000);
    
    // 关闭背光
    ttgo->closeBL();
    
    // 关闭外设电源
    ttgo->power->setPowerOutPut(AXP202_LDO2, AXP202_OFF); // 震动电机
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF); // 显示屏
    
    // 深度睡眠或关机
    ttgo->power->shutdown();
    
    // 如果关机失败，进入死循环
    while (1) {
        delay(1000);
    }
}

// MQTT消息回调
void callback(char* topic, byte* payload, unsigned int length) {
    // 如果正在关机，忽略所有消息
    if (isShuttingDown) return;
    
    String message = "";
    for (int i = 0; i < length; i++) message += (char)payload[i];
    
    if (message.indexOf("leave") != -1) {
        // 打开屏幕背光
        ttgo->openBL();
        
        // 清屏并显示感叹号
        tft->fillScreen(TFT_BLACK);
        tft->setTextColor(TFT_RED, TFT_BLACK);
        tft->setTextSize(5);
        tft->setCursor(60, 100); // 更居中的位置
        tft->print("!!!!!!!");   // 7个感叹号
        
        // 触发震动提醒
        vibratePattern();
        Serial.println("Alert triggered!");
        
        // 保持显示3秒
        delay(3000);
        
        // 关闭屏幕背光
        ttgo->closeBL();
    }
}

void setup() {
    Serial.begin(115200);
    
    // PWM初始化
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(VIBRATOR_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0); // 确保震动关闭
    delay(100); // 确保设置生效
    
    // 初始化手表
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    
    // 配置关机按键（AXP202_INT）
    pinMode(SHUTDOWN_BTN_PIN, INPUT_PULLUP);
    
    // 启用必要的电源输出
    ttgo->power->setPowerOutPut(AXP202_LDO2, AXP202_ON); // 震动电机电源
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_ON); // 显示屏电源
    
    // 显示初始化（只显示连接进度）
    ttgo->openBL(); // 开启背光
    tft = ttgo->tft;
    tft->fillScreen(TFT_BLACK);
    tft->setTextFont(2);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setCursor(0, 50);
    tft->print("正在连接网络...");
    
    // WiFi连接
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        tft->print(".");
    }
    
    // MQTT连接
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    
    tft->fillRect(0, 50, 240, 30, TFT_BLACK); // 清空连接进度
    tft->setCursor(0, 50);
    tft->print("正在连接MQTT...");
    
    while (!client.connect("twatch2020")) {
        delay(500);
        tft->print(".");
    }
    client.subscribe(topic);
    
    // 显示连接成功信息（短暂显示）
    tft->fillScreen(TFT_BLACK);
    tft->setCursor(0, 100);
    tft->print("连接成功!");
    tft->setCursor(0, 120);
    tft->print("准备接收提醒...");
    
    // 2秒后关闭背光
    delay(2000);
    ttgo->closeBL();
    
    Serial.println("Setup completed. Ready for alerts.");
}

void loop() {
    // 处理关机按键
    if (digitalRead(SHUTDOWN_BTN_PIN) == LOW) { // 按键按下（低电平触发）
        if (shutdownPressStart == 0) {
            shutdownPressStart = millis(); // 记录按下开始时间
        } else if (!isShuttingDown && millis() - shutdownPressStart >= SHUTDOWN_HOLD_TIME) {
            isShuttingDown = true;
            shutdownDevice(); // 执行关机
        }
    } else {
        shutdownPressStart = 0; // 按键释放
    }
    
    // 确保震动状态关闭（双重保险）
    static unsigned long lastCheck = millis();
    if (millis() - lastCheck > 1000) {
        ledcWrite(PWM_CHANNEL, 0); // 确保震动关闭
        lastCheck = millis();
    }
    
    // 连接管理 (如果正在关机则跳过)
    if (!isShuttingDown) {
        if (!client.connected()) {
            if (client.connect("twatch2020")) {
                client.subscribe(topic);
            }
        }
        client.loop();
    }
    
    delay(10);
}