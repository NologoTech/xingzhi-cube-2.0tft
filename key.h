#ifndef __KEY_H__
#define __KEY_H__

#include <Arduino.h>
#define DEBOUNCE_TIME      20    // 消抖时间20ms
#define LONG_PRESS_DELAY   800   // 长按判定时间800ms
#define DOUBLE_CLICK_GAP   300   // 双击间隔300ms
#define REPEAT_INTERVAL    200   // 长按重复间隔200ms
#define POLL_INTERVAL      10    // 轮询间隔10ms
#define EVENT_QUEUE_SIZE 5
// uint16_t adc_val = 0;
unsigned long ms2 = 0;

enum ButtonEventType  {
  NO_EVENT,
  SINGLE_CLICK,
  DOUBLE_CLICK,
  LONG_PRESS,
  LONG_PRESS_REPEAT
};

// / 事件结构体
struct ButtonEvent {
  ButtonEventType type;
  struct ButtonADC* source;
  unsigned long timestamp;
};

// 环形队列实现
class EventQueue {
private:
  ButtonEvent queue[EVENT_QUEUE_SIZE];
  uint8_t head = 0;
  uint8_t tail = 0;
  uint8_t count = 0;

public:
  bool putEvent(const ButtonEvent &evt) {
    if(count >= EVENT_QUEUE_SIZE) return false;
    
    queue[tail] = evt;
    tail = (tail + 1) % EVENT_QUEUE_SIZE;
    count++;
    return true;
  }

  bool getEvent(ButtonEvent *evt) {
    if(count == 0) return false;
    
    *evt = queue[head];
    head = (head + 1) % EVENT_QUEUE_SIZE;
    count--;
    return true;
  }

  bool isEmpty() const { return count == 0; }
};
EventQueue eventQueue;

struct ButtonADC {
  // ADC参数
  uint16_t adc_low;
  uint16_t adc_high;
  
  // 状态机
  bool last_raw_state;      // 原始状态
  bool stable_state;        // 稳定状态
  bool is_pressed;          // 有效按下状态
  bool is_debouncing;     // 消抖状态标志
  unsigned long press_ts;    // 按下时间戳
  uint32_t last_repeat_ts;// 长按重复计时
  
  // 事件控制
  uint8_t click_count;       // 点击计数
  unsigned long release_ts;  // 释放时间戳
  bool longpress_flag;      // 长按标记
};

ButtonADC btn_volup = {1200, 2800, false, false, false, 0, 0, 0, false};
ButtonADC btn_voldn = {50, 1000, false, false, false, 0, 0, 0, false};


void updateButtonState(ButtonADC &btn, uint16_t adc_val) {
  static ButtonEvent evt;
  const bool previous_stable_state = btn.stable_state;
  // 更新原始状态
  bool current_raw = (adc_val > btn.adc_low) && (adc_val < btn.adc_high);
  // btn.last_raw_state = btn.stable_state;
  // // 消抖处理
  if (current_raw != btn.stable_state && btn.is_debouncing) {
    btn.press_ts = ms2;
    btn.is_debouncing = false;
  }
  // 更新稳定状态
  if (ms2 - btn.press_ts > DEBOUNCE_TIME) {
    btn.stable_state = current_raw;
    btn.is_debouncing = true;
  }
  btn.is_pressed = btn.stable_state;

  const bool is_physical_release = previous_stable_state && !btn.stable_state;

  if (is_physical_release) {
    if (ms2 - btn.release_ts < DOUBLE_CLICK_GAP) {
      btn.click_count = (btn.click_count < 255) ? btn.click_count + 1 : 1;
    } else {
      btn.click_count = 1; // 超过间隔则重置计数
    }
    btn.release_ts = ms2;      // 更新释放时间戳
    btn.longpress_flag = false;// 清除长按标记
  }
  // 长按检测（优先处理）
  if(btn.is_pressed) {
    unsigned long duration = ms2 - btn.press_ts;
    
    if(duration >= LONG_PRESS_DELAY) {
      if(!btn.longpress_flag) {
        evt = {LONG_PRESS, &btn, ms2};
        eventQueue.putEvent(evt);
        btn.longpress_flag = true;
        btn.last_repeat_ts = ms2;
      }
      else if(ms2 - btn.last_repeat_ts >= REPEAT_INTERVAL){
        evt = {LONG_PRESS_REPEAT, &btn, ms2};
        eventQueue.putEvent(evt);
        btn.last_repeat_ts = ms2;
      }
    }
    return;
  }

  // 双击检测窗口
  if(!btn.is_pressed && (ms2 - btn.release_ts >= DOUBLE_CLICK_GAP)) {
    if(btn.click_count == 1) {
      evt = {SINGLE_CLICK, &btn, ms2};
      eventQueue.putEvent(evt);
      btn.click_count = 0;
    }
    else if(btn.click_count >= 2) {
      evt = {DOUBLE_CLICK, &btn, ms2};
      eventQueue.putEvent(evt);
      btn.click_count = 0;
    }
  }
}

#endif