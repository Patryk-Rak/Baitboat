// ============================================================================
// Nagłówki bibliotek
// ============================================================================

#include <esp_now.h>
#include <WiFi.h>

// ============================================================================
// Definicje stałych i zmiennych globalnych
// ============================================================================

// Struktura danych do odbierania przez ESP-NOW
typedef struct struct_message {
    int x;
    int y;
    bool trigger;
} struct_message;

// Utworzenie zmiennej dla danych
struct_message receivedData;

// ============================================================================
// Funkcje ESP-NOW
// ============================================================================

/**
 * Callback wywoływany po odebraniu danych
 * @param mac_addr Adres MAC nadajnika
 * @param data Wskaźnik na odebrane dane
 * @param len Długość danych
 */
void OnDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (len == sizeof(struct_message)) {
        memcpy(&receivedData, data, sizeof(receivedData));
        Serial.print("Received: x = ");
        Serial.print(receivedData.x);
        Serial.print(", y = ");
        Serial.print(receivedData.y);
        Serial.print(", trigger = ");
        Serial.println(receivedData.trigger ? "true" : "false");
    }
}

// ============================================================================
// Funkcje główne Arduino
// ============================================================================

/**
 * Funkcja inicjalizacyjna programu
 */
void setup() {
    Serial.begin(115200);  // Inicjalizacja komunikacji szeregowej

    // Inicjalizacja Wi-Fi i ESP-NOW
    WiFi.mode(WIFI_STA);  // Ustawienie trybu stacji
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);  // Rejestracja callbacku odbioru

    Serial.println("Receiver Ready");
}

/**
 * Główna pętla programu
 */
void loop() {
    delay(10);  // Krótkie opóźnienie dla stabilności
}