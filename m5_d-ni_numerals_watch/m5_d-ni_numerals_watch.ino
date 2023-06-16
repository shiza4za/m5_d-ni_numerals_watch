// m5_d-ni_numerals_watch.ino
// version 0.3.1 | MIT License | https://github.com/shiza4za/m5_d-ni_numerals_watch/blob/main/LICENSE

#include <iostream>
#include <bitset>
#include <vector>
#include <esp_sntp.h>
#include <WiFi.h>
#include <M5Unified.h>





//////////////////////////// カスタマイズ ////////////////////////////

// ★NTP同期時に接続するWi-FiのSSID
constexpr const char* ssid    = "your SSID";
// ★NTP同期時に接続するWi-Fiのpw
constexpr const char* ssid_pw = "your SSID password";

// ★LCD輝度段階
const int brightness_0      =  30;      // <レベル0>  デフォルト  30
const int brightness_1      =  85;      // <レベル1>  デフォルト  85
const int brightness_2      = 200;      // <レベル2>  デフォルト 200

// ★LCD輝度段階切替のデフォルトレベル
int BtnB_lcd_lv = 1;    // デフォルト 1

// ★デフォルトで答え合わせを表示する/しない設定
bool BtnB_dec_ck     = false;    // デフォルト false

// ★背景色
const int color_lcd       = BLACK;    // デフォルト BLACK
// ★文字色(バッテリ以外)
const int color_text      = WHITE;    // デフォルト WHITE
// ★シンボルの色
const int color_sym       = RED;      // デフォルト RED

// ★バッテリ表示の各文字色
const int color_bt_good   = 0x0723;   // 100-40 デフォルト緑 0x0723
const int color_bt_hmm    = 0xfd66;   //  39-20 デフォルト黄 0xfd66
const int color_bt_danger = 0xfa86;   //  19- 0 デフォルト赤 0xf982

// ★BtnBでLCD輝度段階をレベル2にしたときの"BRT"の色
const int color_brt       = 0xfd66;   // デフォルト黄 0xfd66

// ★自動終了するまでの時間(秒)
// 　※ミリ秒ではなく秒です
// 　※多分58秒未満を推奨
// 　※0にすると、勝手にオフしなくなります
#define BUTTON_TIMEOUT 0   // デフォルト 0

// ★BtnC押下時・または一定秒間操作がなかったときに、
// 　powerOffまたはdeepSleepする設定
// 　・trueでpowerOff関数実行。次回電源ボタンを押すと再起動
// 　・falseでdeepSleep関数実行。次回画面など触れると再起動
bool poweroffmode = true;   // デフォルト true

// ★稼働中のディレイ(ms)
const int delay_in_loop = 10;   // デフォルト 10

/////////////////////////////////////////////////////////////////////

// グローバル変数として宣言
auto start_time = time(nullptr);
auto start_time_local = localtime(&start_time);
int start_time_local_sec = start_time_local->tm_sec;
const int defy = 20;
const char* const week_str[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
// bool BtnB_lcd_ck = false;
int brightness = 0;
bool first_load = false;

// シンボル開始ポイント
int       sym_start_x_p = 23; // 時
const int sym_start_y   = 85; // y固定

int hour_a = start_time_local->tm_hour;
int hour_b = start_time_local->tm_hour;
int min_a = start_time_local->tm_min;
int min_b = start_time_local->tm_min;
int sec_a = start_time_local->tm_sec;
int sec_b = start_time_local->tm_sec;





// BtnAで呼出：RTC確認 → Wi-Fi接続 → NTPサーバ接続 → 時刻同期
void connect() {
  M5.Lcd.clear(color_lcd);
  M5.Lcd.setCursor(0, 0);

  // バージョン
  M5.Lcd.setTextColor(0xad55, color_lcd);
  M5.Display.printf("\nversion 0.3.1\n\n");

  // SSIDの参考表示
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.printf("SSID: ");
  M5.Display.printf("%s\n\n", ssid);
  vTaskDelay(1000);

  // RTC状態表示
  M5.Display.printf("RTC...");
  if (!M5.Rtc.isEnabled()) {
    M5.Lcd.setTextColor(0xfa86, color_lcd);
    M5.Display.printf("ERR. \nPlease power off \nand try again with the \nRTC available.\n\n");
    for (;;) { vTaskDelay(10000); }
  }
  M5.Lcd.setTextColor(0x0723, color_lcd);
  M5.Display.printf("OK.\n\n");

  // Wi-Fi接続
  WiFi.disconnect();
  vTaskDelay(1000);
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.printf("Wi-Fi...");
  WiFi.begin(ssid, ssid_pw);
  while (WiFi.status() != WL_CONNECTED) { M5.Display.printf("."); vTaskDelay(500); }
  M5.Lcd.setTextColor(0x0723, color_lcd);
  M5.Display.printf("OK.\n\n");
  vTaskDelay(1000);

  // NTP時刻取得
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.printf("NTP...");
  configTzTime("JST-9", "ntp.nict.jp", "ntp.nict.jp", "ntp.nict.jp");
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    M5.Display.printf("."); vTaskDelay(500);
  }
  M5.Lcd.setTextColor(0x0723, color_lcd);
  M5.Display.printf("OK.\n\n");
  vTaskDelay(1000);
  time_t get_time = time(nullptr);
  tm* local_time = localtime(&get_time);
  local_time->tm_hour += 9;
  time_t jst = mktime(local_time);
  tm* jstTime = gmtime(&jst);
  M5.Rtc.setDateTime(gmtime(&jst));
  vTaskDelay(500);
  WiFi.disconnect();
  vTaskDelay(500);

  firstScreen();
}





// 起動オフの処理選択
void poweroffTask() {
  if        (poweroffmode == true) {
    M5.Power.powerOff();
  } else if (poweroffmode == false) {
    M5.Power.deepSleep(0);
  }
}





// LCD輝度状態によってBRTの文字と色変更
void displayBrt(int BtnB_lcd_lv) {
  M5.Lcd.setCursor(132, 16*14);
  if        (BtnB_lcd_lv == 0) {
    M5.Lcd.setTextColor(color_text, color_lcd);
    M5.Display.printf("BRT:0");
  } else if (BtnB_lcd_lv == 1) {
    M5.Lcd.setTextColor(color_text, color_lcd);
    M5.Display.printf("BRT:1");
  } else if (BtnB_lcd_lv == 2) {
    M5.Lcd.setTextColor(color_brt, color_lcd);
    M5.Display.printf("BRT:2");
  }
  M5.Lcd.setTextColor(color_text, color_lcd);

  M5.Lcd.setCursor(0, 0);
}





// バッテリ残量表示
void displayBattery() {
  // 100-50 緑
  //  49-20 黄
  //  19- 0 赤
  int battery_level = M5.Power.getBatteryLevel();
  M5.Lcd.setCursor(0, 16*14);
  if      (battery_level <= 100 && battery_level >= 40) { M5.Lcd.setTextColor(color_bt_good, color_lcd); }
  else if (battery_level <=  39 && battery_level >= 20) { M5.Lcd.setTextColor(color_bt_hmm, color_lcd); }
  else if (battery_level <=  19                       ) { M5.Lcd.setTextColor(color_bt_danger, color_lcd); }
  M5.Display.printf("%03d", battery_level);
  M5.Lcd.setTextColor(color_text, color_lcd);
}





// バイナリ点滅以外の基本表示一式
void firstScreen() {
  M5.Lcd.clear(color_lcd);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(color_text, color_lcd);

  // メインのフレーム
  M5.Lcd.fillRect( 17,  79, 286,   6, color_sym);
  M5.Lcd.fillRect( 17, 135, 286,   6, color_sym);
  M5.Lcd.fillRect( 17,  85,   6,  50, color_sym);
  M5.Lcd.fillRect( 73,  85,   6,  50, color_sym);
  M5.Lcd.fillRect(129,  85,   6,  50, color_sym);
  M5.Lcd.fillRect(185,  85,   6,  50, color_sym);
  M5.Lcd.fillRect(241,  85,   6,  50, color_sym);
  M5.Lcd.fillRect(297,  85,   6,  50, color_sym);
  // はみ出してるとこ
  M5.Lcd.fillRect( 12,  79,   5,   6, color_sym);
  M5.Lcd.fillRect( 12, 135,   5,   6, color_sym);
  M5.Lcd.fillRect(303,  79,   5,   6, color_sym);
  M5.Lcd.fillRect(303, 135,   5,   6, color_sym);
  M5.Lcd.drawLine( 11,  80,  11,  83, color_sym);
  M5.Lcd.drawLine(308,  80, 308,  83, color_sym);
  M5.Lcd.drawLine( 11, 136,  11, 139, color_sym);
  M5.Lcd.drawLine(308, 136, 308, 139, color_sym);

  // NTP・Shutdown文字
  M5.Display.setTextSize(2);
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.setCursor(55, 16*14);
  M5.Display.printf("NTP");
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.setCursor(223, 16*14);
  M5.Display.printf("Shutdown");

  first_load = true;
}





//// シンボルパターン

// clear
void sym_clear(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +   0, sym_start_y +   0,                50,                50, color_lcd);
}



// 0
void sym_0(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +  23, sym_start_y +  23,                 4,                 4, color_sym);
  M5.Lcd.drawLine(sym_start_x +  22, sym_start_y +  23, sym_start_x +  22, sym_start_y +  26, color_sym);
  M5.Lcd.drawLine(sym_start_x +  27, sym_start_y +  23, sym_start_x +  27, sym_start_y +  26, color_sym);
  M5.Lcd.drawLine(sym_start_x +  23, sym_start_y +  22, sym_start_x +  26, sym_start_y +  22, color_sym);
  M5.Lcd.drawLine(sym_start_x +  23, sym_start_y +  27, sym_start_x +  26, sym_start_y +  27, color_sym);
}



// 1
void sym_1(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +  23, sym_start_y +   0,                 4,                50, color_sym);
}

// 2
void sym_2(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +   0, sym_start_y +   0,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   1, sym_start_y +   1,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   2, sym_start_y +   2,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   3, sym_start_y +   3,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   4, sym_start_y +   4,                 4,                 3, color_sym);

  M5.Lcd.fillRect(sym_start_x +   5, sym_start_y +   6,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   6, sym_start_y +   8,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   7, sym_start_y +  10,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   8, sym_start_y +  12,                 4,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   9, sym_start_y +  15,                 4,                 5, color_sym);
  M5.Lcd.fillRect(sym_start_x +  10, sym_start_y +  19,                 4,                12, color_sym);
  M5.Lcd.fillRect(sym_start_x +   9, sym_start_y +  30,                 4,                 5, color_sym);
  M5.Lcd.fillRect(sym_start_x +   8, sym_start_y +  34,                 4,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   7, sym_start_y +  37,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   6, sym_start_y +  39,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   5, sym_start_y +  41,                 4,                 3, color_sym);

  M5.Lcd.fillRect(sym_start_x +   4, sym_start_y +  43,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   3, sym_start_y +  44,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   2, sym_start_y +  45,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   1, sym_start_y +  46,                 4,                 3, color_sym);
  M5.Lcd.fillRect(sym_start_x +   0, sym_start_y +  47,                 4,                 3, color_sym);
}

// 3
void sym_3(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.drawLine(sym_start_x +  21, sym_start_y +   0, sym_start_x +   0, sym_start_y +  21, color_sym);
  M5.Lcd.drawLine(sym_start_x +  22, sym_start_y +   0, sym_start_x +   0, sym_start_y +  22, color_sym);
  M5.Lcd.drawLine(sym_start_x +  23, sym_start_y +   0, sym_start_x +   0, sym_start_y +  23, color_sym);
  M5.Lcd.drawLine(sym_start_x +  24, sym_start_y +   0, sym_start_x +   0, sym_start_y +  24, color_sym);
  M5.Lcd.drawLine(sym_start_x +  25, sym_start_y +   0, sym_start_x +   0, sym_start_y +  25, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  23, sym_start_x +  26, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  24, sym_start_x +  25, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  25, sym_start_x +  24, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  26, sym_start_x +  23, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  27, sym_start_x +  22, sym_start_y +  49, color_sym);
}

// 4
void sym_4(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +  24, sym_start_y +  11,                26,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  23, sym_start_y +  12,                 4,                38, color_sym);
}



// 5
void sym_5(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +  0, sym_start_y +   23,                50,                 4, color_sym);
}

// 10
void sym_10(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +   0, sym_start_y +  46,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   1, sym_start_y +  45,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   2, sym_start_y +  44,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   3, sym_start_y +  43,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   4, sym_start_y +  42,                 3,                 4, color_sym);

  M5.Lcd.fillRect(sym_start_x +   6, sym_start_y +  41,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +   8, sym_start_y +  40,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  10, sym_start_y +  39,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  12, sym_start_y +  38,                 4,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  15, sym_start_y +  37,                 5,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  19, sym_start_y +  36,                12,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  30, sym_start_y +  37,                 5,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  34, sym_start_y +  38,                 4,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  37, sym_start_y +  39,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  39, sym_start_y +  40,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  41, sym_start_y +  41,                 3,                 4, color_sym);

  M5.Lcd.fillRect(sym_start_x +  43, sym_start_y +  42,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  44, sym_start_y +  43,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  45, sym_start_y +  44,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  46, sym_start_y +  45,                 3,                 4, color_sym);
  M5.Lcd.fillRect(sym_start_x +  47, sym_start_y +  46,                 3,                 4, color_sym);
}

// 15
void sym_15(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  23, sym_start_x +  26, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  24, sym_start_x +  25, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  25, sym_start_x +  24, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  26, sym_start_x +  23, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +   0, sym_start_y +  27, sym_start_x +  22, sym_start_y +  49, color_sym);
  M5.Lcd.drawLine(sym_start_x +  26, sym_start_y +  49, sym_start_x +  49, sym_start_y +  26, color_sym);
  M5.Lcd.drawLine(sym_start_x +  25, sym_start_y +  49, sym_start_x +  49, sym_start_y +  25, color_sym);
  M5.Lcd.drawLine(sym_start_x +  24, sym_start_y +  49, sym_start_x +  49, sym_start_y +  24, color_sym);
  M5.Lcd.drawLine(sym_start_x +  23, sym_start_y +  49, sym_start_x +  49, sym_start_y +  23, color_sym);
  M5.Lcd.drawLine(sym_start_x +  22, sym_start_y +  49, sym_start_x +  49, sym_start_y +  22, color_sym);
}

// 20
void sym_20(int sym_start_x_p) {
  int sym_start_x = sym_start_x_p;
  M5.Lcd.fillRect(sym_start_x +  11, sym_start_y +   0,                 4,                26, color_sym);
  M5.Lcd.fillRect(sym_start_x +  12, sym_start_y +  23,                38,                 4, color_sym);
}




// 5&25進数変換

void convert_5_25(int number, int sym_start_x_p) {

  int num = number;

  sym_start_x_p -= 56;

  if        (num >=  0 && num <= 24) {
    sym_0(sym_start_x_p);

  } else if (num >= 25 && num <= 49) {
    sym_1(sym_start_x_p);
    num -= 25;
  } else if (num >= 50 && num <= 74) {
    sym_2(sym_start_x_p);
    num -= 50;
  }

  sym_start_x_p += 56;

  if        (num ==  0             ) {
    sym_0(sym_start_x_p);

  } else if (num >=  1 && num <=  4) {


  } else if (num >=  5 && num <=  9) {
    sym_5(sym_start_x_p);
    num -=  5;
  } else if (num >= 10 && num <= 14) {
    sym_10(sym_start_x_p);
    num -= 10;
  } else if (num >= 15 && num <= 19) {
    sym_15(sym_start_x_p);
    num -= 15;
  } else if (num >= 20 && num <= 24) {
    sym_20(sym_start_x_p);
    num -= 20;
  }

  if        (num == 1) {
    sym_1(sym_start_x_p);
  } else if (num == 2) {
    sym_2(sym_start_x_p);
  } else if (num == 3) {
    sym_3(sym_start_x_p);
  } else if (num == 4) {
    sym_4(sym_start_x_p);
  }
}





// ////////////////////////////////////////////////////////////////////////////////



void setup() {
  auto cfg = M5.config();

  // ★外部のRTCを読み取る場合は、コメント外します
  // 　Core2はRTC内蔵しているので不要
  // cfg.external_rtc  = true;

  M5.begin(cfg);
  M5.Displays(0).setTextSize(2);
  if        (BtnB_lcd_lv == 0) {
    brightness = brightness_0;
  } else if (BtnB_lcd_lv == 1) {
    brightness = brightness_1;
  } else if (BtnB_lcd_lv == 2) {
    brightness = brightness_2;
  }
  M5.Lcd.setBrightness(brightness);

  firstScreen();

  // 操作なし時間確認用 基準秒
  start_time = time(nullptr);
  start_time_local = localtime(&start_time);
  start_time_local_sec = start_time_local->tm_sec;

  int hour_a = start_time_local->tm_hour;
  int min_a = start_time_local->tm_min;
  int sec_a = start_time_local->tm_sec;

}



// ////////////////////////////////////////////////////////////////////////////////



void loop() {
  // vTaskDelay(delay_in_loop);
  M5.update();

  displayBrt(BtnB_lcd_lv);


  auto now_time = time(nullptr);
  auto now_time_local = localtime(&now_time);


  // 操作なし時間確認、経過したら起動オフ
  int now_time_local_sec = now_time_local->tm_sec;

  if (start_time_local_sec > now_time_local_sec) {
    now_time_local_sec += 60;
  }
  if (BUTTON_TIMEOUT != 0) {
    if (now_time_local_sec - start_time_local_sec > BUTTON_TIMEOUT) {
      poweroffTask();
    }
  }




  // ボタン操作
    auto get_detail = M5.Touch.getDetail(1);
    // m5::touch_state_t get_state;
    // get_state = get_detail.state;

    // BtnA ちょっと長押しでWi-Fi・NTPサーバ接続開始
    if (M5.BtnA.wasHold()) {
      connect();
      start_time_local_sec = now_time_local_sec;
    }

    // タッチでデシマル表示/非表示
    if (get_detail.wasClicked()) {
      if (BtnB_dec_ck == false) {
        BtnB_dec_ck = true;
        start_time_local_sec = now_time_local_sec;
      } else if (BtnB_dec_ck == true) {
        BtnB_dec_ck = false;
        start_time_local_sec = now_time_local_sec;
        // BtnBで呼び出したデシマル表示を塗り潰して無理やり非表示
        // M5.Display.fillRect(0, 0, 60, 222, color_lcd);
      }
    }

    // BtnB ちょっと長押しでLCD輝度MAX(brightness値<->MAX)
    if (M5.BtnB.wasHold()) {
      if        (BtnB_lcd_lv == 0) {
        M5.Lcd.setBrightness(brightness_1);
        BtnB_lcd_lv = 1;
      } else if (BtnB_lcd_lv == 1) {
        M5.Lcd.setBrightness(brightness_2);
        BtnB_lcd_lv = 2;
      } else if (BtnB_lcd_lv == 2) {
        M5.Lcd.setBrightness(brightness_0);
        BtnB_lcd_lv = 0;
      }
      start_time_local_sec = now_time_local_sec;
    }

    // BtnC ちょっと長押しで起動オフ
    if (M5.BtnC.wasHold()) {
      poweroffTask();
    }
  //





  int year = now_time_local->tm_year+1900;
  int month = now_time_local->tm_mon+1;
  int day = now_time_local->tm_mday;
  int week = now_time_local->tm_wday;
  int hour = now_time_local->tm_hour;
  int min = now_time_local->tm_min;
  int sec = now_time_local->tm_sec;

  if (first_load == true) {
    convert_5_25(hour, sym_start_x_p);
    convert_5_25(min, sym_start_x_p+112);
    convert_5_25(sec, sym_start_x_p+224);
    first_load == false;
  }


  // 時
  hour_b = hour;
  if (hour_a != hour_b) {
    sym_clear(sym_start_x_p);
    convert_5_25(hour, sym_start_x_p);
    hour_a = hour_b;
  }

  // 分
  min_b = min;
  if (min_a != min_b) {
    sym_clear(sym_start_x_p+112);
    convert_5_25(min, sym_start_x_p+112);
    min_a = min_b;
    if ( min_b == 0 || min_b == 25 || min_b == 50 ) {
      sym_clear(sym_start_x_p+112-56);
      convert_5_25(min, sym_start_x_p+112);
      min_a = min_b;
    }

  }

  // 秒
  sec_b = sec;
  if (sec_a != sec_b) {
    sym_clear(sym_start_x_p+224);
    convert_5_25(sec, sym_start_x_p+224);
    sec_a = sec_b;
    if ( sec_b == 0 || sec_b == 25 || sec_b == 50 ) {
      sym_clear(sym_start_x_p+224-56);
      convert_5_25(sec, sym_start_x_p+224);
      sec_a = sec_b;
    }
  }





  // デシマル表示出力
    if (BtnB_dec_ck == true) {
      M5.Lcd.setTextColor(color_sym, color_lcd);
    } else {
      M5.Lcd.setTextColor(color_lcd, color_lcd);
    }
    // 上段
    M5.Lcd.setCursor(37, 56);
    M5.Display.printf("%02d", hour);

    M5.Lcd.setCursor(37+56, 56);
    M5.Display.printf("%02d", (((min / 25) % 25) * 25));
    // M5.Lcd.setCursor(37+56+35, 56);
    // M5.Display.printf("+");

    M5.Lcd.setCursor(37+56+56, 56);
    M5.Display.printf("%02d", (min % 25));

    M5.Lcd.setCursor(37+56+56+56, 56);
    M5.Display.printf("%02d", (((sec / 25) % 25) * 25));
    // M5.Lcd.setCursor(37+56+56+56+35, 56);
    // M5.Display.printf("+");

    M5.Lcd.setCursor(37+56+56+56+56, 56);
    M5.Display.printf("%02d", (sec % 25));

    // 下段
    M5.Lcd.setCursor( 37, 150);
    M5.Display.printf("%02d", hour);
    M5.Lcd.setCursor(122, 150);
    M5.Display.printf("%02d", min);
    M5.Lcd.setCursor(233, 150);
    M5.Display.printf("%02d", sec);


  M5.Display.setTextSize(2);
  //


  displayBattery();


}
