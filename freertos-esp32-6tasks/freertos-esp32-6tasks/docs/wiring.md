# Sơ đồ nối dây chi tiết

Board: ESP32 DevKit V1 (38 chân). Mọi linh kiện dùng nguồn 3.3V của board.

## LED

```
GPIO25 ──[220Ω]──▶|── GND    (LED1)
GPIO26 ──[220Ω]──▶|── GND    (LED2)
GPIO27 ──[220Ω]──▶|── GND    (LED3)
```

Chân dài của LED (anode) nối về phía điện trở, chân ngắn (cathode) nối GND.

## Nút nhấn

```
GPIO32 ──[BTN1]── GND
GPIO33 ──[BTN2]── GND
```

Dùng điện trở kéo lên nội (`INPUT_PULLUP`): nhả = HIGH, nhấn = LOW. Không cần điện trở ngoài.

## Cảm biến ánh sáng (LDR)

```
3.3V ──[LDR]──┬── GPIO34
              │
            [10kΩ]
              │
             GND
```

Trời càng sáng, điện trở LDR càng nhỏ, điện áp tại GPIO34 càng lớn, giá trị ADC càng cao (0–4095).
GPIO34 thuộc ADC1, chỉ là chân input, vẫn đọc được khi bật WiFi.

## OLED SSD1306 (I²C)

| OLED | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

Địa chỉ mặc định 0x3C. Nếu màn hình không hiện, thử đổi `OLED_ADDR` thành `0x3D` trong `src/main.cpp`.
