// ============================================================================
// Nagłówki bibliotek
// ============================================================================
#include <esp_now.h>
#include <WiFi.h>

// ============================================================================
// Definicje stałych i zmiennych globalnych
// ============================================================================

// Piny dla sterownika DRI0041 (L298N) na ESP32 DevKitC V4
const int ENA_PIN = 26; // PWM dla silnika 1
const int IN1_PIN = 27; // Kierunek 1 silnika 1
const int IN2_PIN = 14; // Kierunek 2 silnika 1
const int ENB_PIN = 12; // PWM dla silnika 2
const int IN3_PIN = 13; // Kierunek 1 silnika 2
const int IN4_PIN = 15; // Kierunek 2 silnika 2

// Struktura danych do odbierania przez ESP-NOW z atrybutem packed
typedef struct __attribute__((packed)) struct_message {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    bool trigger;
} struct_message;

// Utworzenie zmiennej dla danych
struct_message receivedData;

// Debugowanie rozmiaru struktury
const size_t EXPECTED_SIZE = 5; // Oczekiwany rozmiar: 4 * uint8_t (1 bajt) + 1 * bool (1 bajt)

#ifdef DEBUG
void debugStructSize() {
    Serial.print("sizeof(struct_message): ");
    Serial.println(sizeof(struct_message));
    Serial.print("Expected size: ");
    Serial.println(EXPECTED_SIZE);
}
#endif

// ============================================================================
// Funkcje ESP-NOW
// ============================================================================

/**
 * Funkcja obsługi odebranych danych
 * @param msg Struktura z odebranymi danymi
										 
							   
 */
void handleReceivedData(const struct_message& msg) {
    Serial.print("Received: up = ");
    Serial.print(msg.up);
    Serial.print(", down = ");
    Serial.print(msg.down);
    Serial.print(", left = ");
    Serial.print(msg.left);
    Serial.print(", right = ");
    Serial.print(msg.right);
    Serial.print(", trigger = ");
    Serial.println(msg.trigger ? "true" : "false");

    // Logika sterowania silnikami
    if (msg.trigger) {
        // Zatrzymanie silników przy aktywnym triggerze
        analogWrite(ENA_PIN, 0);
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, LOW);
        analogWrite(ENB_PIN, 0);
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, LOW);
    } else {
        // Obliczenie prędkości bazowej (średnia z up i down)
        int baseSpeed = (msg.up + (255 - msg.down)) / 2; // Skalowanie 0-255
        baseSpeed = constrain(baseSpeed, 0, 255);

        // Obliczenie różnicy dla skrętu (left i right)
        int turnAdjust = ((int)msg.right - (int)msg.left) / 2; // Różnica dla skrętu
        turnAdjust = constrain(turnAdjust, -255, 255);

        // Prędkość dla silnika 1 (lewy)
        int speed1 = baseSpeed + turnAdjust;
        speed1 = constrain(speed1, 0, 255);

        // Prędkość dla silnika 2 (prawy)
        int speed2 = baseSpeed - turnAdjust;
        speed2 = constrain(speed2, 0, 255);

        // Ustawienie kierunku i prędkości dla silnika 1
        if (msg.up > msg.down) {
            digitalWrite(IN1_PIN, HIGH);
            digitalWrite(IN2_PIN, LOW);
        } else if (msg.down > msg.up) {
            digitalWrite(IN1_PIN, LOW);
            digitalWrite(IN2_PIN, HIGH);
        } else {
            digitalWrite(IN1_PIN, LOW);
            digitalWrite(IN2_PIN, LOW);
        }
        analogWrite(ENA_PIN, speed1);

        // Ustawienie kierunku i prędkości dla silnika 2
        if (msg.up > msg.down) {
            digitalWrite(IN3_PIN, HIGH);
            digitalWrite(IN4_PIN, LOW);
        } else if (msg.down > msg.up) {
            digitalWrite(IN3_PIN, LOW);
            digitalWrite(IN4_PIN, HIGH);
        } else {
            digitalWrite(IN3_PIN, LOW);
            digitalWrite(IN4_PIN, LOW);
        }
        analogWrite(ENB_PIN, speed2);
    }
}

/**
 * Callback wywoływany po odebraniu danych przez ESP-NOW
 * @param mac_addr Adres MAC nadajnika
 * @param data Wskaźnik na odebrane dane
 * @param len Długość danych
 */
void OnDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (len == sizeof(struct_message)) {
        memcpy(&receivedData, data, sizeof(receivedData));
        handleReceivedData(receivedData);
    } else {
        Serial.print("Received data size mismatch. Expected: ");
        Serial.print(sizeof(struct_message));
        Serial.print(", Received: ");
        Serial.println(len);
    }
}

// void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
//     Serial.print("Send Status: ");
//     Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
// }																		


// ============================================================================
// Funkcje główne Arduino
// ============================================================================

/**
 * Funkcja inicjalizacyjna programu
 */
void setup() {
    Serial.begin(115200);  // Inicjalizacja komunikacji szeregowej

    // Konfiguracja pinów dla DRI0041
    pinMode(ENA_PIN, OUTPUT);
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(ENB_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);

    // Inicjalizacja Wi-Fi i ESP-NOW
    WiFi.mode(WIFI_STA);  // Ustawienie trybu stacji
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        while (1); // Pętla nieskończona w przypadku błędu
    }
																			  
    esp_now_register_recv_cb(OnDataRecv);  // Rejestracja callbacku odbioru

    #ifdef DEBUG
    debugStructSize();
    #endif

    Serial.println("Receiver Ready");
}

/**
 * Główna pętla programu
 */
void loop() {
    delay(10);  // Krótkie opóźnienie dla stabilności
}