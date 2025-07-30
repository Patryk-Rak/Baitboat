// ============================================================================
// Nagłówki bibliotek
// ============================================================================
#include <ui.h>
#include <ui_events.h>
#include <ui_helpers.h>
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Wire.h>
#include <NintendoExtensionCtrl.h>
#include <esp_now.h>
#include <WiFi.h>
#include "lvgl.h"

// ============================================================================
// Definicje stałych i zmiennych globalnych
// ============================================================================
static const uint16_t SCREEN_WIDTH  = 320;    // Szerokość ekranu TFT
static const uint16_t SCREEN_HEIGHT = 240;    // Wysokość ekranu TFT
static const uint16_t SCREENBUFFER_SIZE_PIXELS = SCREEN_WIDTH * SCREEN_HEIGHT / 10; // Bufor LVGL (1/10 ekranu)
static lv_color_t screen_buffer[SCREENBUFFER_SIZE_PIXELS]; // Bufor ekranu dla LVGL

const int JOY_MIN_X = 0, JOY_MAX_X = 255, JOY_MIN_Y = 0, JOY_MAX_Y = 255; // Zakresy joysticka
const int JOY_DEADZONE_LOW = 5, JOY_DEADZONE_HIGH = 5; // Martwa strefa joysticka

static int progress_value = 0; // Postęp ładowania
static bool is_loading = true, was_connected = false; // Statusy ładowania i połączenia
static int joy_x = 0, joy_y = 0; // Odczyty joysticka
static bool trigger = false; // Stan przycisku Z
static lv_obj_t* popup = nullptr; // Popup na ekranie

static lv_timer_t *bar_timer = nullptr, *connection_timer = nullptr; // Timery LVGL

// Struktura do przesyłania danych przez ESP-NOW
typedef struct {
    int x;
    int y;
    bool trigger;
} struct_message;

struct_message data; // Dane do wysyłki
uint8_t receiverAddress[] = {0xA8, 0x48, 0xFA, 0x6B, 0xB4, 0xAC}; // Adres odbiorcy ESP-NOW

// Obiekty sprzętowe
TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT); // Ekran TFT
#define TOUCH_IRQ_PIN  36
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25
#define TOUCH_CS_PIN   33
SPIClass touch_spi(VSPI); // SPI dla dotyku
XPT2046_Touchscreen touch_screen(TOUCH_CS_PIN, TOUCH_IRQ_PIN); // Sterownik dotyku
uint16_t touch_min_x = 400, touch_max_x = 3600, touch_min_y = 300, touch_max_y = 3700; // Kalibracja dotyku

#define NUNCHUK_SDA_PIN 22
#define NUNCHUK_SCL_PIN 27
Nunchuk nunchuk; // Kontroler Nunchuk

// ============================================================================
// Funkcje pomocnicze LVGL
// ============================================================================

// Funkcja logowania LVGL do Serial
void log_print(lv_log_level_t, const char* buf) {
    Serial.println(buf);
    Serial.flush();
}

// Funkcja odświeżania ekranu LVGL (flush)
void my_display_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixelmap) {
    uint32_t width  = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, width, height);
    tft.pushColors((uint16_t*)pixelmap, width * height, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// Odczyt dotyku dla LVGL
void my_touch_read(lv_indev_t*, lv_indev_data_t* data) {
    if (touch_screen.touched()) {
        TS_Point point = touch_screen.getPoint();
        touch_min_x = min(touch_min_x, (uint16_t)point.x);
        touch_max_x = max(touch_max_x, (uint16_t)point.x);
        touch_min_y = min(touch_min_y, (uint16_t)point.y);
        touch_max_y = max(touch_max_y, (uint16_t)point.y);
        data->point.x = map(point.x, touch_min_x, touch_max_x, 1, SCREEN_WIDTH);
        data->point.y = map(point.y, touch_min_y, touch_max_y, 1, SCREEN_HEIGHT);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Funkcja zwracająca tick dla LVGL
static uint32_t my_tick_get_cb(void) {
    return millis();
}

// ============================================================================
// Funkcje ESP-NOW
// ============================================================================

// Callback po wysłaniu pakietu ESP-NOW
void OnDataSent(const uint8_t*, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// ============================================================================
// Funkcje interfejsu użytkownika
// ============================================================================

// Animacja ładowania ekranu
static void loading_screen(lv_timer_t*) {
    lv_bar_set_range(ui_LoadingBar, 0, 100);
    progress_value += random(0, 8);
    lv_bar_set_value(ui_LoadingBar, progress_value, LV_ANIM_ON);
    if (progress_value >= 100) {
        lv_timer_del(bar_timer);
        bar_timer = nullptr;
        is_loading = false;
        bool is_connected = nunchuk.connect();
        was_connected = is_connected;
        if (is_connected) {
            Serial.println("Loading complete, Nunchuk connected, switching to ui_Menu");
            _ui_screen_change(&ui_Menu, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, &ui_Menu_screen_init);
        } else {
            Serial.println("Loading complete, Nunchuk disconnected, switching to ui_Connect");
            _ui_screen_change(&ui_Connect, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, &ui_Connect_screen_init);
            unplugged_Animation(ui_Unplugged, 0);
        }
    }
}

// Sprawdzanie połączenia z kontrolerem
static void check_connection(lv_timer_t*) {
    if (is_loading) return;
    bool is_connected = nunchuk.connect();
    if (is_connected && !was_connected) {
        Serial.println("Nunchuk connected, switching to ui_Menu");
        _ui_screen_change(&ui_Menu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_Menu_screen_init);
        unplugged_Animation(ui_Unplugged, 0);
    } else if (!is_connected && was_connected) {
        Serial.println("Nunchuk disconnected, switching to ui_Connect");
        _ui_screen_change(&ui_Connect, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_Connect_screen_init);
        unplugged_Animation(ui_Unplugged, 0);
    }
    was_connected = is_connected;
}

// Aktualizacja pasków prędkości i wysyłka danych przez ESP-NOW
static void update_speed_values(lv_timer_t*) {
    if (!nunchuk.connect()) {
        lv_label_set_text(ui_BatteryText, "N/A");
        lv_bar_set_value(ui_SpeedBarUp, 0, LV_ANIM_ON);
        lv_bar_set_value(ui_SpeedBarDown, 0, LV_ANIM_ON);
        lv_bar_set_value(ui_SpeedBarLeft, 0, LV_ANIM_ON);
        lv_bar_set_value(ui_SpeedBarRight, 0, LV_ANIM_ON);
        return;
    }

    nunchuk.update();
    joy_x = nunchuk.joyX();
    joy_y = nunchuk.joyY();
    trigger = nunchuk.buttonZ();
    Serial.printf("Raw joyX: %d\nRaw joyY: %d\nTrigger: %s\n", joy_x, joy_y, trigger ? "Pressed" : "Released");

    // Mapowanie wartości joysticka na zakres 0-100
    int mapped_value_x = (joy_x < JOY_MIN_X + JOY_DEADZONE_LOW) ? 0 :
                         (joy_x > JOY_MAX_X - JOY_DEADZONE_HIGH) ? 100 :
                         map(joy_x, JOY_MIN_X + JOY_DEADZONE_LOW, JOY_MAX_X - JOY_DEADZONE_HIGH, 0, 100);
    mapped_value_x = constrain(mapped_value_x, 0, 100);

    int mapped_value_y = (joy_y < JOY_MIN_Y + JOY_DEADZONE_LOW) ? 0 :
                         (joy_y > JOY_MAX_Y - JOY_DEADZONE_HIGH) ? 100 :
                         map(joy_y, JOY_MIN_Y + JOY_DEADZONE_LOW, JOY_MAX_Y - JOY_DEADZONE_HIGH, 0, 100);
    mapped_value_y = constrain(mapped_value_y, 0, 100);

    // Obliczanie wartości pasków
    int up_value    = max(0, mapped_value_y - 50) * 2;
    int down_value  = (50 - min(mapped_value_y, 50)) * 2;
    int left_value  = (50 - min(mapped_value_x, 50)) * 2;
    int right_value = max(0, mapped_value_x - 50) * 2;

    // Aktualizacja pasków prędkości
    lv_bar_set_value(ui_SpeedBarUp, up_value, LV_ANIM_ON);
    lv_bar_set_value(ui_SpeedBarDown, down_value, LV_ANIM_ON);
    lv_bar_set_value(ui_SpeedBarLeft, left_value, LV_ANIM_ON);
    lv_bar_set_value(ui_SpeedBarRight, right_value, LV_ANIM_ON);

    // Wysyłanie danych przez ESP-NOW
    data.x = joy_x;
    data.y = joy_y;
    data.trigger = trigger;
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t*)&data, sizeof(data));
    Serial.println(result == ESP_OK ? "Sent with success" : "Error sending the data");

    // Obsługa popupu po naciśnięciu triggera
    if (trigger && lv_scr_act() == ui_Menu && !popup) {
        popup = lv_obj_create(ui_Control);
        lv_obj_set_size(popup, 200, 100);
        lv_obj_align(popup, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(popup, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(popup, LV_OPA_80, LV_PART_MAIN);
        lv_obj_t* label = lv_label_create(popup);
        lv_label_set_text(label, "Trigger pressed");
        lv_obj_center(label);
        lv_timer_t* popup_timer = lv_timer_create([](lv_timer_t* timer) {
            lv_obj_del(popup);
            popup = nullptr;
            lv_timer_del(timer);
        }, 2000, nullptr);
    }
}

// ============================================================================
// Funkcja inicjalizacyjna programu
// ============================================================================
void setup() {
    Serial.begin(115200);
    Serial.printf("LVGL Library Version: %d.%d.%d\n", lv_version_major(), lv_version_minor(), lv_version_patch());

    // Inicjalizacja ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);

    // Dodanie odbiorcy ESP-NOW
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    // Inicjalizacja kontrolera Nunchuk
    Wire.begin(NUNCHUK_SDA_PIN, NUNCHUK_SCL_PIN);
    nunchuk.begin();
    was_connected = nunchuk.connect();
    Serial.println(was_connected ? "Nunchuk connected successfully" : "Failed to connect to Nunchuk");

    // Inicjalizacja ekranu TFT
    tft.begin();
    tft.fillScreen(TFT_BLACK);
    delay(10);
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    delay(10);

    // Inicjalizacja dotyku
    touch_spi.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    touch_screen.begin(touch_spi);
    touch_screen.setRotation(1);

    // Inicjalizacja LVGL
    lv_init();
    lv_log_register_print_cb(log_print);

    static lv_disp_t* display;
    display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_buffers(display, screen_buffer, nullptr, SCREENBUFFER_SIZE_PIXELS * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, my_display_flush);

    lv_indev_t* touch_input = lv_indev_create();
    lv_indev_set_type(touch_input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_input, my_touch_read);

    lv_tick_set_cb(my_tick_get_cb);

    // Inicjalizacja UI i timerów
    ui_init();
    lv_timer_handler();

    bar_timer = lv_timer_create(loading_screen, 100, nullptr);         // Timer ładowania
    connection_timer = lv_timer_create(check_connection, 500, nullptr); // Timer sprawdzania połączenia
    lv_timer_t* speed_timer = lv_timer_create(update_speed_values, 100, nullptr); // Timer pasków prędkości

    Serial.println("Setup done");
}

// ============================================================================
// Główna pętla programu
// ============================================================================
void loop() {
    lv_timer_handler(); // Obsługa timerów LVGL
    delay(5);           // Krótka pauza dla stabilności
}