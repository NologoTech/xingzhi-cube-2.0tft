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

//SD
#define PIN_SD_CS 46
#define PIN_SD_MOSI 40
#define PIN_SD_MISO 39
#define PIN_SD_SCK 38
 
#define PIN_ADC 2

using namespace std;

#define FONT16 &fonts::efontCN_16

typedef struct {
  String name;
  String path;
} SongItem;

typedef struct {
  String ssid;
  String passwd;
}WIFIMES;
static WIFIMES wifi_list[10];
static const char *WEEK_DAYS[] = {"日", "一", "二", "三", "四", "五", "六"};

Ticker timer;
LGFX tft;
LGFX_Sprite sp(&tft);
long check1s = 0, check10ms = 0,check100ms = 0, check300ms = 0, check5s = 0;
char buf[128] = {0};
int curIndex = 0;
int curVolume = 40;
int network = 0;
vector<SongItem *> songs;
std::map<u32_t, OneButton *> buttons;
String sliders[3];
uint8_t sildeIdx = 0;
extern unsigned long ms2;
extern ButtonADC btn_volup;
extern ButtonADC btn_voldn;
extern EventQueue eventQueue;
int adc_val =0;
int lunxun=0;
bool power_button=false;
bool no_sd=false;
bool off=false;
extern unsigned char data[44350];

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
        printf("ota_0\n");
    } else {
        printf("设置失败错误代码0x%x\n", err);
    }
  // delay(1000);
}

void inline initAudioDevice() {
  audio.setPinout(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO,I2S_MCK_IO);
  audio.setVolume((curVolume/5));
}

void inline showPlayProgress() {
  uint32_t act = audio.getAudioCurrentTime();
  uint32_t afd = audio.getAudioFileDuration();
  sp.createSprite(320, 32);
  sp.drawRoundRect(45, 0, 202, 10, 3, TFT_ORANGE);
  if (act > 0 && afd > 0) {
    int prog = act * 200 / afd;
    sp.fillRoundRect(45, 2, prog, 6, 2, TFT_GREEN);
    sprintf(buf, "%02i:%02d", (act / 60), (act % 60));
    sp.drawString(buf, 45, 16);
    sprintf(buf, "%02i:%02d", (afd / 60), (afd % 60));
    sp.drawRightString(buf, 250, 16);
  }
  sp.pushSprite(0, 50);
  sp.deleteSprite();
}
//下一个播放
void playNext(int offset) {
  audio.stopSong();
  int total = songs.size();
  if (total > 0) {
    curIndex += offset;
    if (curIndex >= total) {
      curIndex %= total;
    } else if (curIndex < 0) {
      curIndex += total;
    }
    SetConfigPlay(curIndex);

    sprintf(buf, "正在播放: %d/%d", curIndex + 1, total);
    sliders[2] = buf;
    auto *song = songs[curIndex];
    auto *name = song->name.c_str();
    sprintf(buf, "%d.%s", curIndex + 1, name);
    if(!no_sd){
      sp.createSprite(320, 24);
      sp.drawCentreString(buf, 146, 0);
      sp.pushSprite(0, 20);
      sp.deleteSprite();
    }
    audio.connecttoFS(SD, song->path.c_str());
  }
}

void inline autoConfigVolume() {
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

void SetConfigPlay(int v) {
  Preferences prefs;
  prefs.begin("audio", false);    // 读写模式创建
  prefs.putInt("output_mp3", v);
  prefs.end();
}
void GetConfigPlay() {
  Preferences prefs;
  prefs.begin("audio", false);    // 读写模式创建
  curIndex = prefs.getInt("output_mp3",0);
  prefs.end();
}

void nextVolume(int offset) {
  int vol2 = curVolume%5;
  int vol = curVolume/5  + offset*2;
  if (vol >= 0 && vol <= 20) {
    curVolume = vol*5+vol2;
    if(curVolume>100){curVolume=100;}
    audio.setVolume(vol);//1-20
    SetConfigVolume(curVolume);
    sprintf(buf, "音量: %d", curVolume);
    sp.createSprite(160, 16);
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

std::vector<String> wifi_scan;
//获取WIFI         wifi_list
void inline autoConfigWifi() {
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
    // Serial.printf("connect=%d",rssi);
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

//显示时间
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
  sp.pushSprite(0, 160);
  sp.deleteSprite();
}
inline void showNOnet() {
  sp.createSprite(320, 36);
  sp.drawCentreString("请联网", 144, 0);
  sp.pushSprite(0, 160);
  sp.deleteSprite();
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
    // Serial.println("Boot partition switched");
    // delay(3000);
    
    ESP.restart();
  }
  
}

//时间同步
void inline startConfigTime() {
  const int timeZone = 8 * 3600;
  configTime(timeZone, 0, "ntp6.aliyun.com", "cn.ntp.org.cn", "ntp.ntsc.ac.cn");
  // while (time(nullptr) < 8 * 3600 * 2) {
  //   delay(300);
  // }
}

//显示IP
inline void showClientIP() {
  tft.clear();
  sprintf(buf, "%s", WiFi.localIP().toString().c_str());
  sp.createSprite(160, 24);
  sp.drawRightString(buf, 160, 0);
  sp.pushSprite(160, 216);
  sp.deleteSprite();
}

void onButtonClick(void *p) {
  u32_t pin = (u32_t)p;
  switch (pin) {
  case PIN_KEY_MODE:
    audio.pauseResume();
    break;
  // case PIN_KEY_SET:
    // SetOta0();
    // ESP.restart();
  default:
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
  //   if(power_button==true){
  //     // SetOta0();
  //     off=true;
  //     audio.stopSong();
  //     sp.createSprite(320, 16);
  //     sp.drawCentreString("将在3s后关机", 144, 0);
  //     sp.pushSprite(0, 100);

  //     delay(900);
  //     sp.fillSprite(TFT_BLACK); // 清空背景再写新内容
  //     sp.drawCentreString("将在2s后关机", 144, 0);
  //     sp.pushSprite(0, 100);

  //     delay(900);
  //     sp.fillSprite(TFT_BLACK); // 清空背景再写新内容
  //     sp.drawCentreString("将在1s后关机", 144, 0);
  //     sp.pushSprite(0, 100);
  //     sp.deleteSprite();

  //     delay(200);
  //     sp.createSprite(320, 240);  // 先创建Sprite
  //     sp.fillSprite(TFT_BLACK);   // 清屏为黑色
  //     sp.pushSprite(0, 0);        // 显示到屏幕s
  //     sp.deleteSprite();
  //     tft.fillScreen(TFT_BLACK);
  //     delay(200);
  //     digitalWrite(48, LOW);
  //     break;
  //   }
  default:
    break;
  }
}

void attachLongPressStop(void *p){
  u32_t pin = (u32_t)p;
  switch (pin) {
  // case PIN_KEY_SET:
  //   if(power_button==true){digitalWrite(48, LOW);}
  //   break;
  default:
    break;
  }
}
void playAudioFrom(){
  size_t bytes_written;
  // digitalWrite(ES8311_PA, LOW);
  // digitalWrite(ES8311_PA, HIGH);
  audio.stopSong();
  i2s_write(I2S_NUM_0, data, sizeof(data), &bytes_written, portMAX_DELAY);
}
//设置按键
void inline setupButtons() {
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

void inline showSlider() {
  sp.createSprite(240, 16);
  String txt = sliders[sildeIdx];
  sp.drawCentreString(txt, 110, 0);
  sp.pushSprite(0, 100);
  sp.deleteSprite();
}

inline void  initSDCard() {
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI);
  SD.begin(PIN_SD_CS);
  // SD.begin(PIN_SD_CS,SPI,4000000,"/sd",5,true);
}

void scanMp3InDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  File root = fs.open(dirname);
  if (!root) {
    sp.createSprite(320, 16);
    sp.drawCentreString("无SD卡", 144, 0);
    sp.pushSprite(0, 20);
    sp.deleteSprite();
    no_sd=true;
    return;
  }
  if (!root.isDirectory()) {
    sp.createSprite(320, 16);
    sp.drawCentreString("无SD卡", 144, 0);
    sp.pushSprite(0, 20);
    sp.deleteSprite();
    no_sd=true;
    return;
  }
  File mp3 = root.openNextFile();
  while (mp3) {
    String path = mp3.path();
    if (mp3.isDirectory()) {
      if (levels) {
        scanMp3InDir(fs, mp3.path(), levels - 1);
      }
    } else if (path.endsWith(".mp3")) {
      SongItem *song = new SongItem();
      song->name = mp3.name();
      song->path = path;
      songs.push_back(song);
    }
    mp3 = root.openNextFile();
  }
}


void timerCallback() {
  ms2 = ms2 + 10;
}

// ES8311
void ES8311Device(){
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

void setup() {
  Serial.begin(115200);
  pinMode(48, OUTPUT); //开机保持
  digitalWrite(48, HIGH);

  SetOta0();
  ES8311Device();

  initTFTDevice();
  setupButtons();
  initSDCard();

  initAudioDevice();
  autoConfigWifi();
  
  if (network==0){
    startConfigTime();
    showCurrentTime();
  }
  scanMp3InDir(SD, "/", 3);
  timer.attach(0.01, timerCallback);
  autoConfigVolume();
  nextVolume(0);
  GetConfigPlay();
  playNext(0);
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
  if ((ms2 - check1s > 1000)&&( off==false)) {
    check1s = ms2;
    showPlayProgress();
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

void audio_eof_mp3(const char *info) { playNext(1); }