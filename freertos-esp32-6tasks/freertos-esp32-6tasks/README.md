# FreeRTOS trên ESP32 – 6 Task

Project cá nhân môn **Hệ điều hành thời gian thực (RTOS)** – HCMUTE, HK2 2025–2026.

Hệ thống gồm 6 task FreeRTOS chạy song song trên ESP32, điều khiển LED, nút nhấn, cảm biến ánh sáng và màn hình OLED. Các kỹ thuật được minh họa: tạo task, lập lịch theo độ ưu tiên, `vTaskDelay` / `vTaskDelayUntil`, binary semaphore, mutex và giám sát stack/heap.

## Chức năng các task

| Task | Chức năng | Chu kỳ / kích hoạt | Ưu tiên | Cơ chế FreeRTOS |
| --- | --- | --- | --- | --- |
| T1_Blink | LED1 nhấp nháy, T = 200 ms | 100 ms/lần đảo | 1 | `vTaskDelayUntil` |
| T2_Sensor | Đọc cảm biến ánh sáng, lưu vào biến toàn cục | 500 ms | 2 | `vTaskDelayUntil`, mutex |
| T3_Btn1 | Button1 đảo trạng thái LED2 | Quét 10 ms | 2 | `vTaskDelay`, mutex |
| T4_Btn2 | Nhấn Button2 để cấp binary semaphore | Quét 10 ms | 2 | `xSemaphoreGive` |
| T5_Led3 | Chờ semaphore, bật LED3 trong 3 s rồi tắt | Sự kiện | 3 | `xSemaphoreTake`, mutex |
| T6_Display | Hiển thị LED2, ánh sáng, đếm ngược LED3 lên OLED | 500 ms | 1 | `vTaskDelayUntil`, mutex |

Tất cả task được ghim vào **Core 1** (`xTaskCreatePinnedToCore`). Core 0 để dành cho WiFi/Bluetooth.

## Phần cứng

- ESP32 DevKit V1 (board `esp32dev`)
- 3 LED + 3 điện trở 220 Ω
- 2 nút nhấn
- Quang trở (LDR) + điện trở 10 kΩ
- OLED SSD1306 128×64, giao tiếp I²C
- Breadboard, dây nối

## Sơ đồ nối dây

| Linh kiện | Chân ESP32 | Ghi chú |
| --- | --- | --- |
| LED1 | GPIO25 | Nối tiếp 220 Ω xuống GND |
| LED2 | GPIO26 | Nối tiếp 220 Ω xuống GND |
| LED3 | GPIO27 | Nối tiếp 220 Ω xuống GND |
| Button1 | GPIO32 | Một chân nối GND, dùng `INPUT_PULLUP` |
| Button2 | GPIO33 | Một chân nối GND, dùng `INPUT_PULLUP` |
| LDR | GPIO34 (ADC1) | 3.3V – LDR – GPIO34 – 10 kΩ – GND |
| OLED SDA | GPIO21 | |
| OLED SCL | GPIO22 | |
| OLED VCC / GND | 3.3V / GND | Địa chỉ I²C 0x3C |

Ảnh mạch thật đặt trong `docs/images/`.

## Cấu trúc thư mục

```
freertos-esp32-6tasks/
├── platformio.ini      # Cấu hình board, framework, thư viện
├── src/
│   └── main.cpp        # Toàn bộ chương trình 6 task
├── include/            # Header dùng chung (hiện chưa dùng)
├── lib/                # Thư viện riêng của project (hiện chưa dùng)
├── docs/
│   ├── wiring.md       # Chi tiết nối dây
│   └── images/         # Ảnh mạch, ảnh OLED, log Serial
└── .vscode/
    └── extensions.json # Gợi ý cài extension PlatformIO
```

## Build và nạp chương trình

1. Cài **VS Code** và extension **PlatformIO IDE**.
2. Clone repo:
   ```bash
   git clone https://github.com/<username>/freertos-esp32-6tasks.git
   ```
3. Mở thư mục project trong VS Code. PlatformIO tự tải platform `espressif32` và thư viện Adafruit SSD1306, Adafruit GFX.
4. Cắm ESP32, bấm **Build** (✓) rồi **Upload** (→) trên thanh trạng thái.
5. Mở **Serial Monitor** ở baudrate 115200 để xem log.

Hoặc dùng dòng lệnh:

```bash
pio run -t upload
pio device monitor
```

## Log Serial mẫu

```
Da tao 6 task. Scheduler dang chay.
[T3] LED2 -> ON
[T4] Give BinarySemaphore
[T5] Take BinarySemaphore -> LED3 ON 3s
[T6] Stack free (bytes) T1=... T2=... T3=... T4=... T5=... T6=... | Heap=...
```

Log stack/heap được in mỗi 10 s, dùng làm minh chứng khi chọn kích thước stack cho từng task.

## Ghi chú thiết kế

- Ba biến dùng chung (`g_lightRaw`, `g_led2On`, `g_led3RemainMs`) được bảo vệ bởi `g_dataMutex`. Task6 sao chép dữ liệu ra biến cục bộ rồi mới vẽ OLED để giữ mutex ngắn nhất.
- Nút nhấn được chống dội bằng phần mềm (30 ms), dựa trên tick của FreeRTOS.
- Lượt nhấn Button2 trong lúc LED3 đang sáng sẽ bị bỏ qua (`xSemaphoreTake(g_btnSem, 0)` cuối vòng lặp của Task5).
- Trên ESP32, tham số stack của `xTaskCreate` tính bằng **byte**, không phải word như FreeRTOS gốc.
- `loop()` gọi `vTaskDelete(NULL)` để xóa task `loopTask` của Arduino vì không dùng tới.

## Hướng mở rộng

- Thay biến toàn cục bằng **Queue** giữa T2 và T6.
- Dùng **ngắt + Task Notification** cho Button2 thay vì polling.
- Dùng **Software Timer** cho LED1.
- Dùng **Event Group**: LED3 chỉ sáng khi nhấn Button2 và trời tối.
- In thống kê CPU bằng `vTaskGetRunTimeStats`.

## Tài liệu tham khảo

- Richard Barry, *Mastering the FreeRTOS Real Time Kernel* – freertos.org
- FreeRTOS API Reference – freertos.org
- Espressif, ESP-IDF Programming Guide – mục FreeRTOS
- Adafruit SSD1306 / Adafruit GFX Library

## Tác giả

- Họ tên: `<Họ và tên>`
- MSSV: `<MSSV>`
- GVHD: Huỳnh Hoàng Hà
