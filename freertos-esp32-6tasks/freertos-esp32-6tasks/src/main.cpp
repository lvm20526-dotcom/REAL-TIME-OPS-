/*
 * FreeRTOS tren ESP32 - Project ca nhan
 * Dat file nay vao thu muc src/ cua project PlatformIO.
 *
 * ---------------- SO DO NOI DAY (ESP32 DevKit V1) ----------------
 *  LED1  -> GPIO25 --[220R]-- LED -- GND
 *  LED2  -> GPIO26 --[220R]-- LED -- GND
 *  LED3  -> GPIO27 --[220R]-- LED -- GND
 *  BTN1  -> GPIO32 -- nut -- GND   (dung dien tro keo len noi)
 *  BTN2  -> GPIO33 -- nut -- GND   (dung dien tro keo len noi)
 *  LDR   -> 3.3V -- LDR -- GPIO34 -- 10k -- GND  (sang nhieu -> gia tri lon)
 *  OLED SSD1306 I2C: SDA -> GPIO21, SCL -> GPIO22, VCC -> 3.3V, GND -> GND
 * -----------------------------------------------------------------
 *
 * Task 1: LED1 nhay voi chu ky T = 200 ms
 * Task 2: Doc cam bien anh sang, luu vao bien toan cuc (500 ms/lan)
 * Task 3: BTN1 dao trang thai LED2 (polling trong vong lap)
 * Task 4: Nhan BTN2 -> "cap" Binary Semaphore
 * Task 5: Cho Binary Semaphore -> bat LED3 trong 3 s roi tat, cho tiep
 * Task 6: Hien thi trang thai LED2, gia tri anh sang, dem nguoc LED3 len OLED (500 ms/lan)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== Cau hinh chan =====================
#define LED1_PIN   25
#define LED2_PIN   26
#define LED3_PIN   27
#define BTN1_PIN   32
#define BTN2_PIN   33
#define LDR_PIN    34      // ADC1, chi la chan input
#define OLED_SDA   21
#define OLED_SCL   22
#define OLED_ADDR  0x3C

#define SCREEN_W   128
#define SCREEN_H   64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ===================== Thong so task =====================
// Luu y: tren ESP32 (ESP-IDF), tham so stack cua xTaskCreate tinh bang BYTE.
#define STACK_SMALL   2048
#define STACK_OLED    4096
#define APP_CORE      1      // Core 0 de cho WiFi/BT, cac task ung dung chay tren Core 1

#define PRIO_T1   1
#define PRIO_T2   2
#define PRIO_T3   2
#define PRIO_T4   2
#define PRIO_T5   3          // Cao nhat: phan hoi ngay khi nhan duoc semaphore
#define PRIO_T6   1

// ===================== Tai nguyen dung chung =====================
static int  g_lightRaw     = 0;      // Task2 ghi,  Task6 doc
static bool g_led2On       = false;  // Task3 ghi,  Task6 doc
static int  g_led3RemainMs = 0;      // Task5 ghi,  Task6 doc

SemaphoreHandle_t g_dataMutex = NULL;  // Mutex bao ve 3 bien toan cuc o tren
SemaphoreHandle_t g_btnSem    = NULL;  // Binary semaphore: Task4 -> Task5

TaskHandle_t hT1, hT2, hT3, hT4, hT5, hT6;

// ===================== Ham tien ich =====================
// Chong doi nut nhan (debounce) bang phan mem.
// Tra ve true dung 1 lan khi nut chuyen tu nha -> nhan (canh xuong).
struct Button {
  int pin;
  int stable;
  int lastRead;
  TickType_t lastChange;
};

static bool buttonPressed(Button &b) {
  int r = digitalRead(b.pin);
  TickType_t now = xTaskGetTickCount();
  if (r != b.lastRead) {               // Tin hieu vua thay doi -> bat dau dem lai
    b.lastRead = r;
    b.lastChange = now;
  }
  if ((now - b.lastChange) >= pdMS_TO_TICKS(30) && r != b.stable) {
    b.stable = r;                      // On dinh du 30 ms -> chap nhan trang thai moi
    return (r == LOW);                 // LOW = dang nhan (keo len noi)
  }
  return false;
}

static void setLed3Remain(int ms) {
  xSemaphoreTake(g_dataMutex, portMAX_DELAY);
  g_led3RemainMs = ms;
  xSemaphoreGive(g_dataMutex);
}

// ===================== TASK 1: LED1 blink T = 200 ms =====================
void Task1_Blink(void *pv) {
  const TickType_t halfPeriod = pdMS_TO_TICKS(100);  // T = 200 ms -> dao trang thai moi 100 ms
  TickType_t lastWake = xTaskGetTickCount();
  bool state = false;

  for (;;) {
    state = !state;
    digitalWrite(LED1_PIN, state);
    vTaskDelayUntil(&lastWake, halfPeriod);           // Chu ky tuyet doi, khong bi troi
  }
}

// ===================== TASK 2: Doc cam bien 500 ms/lan =====================
void Task2_Sensor(void *pv) {
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    int value = analogRead(LDR_PIN);                  // 0..4095 (ADC 12 bit)

    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      g_lightRaw = value;
      xSemaphoreGive(g_dataMutex);
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
  }
}

// ===================== TASK 3: BTN1 dao trang thai LED2 =====================
void Task3_Button1(void *pv) {
  Button btn1 = {BTN1_PIN, HIGH, HIGH, 0};

  for (;;) {
    if (buttonPressed(btn1)) {
      xSemaphoreTake(g_dataMutex, portMAX_DELAY);
      g_led2On = !g_led2On;
      bool s = g_led2On;
      xSemaphoreGive(g_dataMutex);

      digitalWrite(LED2_PIN, s);
      Serial.printf("[T3] LED2 -> %s\n", s ? "ON" : "OFF");
    }
    vTaskDelay(pdMS_TO_TICKS(10));                    // Nhuong CPU, quet nut moi 10 ms
  }
}

// ===================== TASK 4: BTN2 cap Binary Semaphore =====================
void Task4_Button2(void *pv) {
  Button btn2 = {BTN2_PIN, HIGH, HIGH, 0};

  for (;;) {
    if (buttonPressed(btn2)) {
      xSemaphoreGive(g_btnSem);                       // "Cap" semaphore -> danh thuc Task5
      Serial.println("[T4] Give BinarySemaphore");
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ===================== TASK 5: Cho semaphore -> LED3 sang 3 s =====================
void Task5_Led3(void *pv) {
  for (;;) {
    // Task o trang thai BLOCKED, khong ton CPU, cho den khi co semaphore
    if (xSemaphoreTake(g_btnSem, portMAX_DELAY) == pdTRUE) {
      Serial.println("[T5] Take BinarySemaphore -> LED3 ON 3s");
      digitalWrite(LED3_PIN, HIGH);

      for (int remain = 3000; remain > 0; remain -= 100) {
        setLed3Remain(remain);                        // Cap nhat dem nguoc cho Task6
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      setLed3Remain(0);
      digitalWrite(LED3_PIN, LOW);

      // Bo lan nhan BTN2 bi don lai trong luc LED3 dang sang (tuy chon thiet ke).
      // Neu xoa dong nay, mot lan nhan trong 3 s se kich hoat them mot chu ky nua.
      xSemaphoreTake(g_btnSem, 0);
    }
  }
}

// ===================== TASK 6: Hien thi OLED 500 ms/lan =====================
void Task6_Display(void *pv) {
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t cycle = 0;

  for (;;) {
    // Sao chep nhanh du lieu ra bien cuc bo -> giu mutex trong thoi gian ngan nhat
    int light, remain;
    bool led2;
    xSemaphoreTake(g_dataMutex, portMAX_DELAY);
    light  = g_lightRaw;
    led2   = g_led2On;
    remain = g_led3RemainMs;
    xSemaphoreGive(g_dataMutex);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("FreeRTOS ESP32 Demo");
    display.drawLine(0, 10, SCREEN_W, 10, SSD1306_WHITE);
    display.setCursor(0, 16);
    display.printf("LED2 : %s\n", led2 ? "ON" : "OFF");
    display.printf("Light: %d (%d%%)\n", light, light * 100 / 4095);
    if (remain > 0) display.printf("LED3 : %.1f s\n", remain / 1000.0);
    else            display.println("LED3 : OFF");
    display.display();

    // Moi 10 s in muc stack con trong cua cac task (minh chung cho report)
    if (++cycle % 20 == 0) {
      Serial.printf("[T6] Stack free (bytes) T1=%u T2=%u T3=%u T4=%u T5=%u T6=%u | Heap=%u\n",
                    uxTaskGetStackHighWaterMark(hT1), uxTaskGetStackHighWaterMark(hT2),
                    uxTaskGetStackHighWaterMark(hT3), uxTaskGetStackHighWaterMark(hT4),
                    uxTaskGetStackHighWaterMark(hT5), uxTaskGetStackHighWaterMark(hT6),
                    xPortGetFreeHeapSize());
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init FAILED - kiem tra day noi / dia chi I2C");
  }
  display.clearDisplay();
  display.display();

  // Tao doi tuong dong bo truoc khi tao task
  g_dataMutex = xSemaphoreCreateMutex();
  g_btnSem    = xSemaphoreCreateBinary();
  if (g_dataMutex == NULL || g_btnSem == NULL) {
    Serial.println("Khong du heap de tao semaphore!");
    while (true) { delay(1000); }
  }

  xTaskCreatePinnedToCore(Task1_Blink,   "T1_Blink",   STACK_SMALL, NULL, PRIO_T1, &hT1, APP_CORE);
  xTaskCreatePinnedToCore(Task2_Sensor,  "T2_Sensor",  STACK_SMALL, NULL, PRIO_T2, &hT2, APP_CORE);
  xTaskCreatePinnedToCore(Task3_Button1, "T3_Btn1",    STACK_SMALL, NULL, PRIO_T3, &hT3, APP_CORE);
  xTaskCreatePinnedToCore(Task4_Button2, "T4_Btn2",    STACK_SMALL, NULL, PRIO_T4, &hT4, APP_CORE);
  xTaskCreatePinnedToCore(Task5_Led3,    "T5_Led3",    STACK_SMALL, NULL, PRIO_T5, &hT5, APP_CORE);
  xTaskCreatePinnedToCore(Task6_Display, "T6_Display", STACK_OLED,  NULL, PRIO_T6, &hT6, APP_CORE);

  Serial.println("Da tao 6 task. Scheduler dang chay.");
}

// ===================== LOOP =====================
// Arduino-ESP32 da khoi dong scheduler san; loop() cung chi la mot task (loopTask).
// Khong can dung nua nen xoa no di de giai phong tai nguyen.
void loop() {
  vTaskDelete(NULL);
}
