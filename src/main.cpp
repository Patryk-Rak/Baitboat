// ============================================================================
// Nagłówki bibliotek
// ============================================================================

// Nagłówki bibliotek interfejsu użytkownika
#include <ui.h>
#include <ui_events.h>
#include <ui_helpers.h>

// Nagłówki bibliotek Arduino i peryferiów
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Wire.h>
#include <NintendoExtensionCtrl.h>
#include <esp_now.h>
#include <WiFi.h>

// Nagłówek biblioteki LVGL
#include "lvgl.h"

// ============================================================================
// Definicje stałych i zmiennych globalnych
// ============================================================================

// Wymiary ekranu
static const uint16_t SCREEN_WIDTH  = 320;  // Szerokość ekranu w pikselach
static const uint16_t SCREEN_HEIGHT = 240;  // Wysokość ekranu w pikselach

// Rozmiar bufora LVGL (1/10 powierzchni ekranu)
enum { SCREENBUFFER_SIZE_PIXELS = SCREEN_WIDTH * SCREEN_HEIGHT / 10 };
static lv_color_t screen_buffer[SCREENBUFFER_SIZE_PIXELS];  // Bufor pikseli dla LVGL

// Zakres wartości joysticka Nunchuka
const int JOY_MIN_X      = 0;   // Minimalna wartość osi X joysticka
const int JOY_MAX_X      = 255; // Maksymalna wartość osi X joysticka
const int JOY_MIN_Y      = 0;   // Minimalna wartość osi Y joysticka
const int JOY_MAX_Y      = 255; // Maksymalna wartość osi Y joysticka
const int JOY_DEADZONE_LOW  = 5;    // Dolny próg martwej strefy joysticka
const int JOY_DEADZONE_HIGH = 5;    // Górny próg martwej strefy joysticka

// Zmienne stanu programu
static int progress_value = 0;           // Wartość paska postępu na ekranie ładowania
static bool is_loading    = true;        // Flaga wskazująca, czy trwa ekran ładowania
static bool was_connected = false;       // Flaga śledząca poprzedni stan połączenia Nunchuka
static int joy_x          = 0;           // Wartość osi X joysticka
static int joy_y          = 0;           // Wartość osi Y joysticka
static bool trigger       = false;       // Stan przycisku trigger (buttonZ)
static lv_obj_t* popup    = nullptr;     // Wskaźnik na obiekt popup

// Obiekty i timery LVGL
static lv_timer_t* bar_timer       = nullptr;  // Timer dla paska postępu
static lv_timer_t* connection_timer = nullptr; // Timer sprawdzania połączenia Nunchuka
static lv_timer_t* espnow_timer    = nullptr;  // Timer dla wysyłania danych ESP-NOW

// Struktura danych do wysyłania przez ESP-NOW
typedef struct struct_message {
    int x;
    int y;
    bool trigger;  // Dodanie stanu trigger
} struct_message;

// Utworzenie zmiennej dla danych
struct_message data;

// Adres MAC odbiornika (zmień na rzeczywisty adres MAC drugiego ESP32)
uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Placeholder, zastąp rzeczywistym adresem

// ============================================================================
// Konfiguracja sprzętowa
// ============================================================================

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);  // Inicjalizacja wyświetlacza TFT

// Konfiguracja ekranu dotykowego (XPT2046)
#define TOUCH_IRQ_PIN  36  // Pin przerwania dla ekranu dotykowego
#define TOUCH_MOSI_PIN 32  // Pin MOSI dla ekranu dotykowego
#define TOUCH_MISO_PIN 39  // Pin MISO dla ekranu dotykowego
#define TOUCH_CLK_PIN  25  // Pin zegarowy dla ekranu dotykowego
#define TOUCH_CS_PIN   33  // Pin CS dla ekranu dotykowego
SPIClass touch_spi(VSPI);  // Inicjalizacja interfejsu SPI dla ekranu dotykowego
XPT2046_Touchscreen touch_screen(TOUCH_CS_PIN, TOUCH_IRQ_PIN);  // Inicjalizacja kontrolera dotyku
uint16_t touch_min_x = 400, touch_max_x = 3600;  // Zakres wartości X dla kalibracji dotyku
uint16_t touch_min_y = 300, touch_max_y = 3700;  // Zakres wartości Y dla kalibracji dotyku

// Konfiguracja Nunchuka (I2C)
#define NUNCHUK_SDA_PIN 22  // Pin SDA dla Nunchuka
#define NUNCHUK_SCL_PIN 27  // Pin SCL dla Nunchuka
Nunchuk nunchuk;  // Inicjalizacja kontrolera Nunchuka

// ============================================================================
// Funkcje pomocnicze LVGL
// ============================================================================

/**
 * Funkcja logowania komunikatów LVGL do portu szeregowego
 * @param level Poziom logowania (niewykorzystany)
 * @param buf Treść komunikatu do wyświetlenia
 */
void log_print(lv_log_level_t level, const char* buf) {
    LV_UNUSED(level);  // Pomijanie parametru level
    Serial.println(buf);  // Wyświetlanie komunikatu
    Serial.flush();       // Oczekiwanie na zakończenie transmisji
}

/**
 * Funkcja flush do renderowania obrazu na wyświetlaczu TFT
 * @param disp Wskaźnik na obiekt wyświetlacza LVGL
 * @param area Obszar do zaktualizowania
 * @param pixelmap Bufor pikseli do wyświetlenia
 */
void my_display_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixelmap) {
    uint32_t width  = area->x2 - area->x1 + 1;  // Obliczenie szerokości obszaru
    uint32_t height = area->y2 - area->y1 + 1;  // Obliczenie wysokości obszaru
    tft.startWrite();  // Rozpoczęcie operacji zapisu
    tft.setAddrWindow(area->x1, area->y1, width, height);  // Ustawienie okna adresowego
    tft.pushColors((uint16_t*)pixelmap, width * height, true);  // Wysłanie pikseli
    tft.endWrite();  // Zakończenie operacji zapisu
    lv_disp_flush_ready(disp);  // Powiadomienie LVGL o zakończeniu flush
}

/**
 * Funkcja odczytu danych z ekranu dotykowego
 * @param indev_drv Wskaźnik na urządzenie wejściowe LVGL
 * @param data Struktura danych wejściowych
 */
void my_touch_read(lv_indev_t* indev_drv, lv_indev_data_t* data) {
    if (touch_screen.touched()) {  // Sprawdzenie, czy ekran jest dotykany
        TS_Point point = touch_screen.getPoint();  // Pobranie pozycji dotyku
        touch_min_x = min(touch_min_x, (uint16_t)point.x);  // Aktualizacja minimalnej wartości X
        touch_max_x = max(touch_max_x, (uint16_t)point.x);  // Aktualizacja maksymalnej wartości X
        touch_min_y = min(touch_min_y, (uint16_t)point.y);  // Aktualizacja minimalnej wartości Y
        touch_max_y = max(touch_max_y, (uint16_t)point.y);  // Aktualizacja maksymalnej wartości Y
        data->point.x = map(point.x, touch_min_x, touch_max_x, 1, SCREEN_WIDTH);  // Mapowanie X
        data->point.y = map(point.y, touch_min_y, touch_max_y, 1, SCREEN_HEIGHT); // Mapowanie Y
        data->state = LV_INDEV_STATE_PR;  // Ustawienie stanu dotyku
    } else {
        data->state = LV_INDEV_STATE_REL;  // Ustawienie stanu braku dotyku
    }
}

/**
 * Funkcja zwracająca aktualny czas w milisekundach
 * @return Aktualny czas systemowy
 */
static uint32_t my_tick_get_cb(void) {
    return millis();  // Zwracanie czasu od startu programu
}

// ============================================================================
// Funkcje ESP-NOW
// ============================================================================

/**
 * Callback wywoływany po wysłaniu danych
 * @param mac_addr Adres MAC odbiornika
 * @param status Status wysłania (0 = sukces)
 */
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/**
 * Funkcja wysyłająca dane joysticka przez ESP-NOW
 * @param timer Wskaźnik na timer LVGL
 */
static void send_espnow_data(lv_timer_t* timer) {
    if (!nunchuk.connect()) return;  // Pominiecie, jeśli brak połączenia z Nunchukiem

    nunchuk.update();  // Aktualizacja danych z Nunchuka
    joy_x = nunchuk.joyX();  // Pobranie wartości osi X
    joy_y = nunchuk.joyY();  // Pobranie wartości osi Y
    trigger = nunchuk.buttonZ();  // Pobranie stanu przycisku trigger (buttonZ)

    data.x = joy_x;      // Przypisanie wartości do struktury
    data.y = joy_y;
    data.trigger = trigger;

    // Wysłanie danych przez ESP-NOW
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t*)&data, sizeof(data));
    if (result == ESP_OK) {
        Serial.println("Sent with success");
    } else {
        Serial.println("Error sending the data");
    }
}

// ============================================================================
// Funkcje interfejsu użytkownika
// ============================================================================

/**
 * Funkcja aktualizująca pasek postępu na ekranie ładowania
 * @param timer Wskaźnik na timer LVGL
 */
static void set_value_task(lv_timer_t* timer) {
    lv_bar_set_range(ui_LoadingBar, 0, 100);  // Ustawienie zakresu paska postępu
    progress_value += random(0, 8);           // Zwiększenie wartości postępu losowo
    lv_bar_set_value(ui_LoadingBar, progress_value, LV_ANIM_ON);  // Aktualizacja paska z animacją
    if (progress_value >= 100) {              // Sprawdzenie zakończenia ładowania
        lv_timer_del(bar_timer);              // Usunięcie timera
        bar_timer = nullptr;                  // Wyczyszczenie wskaźnika
        is_loading = false;                   // Wyłączenie flagi ładowania
        bool is_connected = nunchuk.connect(); // Sprawdzenie połączenia z Nunchukiem
        was_connected = is_connected;         // Aktualizacja stanu poprzedniego połączenia
        if (is_connected) {                   // Jeśli Nunchuk jest podłączony
            Serial.println("Loading complete, Nunchuk connected, switching to ui_Menu");
            _ui_screen_change(&ui_Menu, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_Menu_screen_init);
        } else {                              // Jeśli Nunchuk jest odłączony
            Serial.println("Loading complete, Nunchuk disconnected, switching to ui_Connect");
            _ui_screen_change(&ui_Connect, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_Connect_screen_init);
            unplugged_Animation(ui_Unplugged, 0); // Uruchomienie animacji rozłączenia
        }
    }
}

/**
 * Funkcja sprawdzająca połączenie z Nunchukiem i przełączająca ekrany
 * @param timer Wskaźnik na timer LVGL
 */
static void check_connection(lv_timer_t* timer) {
    if (is_loading) return;  // Pominiecie, jeśli trwa ładowanie
    bool is_connected = nunchuk.connect();  // Sprawdzenie połączenia
    if (is_connected && !was_connected) {   // Jeśli połączenie nawiązano
        Serial.println("Nunchuk connected, switching to ui_Menu");
        _ui_screen_change(&ui_Menu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_Menu_screen_init);
        unplugged_Animation(ui_Unplugged, 0); // Wyłączenie animacji rozłączenia
    } else if (!is_connected && was_connected) {  // Jeśli połączenie utracono
        Serial.println("Nunchuk disconnected, switching to ui_Connect");
        _ui_screen_change(&ui_Connect, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_Connect_screen_init);
        unplugged_Animation(ui_Unplugged, 0); // Uruchomienie animacji rozłączenia
    }
    was_connected = is_connected;  // Aktualizacja stanu połączenia
}

/**
 * Funkcja aktualizująca wartości pasków prędkości na podstawie danych joysticka
 * @param timer Wskaźnik na timer LVGL
 */
static void update_speed_bars(lv_timer_t* timer) {
    if (!nunchuk.connect()) {  // Sprawdzenie połączenia z Nunchukiem
        lv_label_set_text(ui_BatteryText, "N/A");  // Wyświetlenie "N/A" przy braku połączenia
        lv_bar_set_value(ui_SpeedBarUp, 0, LV_ANIM_ON);    // Reset pasków
        lv_bar_set_value(ui_SpeedBarDown, 0, LV_ANIM_ON);
        lv_bar_set_value(ui_SpeedBarLeft, 0, LV_ANIM_ON);
        lv_bar_set_value(ui_SpeedBarRight, 0, LV_ANIM_ON);
        return;  // Wyjście, jeśli brak połączenia
    }

    nunchuk.update();  // Aktualizacja danych z Nunchuka
    joy_x = nunchuk.joyX();  // Pobranie wartości osi X
    joy_y = nunchuk.joyY();  // Pobranie wartości osi Y
    trigger = nunchuk.buttonZ();  // Pobranie stanu przycisku trigger (buttonZ)
    Serial.print("Raw joyX: "); Serial.println(joy_x);  // Wyświetlanie surowych danych
    Serial.print("Raw joyY: "); Serial.println(joy_y);
    Serial.print("Trigger: "); Serial.println(trigger ? "Pressed" : "Released");

    // Mapowanie wartości joysticka X na zakres 0–100
    int mapped_value_x = 0;
    if (joy_x < JOY_MIN_X + JOY_DEADZONE_LOW) mapped_value_x = 0;
    else if (joy_x > JOY_MAX_X - JOY_DEADZONE_HIGH) mapped_value_x = 100;
    else mapped_value_x = map(joy_x, JOY_MIN_X + JOY_DEADZONE_LOW, JOY_MAX_X - JOY_DEADZONE_HIGH, 0, 100);
    mapped_value_x = constrain(mapped_value_x, 0, 100);

    // Mapowanie wartości joysticka Y na zakres 0–100
    int mapped_value_y = 0;
    if (joy_y < JOY_MIN_Y + JOY_DEADZONE_LOW) mapped_value_y = 0;
    else if (joy_y > JOY_MAX_Y - JOY_DEADZONE_HIGH) mapped_value_y = 100;
    else mapped_value_y = map(joy_y, JOY_MIN_Y + JOY_DEADZONE_LOW, JOY_MAX_Y - JOY_DEADZONE_HIGH, 0, 100);
    mapped_value_y = constrain(mapped_value_y, 0, 100);

    // Obliczanie wartości dla pasków
    int up_value    = max(0, mapped_value_y - 50) * 2;  // Przód (joyY >= 50%)
    int down_value  = (50 - min(mapped_value_y, 50)) * 2;  // Tył (joyY < 50%)
    int left_value  = (50 - min(mapped_value_x, 50)) * 2;  // Lewo (joyX < 50%)
    int right_value = max(0, mapped_value_x - 50) * 2;     // Prawo (joyX >= 50%)

    lv_bar_set_value(ui_SpeedBarUp, up_value, LV_ANIM_ON);    // Aktualizacja paska przód
    lv_bar_set_value(ui_SpeedBarDown, down_value, LV_ANIM_ON);  // Aktualizacja paska tył
    lv_bar_set_value(ui_SpeedBarLeft, left_value, LV_ANIM_ON);  // Aktualizacja paska lewo
    lv_bar_set_value(ui_SpeedBarRight, right_value, LV_ANIM_ON); // Aktualizacja paska prawo

    // Aktualizacja etykiety baterii (symulacja poziomu baterii)
    char batteryText[8];
    sprintf(batteryText, "%d%%", random(0, 100));
    lv_label_set_text(ui_BatteryText, batteryText);
																										  
 

    // Obsługa triggera na ekranie Menu
    if (trigger && lv_scr_act() == ui_Menu) {
        if (!popup) {
            popup = lv_obj_create(ui_Control);  // Tworzenie popupu
            lv_obj_set_size(popup, 200, 100);    // Ustawienie rozmiaru
            lv_obj_align(popup, LV_ALIGN_CENTER, 0, 0);  // Wyśrodkowanie
            lv_obj_set_style_bg_color(popup, lv_color_hex(0x000000), LV_PART_MAIN);  // Czarne tło
            lv_obj_set_style_bg_opa(popup, LV_OPA_80, LV_PART_MAIN);  // Przejrzystość
            lv_obj_t* label = lv_label_create(popup);  // Tworzenie etykiety
            lv_label_set_text(label, "Trigger pressed");  // Ustawienie tekstu
            lv_obj_center(label);  // Wyśrodkowanie etykiety
            lv_timer_t* popup_timer = lv_timer_create([](lv_timer_t* timer) {
                lv_obj_del(popup);  // Usunięcie popupu po 5 sekundach
                popup = nullptr;
                lv_timer_del(timer);  // Usunięcie timera
            }, 2000, nullptr);  // Timer na 5 sekund
        }
    }
}

/**
 * Funkcja inicjalizacyjna programu
 */
void setup() {
    Serial.begin(115200);  // Inicjalizacja komunikacji szeregowej
    String lvgl_version = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println(lvgl_version);  // Wyświetlanie wersji LVGL

    // Inicjalizacja Wi-Fi i ESP-NOW
    WiFi.mode(WIFI_STA);  // Ustawienie trybu stacji
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);  // Rejestracja callbacku wysyłania

    // Dodanie odbiornika (drugi ESP32)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    Wire.begin(NUNCHUK_SDA_PIN, NUNCHUK_SCL_PIN);  // Inicjalizacja interfejsu I2C
    nunchuk.begin();  // Inicjalizacja Nunchuka
    if (!nunchuk.connect()) {  // Sprawdzenie połączenia z Nunchukiem
        Serial.println("Failed to connect to Nunchuk");
        was_connected = false;
    } else {
        Serial.println("Nunchuk connected successfully");
        was_connected = true;
    }

    tft.begin();  // Inicjalizacja wyświetlacza TFT
    tft.fillScreen(TFT_BLACK);  // Wypełnienie ekranu czarnym kolorem
    delay(10);  // Krótkie opóźnienie
    tft.setRotation(1);  // Ustawienie orientacji ekranu
    tft.fillScreen(TFT_BLACK);  // Ponowne wyczyszczenie ekranu
    delay(10);  // Krótkie opóźnienie

    touch_spi.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);  // Inicjalizacja SPI dla ekranu dotykowego
    touch_screen.begin(touch_spi);  // Inicjalizacja kontrolera dotyku
    touch_screen.setRotation(1);  // Ustawienie orientacji ekranu dotykowego

    lv_init();  // Inicjalizacja biblioteki LVGL
    lv_log_register_print_cb(log_print);  // Rejestracja funkcji logowania

    static lv_disp_t* display;
    display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);  // Tworzenie wyświetlacza LVGL
    lv_display_set_buffers(display, screen_buffer, nullptr, SCREENBUFFER_SIZE_PIXELS * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);  // Ustawienie bufora
    lv_display_set_flush_cb(display, my_display_flush);  // Ustawienie callbacku flush

    lv_indev_t* touch_input = lv_indev_create();  // Tworzenie urządzenia wejściowego
    lv_indev_set_type(touch_input, LV_INDEV_TYPE_POINTER);  // Ustawienie typu na pointer
    lv_indev_set_read_cb(touch_input, my_touch_read);  // Ustawienie callbacku odczytu

    lv_tick_set_cb(my_tick_get_cb);  // Ustawienie callbacku czasu

    ui_init();  // Inicjalizacja wszystkich ekranów UI
    lv_timer_handler();  // Wykonanie początkowej obsługi timerów

    bar_timer = lv_timer_create(set_value_task, 50, nullptr);  // Timer dla paska postępu (50 ms)
    connection_timer = lv_timer_create(check_connection, 500, nullptr);  // Timer dla sprawdzania połączenia (500 ms)
    lv_timer_t* speed_timer = lv_timer_create(update_speed_bars, 50, nullptr);  // Timer dla pasków prędkości (50 ms)
    espnow_timer = lv_timer_create(send_espnow_data, 100, nullptr);  // Timer dla wysyłania danych ESP-NOW (100 ms)

    Serial.println("Setup done");  // Potwierdzenie zakończenia inicjalizacji
}

/**
 * Główna pętla programu
 */
void loop() {
    lv_timer_handler();  // Obsługa timerów LVGL
    delay(5);  // Krótkie opóźnienie dla stabilności
}