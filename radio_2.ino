#include "lgfx.h"
#include "key.h"
#include "AudioBoard.h" 
#include <Audio.h>
#include <HTTPClient.h>
#include <OneButton.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <Preferences.h>
#include <WiFi.h>
#include <map>
#include <Ticker.h>
#include "driver/i2s.h"
#include "re.h"

Audio audio;
DriverPins my_pins;
AudioBoard board(AudioDriverES8311, my_pins); 
CodecConfig cfg;
//ES8311 I2S
#define I2S_MCK_IO 5
#define I2S_BCK_IO 6
#define I2S_DI_IO 17
#define I2S_WS_IO 4
#define I2S_DO_IO 15
#define ES8311_PA 3  //ES8311使能

// ES8311 I2C 
#define I2C_SDA 41
#define I2C_SCL 42
#define ES8311_ADDRESS 0x18

#define PIN_KEY_MODE 0
#define PIN_KEY_SET 16

#define PIN_ADC 2

using namespace std;

#define FONT16 &fonts::efontCN_16
#define FM_URL "http://lhttp.qtfm.cn/live/%d/64k.mp3"

typedef struct {
  u32_t id;
  String name;
} RadioItem;

static const char *WEEK_DAYS[] = {"日", "一", "二", "三", "四", "五", "六"};

Ticker timer;
LGFX tft;
LGFX_Sprite sp(&tft);
long check10ms = 0, check300ms = 0, check60s = 0, check5s = 0;
char buf[128] = {0};

int curIndex = 0;
int curVolume = 40;
extern unsigned long ms2;
extern ButtonADC btn_volup;
extern ButtonADC btn_voldn;
extern EventQueue eventQueue;
int adc_val =0;
int lunxun=0;
bool power_button=false;
bool off=false;
extern unsigned char data[44350];

/*WIFI*/
int network = 0;
typedef struct {
  String ssid;
  String passwd;
}WIFIMES;
static WIFIMES wifi_list[10];
/*WIFI*/

std::map<u32_t, OneButton *> buttons;
std::vector<RadioItem> radios = {
    {4915, "清晨音乐台"},
    {1223, "怀旧好声音"},
    {4866, "浙江音乐调频"},
    {20211686, "成都年代音乐怀旧好声音"},
    {1739, "厦门音乐广播"},
    {1271, "深圳飞扬971"},
    {20240, "山东经典音乐广播"},
    {20500066, "年代音乐1022"},
    {1296, "湖北经典音乐广播"},
    {267, "上海经典947"},
    {20212426, "崂山921"},
    {20003, "天津TIKI FM100.5"},
    {1111, "四川城市之音"},
    {4936, "江苏音乐广播PlayFM897"},
    {4237, "长沙FM101.7城市之声"},
    {1665, "山东音乐广播"},
    {1947, "安徽音乐广播"},
    {332, "北京音乐广播"},
    {4932, "山西音乐广播"},
    {20500149, "两广之声音乐台"},
    {4804, "怀集音乐之声"},
    {1649, "河北音乐广播"},
    {4938, "江苏经典流行音乐"},
    {1260, "广东音乐之声"},
    {273, "上海流行音乐LoveRadio"},
    {274, "上海动感101"},
    {2803, "苏州音乐广播"},
    {839, "哈尔滨音乐广播"},
    {5021381, "959年代音乐怀旧好声音"},
    {15318569, "AsiaFM 亚洲粤语台"},
    {5022308, "500首华语经典"},
    {20500150, "顺德音乐之声"},
    {4875, "FM950广西音乐台"},
    {1283, "江门旅游之声"},
    {1936, "FM954汽车音乐广播"},
    {20847, "FM88.6长沙音乐广播"},
    {1612, "西安音乐广播"},
    {20210755, "星河音乐"},
    {1886, "内蒙古音乐之声"},
    {1208, "河南音乐广播"},
    {4963, "南京音乐广播"},
    {1802, "江西音乐广播"},
    {15318146, "杭州FM90.7"},
    {647, "重庆音乐广播"},
    {15318703, "欧美音乐88.7"},
    {5021523, "惠州音乐广播"},
    {15318341, "AsiaFM HD音乐台"},
    {20769, "南宁经典1049"},
    {1289, "楚天音乐广播"},
    {4873, "陕西音乐广播"},
    {5022474, "武安融媒综合广播"},
    {21209, "东莞音乐广播"},
    {4969, "黑龙江音乐广播"},
    {1136, "嘉兴音乐广播"},
    {21275, "南通音乐广播"},
    {20211619, "怀旧音乐广播895"},
    {4981, "芒果时空音乐台"},
    {1297, "武汉经典音乐广播"},
    {20211638, "定州交通音乐广播"},
    {5022023, "上海KFM981"},
    {20207761, "80后音悦台"},
    {1654, "石家庄音乐广播"},
    {20212227, "经典FM1008"},
    {1149, "1003温州音乐之声"},
    {1671, "济南音乐广播FM88.7"},
    {5021912, "AsiaFM 亚洲经典台"},
    {1084, "大连1067"},
    {1892, "包头汽车音乐广播"},
    {1110, "四川岷江音乐广播"},
    {1831, "吉林音乐广播"},
    {5022405, "AsiaFM 亚洲音乐台"},
    {4581, "亚洲音乐成都FM96.5"},
    {20071, "AsiaFM 亚洲天空台"},
    {20033, "1047 Nice FM"},
    {4930, "FM102.2亲子智慧电台"},
    {4846, "893音乐广播"},
    {20026, "郁南音乐台"},
    {1608, "陕西故事广播·年代878"},
    {4923, "徐州音乐广播FM91.9"},
    {4878, "海南音乐广播"},
    {20211575, "经典983电台"},
    {4594, "潮州交通音乐广播"},
    {20500097, "经典音乐广播FM94.8"},
    {4885, "陕西青少广播·好听1055"},
    {4585, "福建音乐广播"},
    {2799, "常州音乐广播"},
    {1975, "MUSIC876"},
    {5022391, "Easy Fm"},
    {20500067, "FM95.9清远交通音乐广播"},
    {20211620, "流行音乐广播999正青春"},
    {20067, "贵州FM91.6音乐广播"},
    {5021902, "沧州音乐广播FM103.6"},
    {20207781, "眉山交通音乐广播"},
    {2811, "湖州交通文艺广播"},
    {5022050, "FM89.1吴江综合广播"},
    {20500053, "经典958"},
    {5022520, "盛京FM105.6"},
    {20091, "中国校园之声"},
    {4979, "89.3芒果音乐台"},
    {20835, "秦皇岛音乐广播"},
    {20211678, "廊坊飞扬105"},
    {1677, "青岛音乐体育广播"},
    {4029, "新疆MIXFM1039"},
    {5022338, "冰城1026哈尔滨古典音乐广播"},
    {20207762, "河南经典FM"},
    {4921, "郑州音乐广播"},
    {5022610, "察布查尔FM99.5"},
    {4871, "唐山音乐广播"},
    {1683, "烟台音乐广播FM105.9"},
    {5020, "滁州旅游交通广播"},
    {20440, "新疆昌吉 FM103.3综合广播"},
    {20212387, "凤凰音乐"},
    {20500187, "云梦音乐台"},
};

/*----NVS----*/
void SetConfigPlay(int v) {
  Preferences prefs;
  prefs.begin("audio", false);    // 读写模式创建
  prefs.putInt("output_radio", v);
  prefs.end();
}
void GetConfigPlay() {
  Preferences prefs;
  prefs.begin("audio", false);    // 读写模式创建
  curIndex = prefs.getInt("output_radio",0);
  prefs.end();
}
inline void autoConfigVolume() {
  Preferences prefs;
  prefs.begin("audio", false);    // 读写模式创建
  curVolume = prefs.getInt("output_volume", 70);
  Serial.println(curVolume);
  prefs.end();
}

void SetConfigVolume(int v) {
  Preferences prefs;
  prefs.begin("audio", false);    // 读写模式创建
  prefs.putInt("output_volume", v);
  prefs.end();
}

void nextVolume(int offset) {
  int vol = curVolume/5  + offset*2;
  if (vol >= 0 && vol <= 20) {
    curVolume = vol*5;
    audio.setVolume(vol);//1-20
    SetConfigVolume(curVolume);
    sprintf(buf, "音量: %d", curVolume);
    sp.createSprite(160, 24);
    sp.drawString(buf, 8, 0);
    sp.pushSprite(0, 220);
    sp.deleteSprite();
  }else if(vol > 20){
    vol=20;
    curVolume=100;
    audio.setVolume(curVolume);//1-20
    SetConfigVolume(curVolume);
    sprintf(buf, "音量: %d", curVolume);
    sp.createSprite(160, 16);
    sp.drawString(buf, 8, 0);
    sp.pushSprite(0, 220);
    sp.deleteSprite();
  }else if(vol < 0){
    vol=0;
    curVolume=0;
    audio.setVolume(curVolume);//1-20
    SetConfigVolume(curVolume);
    sprintf(buf, "音量: %d", curVolume);
    sp.createSprite(160, 16);
    sp.drawString(buf, 8, 0);
    sp.pushSprite(0, 220);
    sp.deleteSprite();
  }
}
//下一个播放
void playNext(int offset) {
  int total = radios.size();
  curIndex += offset;
  if (curIndex >= total) {
    curIndex %= total;
  } else if (curIndex < 0) {
    curIndex += total;
  }
  SetConfigPlay(curIndex);

  auto radio = radios[curIndex];
  sprintf(buf, FM_URL, radio.id);

  if(audio.connecttohost(buf)){
    sprintf(buf, "%d.%s", curIndex + 1, radio.name.c_str());
    sp.createSprite(320, 16);
    sp.drawCentreString(buf, 144, 0);
    sp.pushSprite(0, 20);
    // sp.deleteSprite();
    // sp.createSprite(320, 16);
    sp.fillSprite(TFT_BLACK); 
    sp.pushSprite(0, 60);
    sp.deleteSprite();
  }else{
    sprintf(buf, "%d.%s", curIndex + 1, radio.name.c_str());
    sp.createSprite(320, 16);
    sp.drawCentreString(buf, 144, 0);
    sp.pushSprite(0, 20);
    sp.deleteSprite();
    sp.createSprite(320,16);
    sp.drawCentreString("连接失败", 144, 0);
    sp.pushSprite(0, 60);
    sp.deleteSprite();
  }
}


void inline initTFTDevice() {
  tft.init();
  tft.setBrightness(60);
  tft.setFont(FONT16);
  tft.setColorDepth(8);
  tft.fillScreen(TFT_BLACK);
  sp.setFont(FONT16);
  sp.setColorDepth(8);
}

void SetOta0(){
  const esp_partition_t *ota0_partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,          // 分区类型：应用程序
    ESP_PARTITION_SUBTYPE_APP_OTA_0, // 子类型：ota_0
    NULL                             // 标签（可选，设为NULL匹配所有）
  );
  esp_err_t err = esp_ota_set_boot_partition(ota0_partition);
    if (err == ESP_OK) {
      printf("success ota_0\n");
    } else {
      printf("设置失败错误代码0x%x\n", err);
    }
  // delay(1000);
}

inline void  initAudioDevice() {
  audio.setPinout(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO,I2S_MCK_IO);
  audio.setVolume(curVolume/5);
}

/*----网络----*/
std::vector<String> wifi_scan;
inline void autoConfigWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  sp.createSprite(320,16);
  sp.drawCentreString("正在初始化WIFI", 144, 0);
  sp.pushSprite(0, 60);
  sp.deleteSprite();
  int n = WiFi.scanNetworks();
  if(n==0)
  {
    return;}else
  {
    for (int i = 0; i < n; ++i) 
    {
      wifi_scan.push_back(WiFi.SSID(i));
    }
  }
  WiFi.scanDelete();
  bool con_bool = false;

  Preferences prefs;
  prefs.begin("wifi", false);    // 读写模式创建
  for(int i=0;i<10;i++){
    if(i!=0){
      String ssid_n = "ssid" + String(i);
      wifi_list[i].ssid=prefs.getString(ssid_n.c_str(), "");
      if(wifi_list[i].ssid.isEmpty()){break;}
      for(int k=0;k<n;k++)
      {
        if(wifi_scan[k].equals(wifi_list[i].ssid.c_str()))
        {
          String passwd_n = "password" + String(i);
          wifi_list[i].passwd=prefs.getString(passwd_n.c_str(), "");
          WiFi.begin(wifi_list[i].ssid.c_str(), wifi_list[i].passwd.c_str());
          con_bool=true;
          break;
        }else{continue;}
      }
    }else{
      wifi_list[i].ssid=prefs.getString("ssid", "");
      wifi_list[i].passwd=prefs.getString("password", "");
      if (wifi_list[i].ssid.isEmpty() || wifi_list[i].passwd.isEmpty()) {network = 1;return;}
      for(int k=0;k<n;k++)
      {
        if(wifi_scan[k].equals(wifi_list[i].ssid.c_str()))
        {
          WiFi.begin(wifi_list[i].ssid.c_str(), wifi_list[i].passwd.c_str());
          con_bool=true;
          break;
        }else{continue;}
      }
    }
    if(con_bool==true)
    {
      break;
    }
  }
  prefs.end();
  // Serial.println("-------------------\n");
  
  for(int j=0;j<8;j++)
  {
    if((con_bool==true) && (WiFi.status() != WL_CONNECTED))
    {
      delay(500);
    }else{
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    int8_t rssi = WiFi.RSSI();
    Serial.printf("connect=%d",rssi);
    if( rssi < -85 ){
      tft.println("       WIFI信号差,正在重启");
      delay(1000);
      ESP.restart();
    }
  }
  else {
    // Serial.println("WiFi faild");
    network = 1;
  }
}
//wifi_end

inline void showCurrentTime() {
  struct tm info;
  getLocalTime(&info);
  sprintf(buf, "%d年%d月%d日 星期%s", 1900 + info.tm_year, info.tm_mon + 1,
          info.tm_mday, WEEK_DAYS[info.tm_wday]);
  sp.createSprite(320, 16);
  sp.drawCentreString(buf, 144, 0);
  sp.pushSprite(0, 130);
  sp.deleteSprite();
  strftime(buf, 36, "%T", &info);
  sp.createSprite(320, 36);
  sp.drawCentreString(buf, 144, 0, &fonts::FreeSans24pt7b);
  sp.pushSprite(0, 150);
  sp.deleteSprite();
}
inline void showNOnet() {
  sp.createSprite(320, 36);
  sp.drawCentreString("请联网", 144, 0);
  sp.pushSprite(0, 160);
  sp.deleteSprite();
}

void inline startConfigTime() {
  const int timeZone = 8 * 3600;
  configTime(timeZone, 0, "ntp6.aliyun.com", "cn.ntp.org.cn", "ntp.ntsc.ac.cn");
  // while (time(nullptr) < 8 * 3600 * 2) {
  //   delay(300);
  // }
}

//切换固件
void switchPartition() {
  // 获取当前运行的分区
  const esp_partition_t *running = esp_ota_get_running_partition();
  // 获取下一个OTA分区
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
  if (next == NULL) {
    Serial.println("No next OTA partition found");
    return;
  }
  // 设置下一个启动分区
  esp_err_t err = esp_ota_set_boot_partition(next);
  if (err != ESP_OK) {
    Serial.printf("Failed to set boot partition: 0x%x\n", err);
  } else {
    ESP.restart();
  }
  
}
/*------按键------*/
void onButtonClick(void *p) {
  u32_t pin = (u32_t)p;
  switch (pin) {
  case PIN_KEY_MODE:
    audio.pauseResume();
    break;
  // case PIN_KEY_SET:
    // SetOta0(); 
    // ESP.restart();
  // default:
    break;
  }
}

void attachLongPressStart(void *p){
  u32_t pin = (u32_t)p;
  switch (pin) {
  case PIN_KEY_MODE:
    sp.createSprite(320, 16);
    sp.drawCentreString("将在3s后切换系统", 144, 0);
    sp.pushSprite(0, 100);
    sp.deleteSprite();
    playAudioFrom();
    delay(900);
    sp.createSprite(320, 16);
    sp.drawCentreString("将在2s后切换系统", 144, 0);
    sp.pushSprite(0, 100);
    sp.deleteSprite();
    playAudioFrom();
    delay(900);
    sp.createSprite(320, 16);
    sp.drawCentreString("将在1s后切换系统", 144, 0);
    sp.pushSprite(0, 100);
    sp.deleteSprite();
    playAudioFrom();
    delay(500);
    switchPartition();
    break;
  // case PIN_KEY_SET:
    // if(power_button==true){
    //   off=true;
    //   audio.stopSong();
    //   sp.createSprite(320, 16);
    //   sp.drawCentreString("将在3s后关机", 144, 0);
    //   sp.pushSprite(0, 100);
    //   delay(900);
    //   sp.fillSprite(TFT_BLACK); // 清空背景再写新内容
    //   sp.drawCentreString("将在2s后关机", 144, 0);
    //   sp.pushSprite(0, 100);
    //   delay(900);
    //   sp.fillSprite(TFT_BLACK); 
    //   sp.drawCentreString("将在1s后关机", 144, 0);
    //   sp.pushSprite(0, 100);
    //   sp.deleteSprite();

    //   delay(200);
    //   sp.createSprite(320, 240);  // 先创建Sprite
    //   sp.fillSprite(TFT_BLACK);   // 清屏为黑色
    //   sp.pushSprite(0, 0);        // 显示到屏幕
    //   sp.deleteSprite();
    //   tft.fillScreen(TFT_BLACK);
    //   delay(200);
      // digitalWrite(48, LOW);
    // }
    // break;
  default:
    break;
  }
}

void attachLongPressStop(void *p){
  u32_t pin = (u32_t)p;
  switch (pin) {
  // case PIN_KEY_SET:
    // if(power_button==true){SetOta0();digitalWrite(48, LOW);}
    // break;
  default:
    break;}
}
inline void  setupButtons() {
  u32_t btnPins[] = {PIN_KEY_MODE};
  // u32_t btnPins[] = {PIN_KEY_MODE,PIN_KEY_SET};
  for (auto pin : btnPins) {
    auto *btn = new OneButton(pin);
    btn->attachClick(onButtonClick, (void *)pin);
    btn->attachLongPressStart(attachLongPressStart,(void *)pin);
    // btn->attachLongPressStop(attachLongPressStop,(void *)pin);
    buttons.insert({pin, btn});
  }
}
void processEvents() {
  ButtonEvent evt;
  while(eventQueue.getEvent(&evt)) {    
    switch(evt.type) {
      case SINGLE_CLICK:
        if(evt.source == &btn_volup)
        {
          nextVolume(1);
        }else if(evt.source == &btn_voldn){
          nextVolume(-1);
        }
        break;
        
      case DOUBLE_CLICK:
        if(evt.source == &btn_volup)
        {
          playNext(1);
        }else if(evt.source == &btn_voldn){
          playNext(-1);
        }
        break;
      case LONG_PRESS_REPEAT: {
        if(evt.source == &btn_volup)
        {
          playNext(1);
        }else if(evt.source == &btn_voldn){
          playNext(-1);
        }
        break;
      }
    }
  }
}
/*------定时器------*/
void timerCallback() {
  ms2 = ms2 + 10;
}
// ES8311
inline void ES8311Device(){
  pinMode(ES8311_PA, OUTPUT);
  digitalWrite(ES8311_PA, HIGH);
  my_pins.addI2C(PinFunction::CODEC, I2C_SCL, I2C_SDA, ES8311_ADDRESS);
  cfg.input_device = ADC_INPUT_ALL;//ADC_INPUT_LINE1; ADC_INPUT_ALL
  cfg.output_device = DAC_OUTPUT_ALL; 
  cfg.i2s.bits = BIT_LENGTH_16BITS;
  cfg.i2s.rate = RATE_44K;
  cfg.i2s.fmt = I2S_NORMAL;  
    
  //初始化ES8311
  board.begin(cfg);
}

inline void playAudioFrom(){
  size_t bytes_written;
  audio.stopSong();
  i2s_write(I2S_NUM_0, data, sizeof(data), &bytes_written, portMAX_DELAY);
}

void setup() {
  Serial.begin(115200);
  pinMode(48, OUTPUT); //开机保持
  digitalWrite(48, HIGH);

  SetOta0();
  ES8311Device();

  initTFTDevice();
  setupButtons();//
  initAudioDevice(); //

  autoConfigWifi();//联网
  
  if (network==0){
    startConfigTime();
    showCurrentTime();
  }
  timer.attach(0.01, timerCallback);
  autoConfigVolume();//获取音量
  nextVolume(0);//
  GetConfigPlay();
  playNext(0);//
  
}

void loop() {
  audio.loop();
  if ((ms2 - check300ms > 300) &&( off==false)) {
    check300ms = ms2;
    if(network ==0){showCurrentTime();}
    else{showNOnet();}
    // if((!power_button)&&digitalRead(PIN_KEY_SET)){
    //   power_button=true;
    // }
  }
  if ((ms2 - check10ms >= 10)&&( off==false)) {
    check10ms = ms2;
    for (auto it : buttons) {
      it.second->tick();
    }
    adc_val = analogRead(PIN_ADC);    //读取A0口的电压值
    updateButtonState(btn_volup, adc_val);
    updateButtonState(btn_voldn, adc_val); 
    if (lunxun>4){
      processEvents();
      lunxun=0;
    }
    lunxun++;
  }
  if ((ms2 - check5s > 5000)&&( off==false)) {
    check5s = ms2;
    if (WiFi.status() != WL_CONNECTED){
      sp.createSprite(320, 16);
      sp.drawCentreString("WIFI连接失败,正在重启", 144, 0);
      sp.pushSprite(0, 100);
      sp.deleteSprite();
      delay(1000);
      ESP.restart();
    }
  }
}

void audio_info(const char *info) { Serial.println(info); }

void audio_eof_stream(const char *info) { playNext(1); }