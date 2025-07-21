#define LILYGO_WATCH_2020_V1
// #define INTENSITY 20
#include <LilyGoWatch.h>
#include <WiFi.h>
#include <PubSubClient.h>

// 配置项
const char *ssid = "SCUNOT";
const char *password = "1346286832LYS";
const char *mqttServer = "43.138.252.29";
const int mqttPort = 1883;
const char *topic = "remind/1/vibrate";

// 震动电机设置
const int VIBRATOR_PIN = 4;
const int PWM_CHANNEL = 0;
const int PWM_FREQUENCY = 1000;
const int PWM_RESOLUTION = 8;
bool isVibrating = false;
bool shouldStopVibration = false;

// 关机按键设置
const int POWER_BUTTON_PIN = 35;          // AXP202_INT 引脚
const long POWER_BUTTON_HOLD_TIME = 1500; // 长按1.5秒关机
// int intensity = INTENSITY;
int intensity = 0;
int time_count = 1;
// 关机状态变量
bool isShuttingDown = false;
unsigned long powerButtonPressStart = 0;

WiFiClient espClient;
PubSubClient client(espClient);

TTGOClass *ttgo;
TFT_eSPI *tft;

// 震动模式函数
void vibratePattern1()
{

    ledcWrite(PWM_CHANNEL, 100);
    delay(500);
    ledcWrite(PWM_CHANNEL, 0);
    delay(500);

    ledcWrite(PWM_CHANNEL, 100);
    delay(500);
    ledcWrite(PWM_CHANNEL, 0);
    delay(500);

    ledcWrite(PWM_CHANNEL, 100);
    delay(500);
    ledcWrite(PWM_CHANNEL, 0);
    delay(500);

    ledcWrite(PWM_CHANNEL, 100);
    delay(500);
    ledcWrite(PWM_CHANNEL, 0);
    delay(500);
}

void vibratePattern2()
{
    unsigned long startTime = millis();
    unsigned long lastIncreaseTime = startTime;
    unsigned long lastMQTTCheck = startTime;

    // 设置震动状态
    isVibrating = true;
    shouldStopVibration = false;

    // 确保屏幕一直亮着
    ttgo->openBL();
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_RED, TFT_BLACK);
    tft->setTextSize(5);
    tft->setCursor(75, 100);
    tft->print("!!!!!!!");
    tft->setTextSize(2);
    // tft->setCursor(60, 150);
    // tft->print("Level: ");
    // tft->print(intensity);

    // 无限循环，持续震动
    while (!shouldStopVibration)
    {
        // 当前震动强度根据intensity级别确定
        int currentIntensity = 20;
        if (intensity == 1)
        {
            currentIntensity = 20;
        }
        else if (intensity == 2)
        {
            currentIntensity = 100;
        }
        else if (intensity >= 3)
        {
            currentIntensity = 200;
        }

        // 执行震动 - 每个循环都会震动
        ledcWrite(PWM_CHANNEL, currentIntensity);
        delay(time_count * 400);
        ledcWrite(PWM_CHANNEL, 0);
        delay((8000 - (time_count * 4)) / 3); // 4个震动,3个暂停

        // 每10秒增加震动强度和时长
        if (millis() - lastIncreaseTime >= 10000)
        {
            intensity++;
            time_count = (time_count >= 3) ? 3 : time_count + 1;
            lastIncreaseTime = millis();

            // 刷新显示
            tft->fillScreen(TFT_BLACK);
            tft->setTextColor(TFT_RED, TFT_BLACK);
            tft->setTextSize(5);
            tft->setCursor(75, 100);
            tft->print("!!!!!!!");

            // 显示当前强度
            // tft->setTextSize(2);
            // tft->setCursor(60, 150);
            // tft->print("Level: ");
            // tft->print(intensity);
        }

        if (millis() - lastMQTTCheck >= 1000)
        {
            client.loop();
            lastMQTTCheck = millis();
        }
    }

    // 震动结束后清理
    isVibrating = false;
    ledcWrite(PWM_CHANNEL, 0);
    ttgo->closeBL();
}

// 停止震动函数
void stopVibration()
{
    shouldStopVibration = true;
    isVibrating = false;
    intensity = 0;
    time_count = 1;
    ledcWrite(PWM_CHANNEL, 0);
    ttgo->closeBL();
}

// 关机函数
void shutdownDevice()
{
    Serial.println("Shutting down...");

    // 关闭震动
    ledcWrite(PWM_CHANNEL, 0);

    // 断开网络连接
    client.disconnect();
    WiFi.disconnect();

    // 显示关机画面
    ttgo->openBL();
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    ttgo->tft->setTextSize(2);
    ttgo->tft->setCursor(50, 110);
    ttgo->tft->print("Power Off");
    delay(1000);

    // 关闭背光
    ttgo->closeBL();

    // 关闭外设电源
    ttgo->power->setPowerOutPut(AXP202_LDO2, AXP202_OFF); // 震动电机
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF); // 显示屏

    // 深度睡眠或关机
    ttgo->power->shutdown();

    // 如果关机失败，进入死循环
    while (1)
    {
        delay(1000);
    }
}

// MQTT消息回调
void callback(char *topic, byte *payload, unsigned int length)
{
    String message = "";
    for (int i = 0; i < length; i++)
        message += (char)payload[i];

    if (message.indexOf("type1") != -1)

    { // 监控检测到的,循环震动直到接收到停止消息
        // 打开屏幕背光
        ttgo->openBL();

        // 清屏并显示感叹号
        tft->fillScreen(TFT_BLACK);
        tft->setTextColor(TFT_RED, TFT_BLACK);
        tft->setTextSize(5);
        tft->setCursor(75, 100); // 居中的位置
        tft->print("!!!!!!!");   // 7个感叹号

        // 触发震动提醒
        vibratePattern2();
    }
    else if (message.indexOf("type2") != -1)
    { // 教师的提醒,短暂震动
        // 打开屏幕背光
        ttgo->openBL();

        // 清屏并显示感叹号
        tft->fillScreen(TFT_BLACK);
        tft->setTextColor(TFT_RED, TFT_BLACK);
        tft->setTextSize(5);
        tft->setCursor(75, 100); // 更居中的位置
        tft->print("!!!!!!!");   // 7个感叹号

        // 触发震动提醒
        vibratePattern1();

        // 保持显示3秒
        delay(3000);

        // 关闭屏幕背光
        ttgo->closeBL();
    }
    else if (message.indexOf("type3") != -1)
    {
        stopVibration();
    }
}

void setup()
{
    Serial.begin(115200);

    // PWM初始化
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(VIBRATOR_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0); // 确保震动关闭
    delay(100);                // 确保设置生效

    // 初始化手表
    ttgo = TTGOClass::getWatch();
    ttgo->begin();

    // 启用必要的电源输出
    // ttgo->power->setPowerOutPut(AXP202_LDO2, AXP202_ON); // 震动电机电源
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_ON); // 显示屏电源

    // 显示初始化（只显示连接进度）
    ttgo->openBL(); // 开启背光
    tft = ttgo->tft;
    tft->fillScreen(TFT_BLACK);
    tft->setTextFont(2);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setCursor(0, 50);
    tft->print("connecting wifi...");
    delay(1000);

    // WiFi连接
    WiFi.begin(ssid, password);
    ledcWrite(PWM_CHANNEL, 0); //// 确保震动关闭

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        tft->print(".");
    }

    ttgo->power->setPowerOutPut(AXP202_LDO2, AXP202_ON);
    // MQTT连接
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);

    tft->fillRect(0, 50, 240, 30, TFT_BLACK); // 清空连接进度
    tft->setCursor(0, 50);
    tft->print("connecting MQTT...");

    while (!client.connect("twatch2020"))
    {
        delay(500);
        tft->print(".");
    }
    client.subscribe(topic);

    // 显示连接成功信息（短暂显示）
    tft->fillScreen(TFT_BLACK);
    tft->setCursor(0, 100);
    tft->print("connected!");
    tft->setCursor(0, 120);
    tft->print("ready to receive alerts...");

    // 2秒后关闭背光
    delay(3000);
    ttgo->closeBL();

    // 配置电源按键中断引脚
    pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
}

void loop()
{
    // 确保震动状态关闭（双重保险）
    static unsigned long lastCheck = millis();
    if (millis() - lastCheck > 1000)
    {
        ledcWrite(PWM_CHANNEL, 0); // 确保震动关闭
        lastCheck = millis();
    }

    // 处理电源按键
    static bool previousButtonState = HIGH;
    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 50; // 防抖延迟

    // 读取当前按钮状态
    bool currentReading = digitalRead(POWER_BUTTON_PIN);

    // 如果状态变化，重置防抖计时器
    if (currentReading != previousButtonState)
    {
        lastDebounceTime = millis();
    }
    previousButtonState = currentReading;

    // 如果状态稳定超过防抖时间
    if ((millis() - lastDebounceTime) > debounceDelay)
    {
        // 如果按钮被按下（低电平）
        if (currentReading == LOW)
        {
            if (powerButtonPressStart == 0)
            {
                powerButtonPressStart = millis(); // 记录按下开始时间
                Serial.println("Power button pressed");
            }
            else if (!isShuttingDown && millis() - powerButtonPressStart >= POWER_BUTTON_HOLD_TIME)
            {
                isShuttingDown = true;
                Serial.println("Shutdown triggered");
                shutdownDevice(); // 执行关机
            }
        }
        else
        {
            powerButtonPressStart = 0; // 按钮释放
        }
    }

    // 连接管理
    if (!client.connected())
    {
        if (client.connect("twatch2020"))
        {
            client.subscribe(topic);
        }
    }
    client.loop();

    delay(10);
}