// ============================================================================
// Nagłówki bibliotek
// ============================================================================
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// ============================================================================
// Definicje stałych i zmiennych globalnych
// ============================================================================

// Piny dla sterownika silników DRI0041 (L298N) na ESP32 DevKitC V4
const int ENA_PIN = 26; // Pin PWM dla prędkości silnika lewego
const int IN1_PIN = 27; // Pin kierunku 1 dla silnika lewego
const int IN2_PIN = 14; // Pin kierunku 2 dla silnika lewego
const int ENB_PIN = 12; // Pin PWM dla prędkości silnika prawego
const int IN3_PIN = 13; // Pin kierunku 1 dla silnika prawego
const int IN4_PIN = 15; // Pin kierunku 2 dla silnika prawego

// Piny i konfiguracja dla serwomechanizmu SG90
const int SERVO_PIN = 18; // Pin do sterowania serwem (zmień, jeśli używasz innego pinu)

// Konfiguracja PWM dla silników
const int PWM_FREQ = 5000;         // Częstotliwość PWM dla silników (5 kHz)
const int PWM_RESOLUTION = 8;      // Rozdzielczość PWM dla silników (8 bitów, 0-255)
// Kanały PWM 0-1 zarezerwowane dla ESP32Servo (servo.attach() używa LEDC od kanału 0)
const int PWM_CHANNEL_ENA = 2;     // Kanał PWM dla pinu ENA
const int PWM_CHANNEL_ENB = 3;     // Kanał PWM dla pinu ENB
const int MIN_PWM = 50;            // Minimalna wartość PWM, aby silniki ruszyły
const int SMOOTHING_STEP = 10;     // Maksymalna zmiana prędkości na iterację (soft start/end)
const int UPDATE_INTERVAL = 10;    // Interwał aktualizacji prędkości w ms

// Struktura danych do odbierania przez ESP-NOW
// Użyto atrybutu packed, aby zapewnić spójny rozmiar struktury
typedef struct __attribute__((packed)) {
    uint8_t up;     // Prędkość do przodu (0-255)
    uint8_t down;   // Prędkość do tyłu (0-255)
    uint8_t left;   // Skręt w lewo (0-255)
    uint8_t right;  // Skręt w prawo (0-255)
    bool trigger;   // Stan przycisku wyzwalającego (true/false) dla servo
} struct_message;

// Zmienna przechowująca odebrane dane
struct_message receivedData;

// Bieżące prędkości silników dla mechanizmu soft start/end
int currentSpeed1 = 0; // Bieżąca prędkość silnika lewego
int currentSpeed2 = 0; // Bieżąca prędkość silnika prawego

// Obiekt serwomechanizmu
Servo servo;

// ============================================================================
// Funkcje debugujące
// ============================================================================

/**
 * Wyświetla wartości prędkości dla debugowania
 * @param baseSpeed Prędkość bazowa
 * @param turnAdjust Korekta skrętu
 * @param targetSpeed1 Docelowa prędkość silnika lewego
 * @param currentSpeed1 Bieżąca prędkość silnika lewego
 * @param targetSpeed2 Docelowa prędkość silnika prawego
 * @param currentSpeed2 Bieżąca prędkość silnika prawego
 */
void debugMotorSpeeds(int baseSpeed, int turnAdjust, int targetSpeed1, int currentSpeed1, int targetSpeed2, int currentSpeed2) {
    Serial.print("baseSpeed: ");
    Serial.print(baseSpeed);
    Serial.print(", turnAdjust: ");
    Serial.print(turnAdjust);
    Serial.print(", targetSpeed1: ");
    Serial.print(targetSpeed1);
    Serial.print(", currentSpeed1: ");
    Serial.print(currentSpeed1);
    Serial.print(", targetSpeed2: ");
    Serial.print(targetSpeed2);
    Serial.print(", currentSpeed2: ");
    Serial.println(currentSpeed2);
}

/**
 * Wyświetla stan sterowania serwem (debugowanie)
 * @param angle Kąt ustawienia serwa w stopniach
 */
void debugServoState(int angle) {
    Serial.print("Servo ustawione na: ");
    Serial.print(angle);
    Serial.println(" stopni");
}

// ============================================================================
// Funkcje sterowania silnikami i serwem
// ============================================================================

/**
 * Wygładza prędkość silnika (soft start/end)
 * @param currentSpeed Bieżąca prędkość silnika
 * @param targetSpeed Docelowa prędkość silnika
 * @return Zaktualizowana prędkość po kroku wygładzania
 */
int smoothSpeed(int currentSpeed, int targetSpeed) {
    if (currentSpeed < targetSpeed) {
        // Soft start: zwiększ prędkość o maksymalnie SMOOTHING_STEP
        return min(currentSpeed + SMOOTHING_STEP, targetSpeed);
    } else if (currentSpeed > targetSpeed) {
        // Soft end: zmniejsz prędkość o maksymalnie SMOOTHING_STEP
        return max(currentSpeed - SMOOTHING_STEP, targetSpeed);
    }
    return currentSpeed;
}

/**
 * Ustawia prędkość i kierunek dla silnika
 * @param channel Kanał PWM (ENA lub ENB)
 * @param in1 Pin kierunku 1
 * @param in2 Pin kierunku 2
 * @param speed Prędkość silnika (-255 do 255)
 */
void setMotor(int channel, int in1, int in2, int speed) {
    // Skalowanie prędkości z uwzględnieniem minimalnego progu PWM
    int absSpeed = abs(speed);
    if (absSpeed > 0) {
        absSpeed = map(absSpeed, 0, 255, MIN_PWM, 255); // Skalowanie od MIN_PWM do 255
    }

    // Ustawienie kierunku silnika
    if (speed > 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW); // Ruch do przodu
    } else if (speed < 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH); // Ruch do tyłu
    } else {
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW); // Zatrzymanie silnika
    }

    // Ustawienie prędkości przez PWM
    ledcWrite(channel, absSpeed);
}

/**
 * Aktualizuje stan serwomechanizmu
 */
void updateServo() {
    if (receivedData.trigger) {
        servo.write(90);
        debugServoState(90);
        Serial.println("Trigger aktywny, servo na 90°");
    } else {
        servo.write(0);
        debugServoState(0);
        Serial.println("Trigger nieaktywny, servo na 0°");
    }
}

// ============================================================================
// Funkcje ESP-NOW
// ============================================================================

/**
 * Przetwarza odebrane dane i steruje silnikami oraz serwem
 * @param msg Odebrana struktura danych z wartościami up, down, left, right, trigger
 */
void handleReceivedData(const struct_message& msg) {
    receivedData = msg; // Aktualizacja danych

    // Wyświetlanie odebranych danych w monitorze szeregowym
    Serial.print("Odebrano: up = ");
    Serial.print(receivedData.up);
    Serial.print(", down = ");
    Serial.print(receivedData.down);
    Serial.print(", left = ");
    Serial.print(receivedData.left);
    Serial.print(", right = ");
    Serial.print(receivedData.right);
    Serial.print(", trigger = ");
    Serial.println(receivedData.trigger ? "true" : "false");

    // Aktualizacja stanu servo
    updateServo();

    // Obliczenie prędkości bazowej (różnica między ruchem do przodu a do tyłu)
    int baseSpeed = receivedData.up - receivedData.down;
    baseSpeed = constrain(baseSpeed, -255, 255); // Ograniczenie do zakresu -255 do 255

    // Obliczenie korekty skrętu (różnica między right a left)
    int turnAdjust = ((int)receivedData.right - (int)receivedData.left) / 2;
    turnAdjust = constrain(turnAdjust, -255, 255); // Ograniczenie do zakresu -255 do 255

    // Obliczenie docelowych prędkości dla silników
    int targetSpeed1 = baseSpeed + turnAdjust; // Silnik lewy
    targetSpeed1 = constrain(targetSpeed1, -255, 255);
    int targetSpeed2 = baseSpeed - turnAdjust; // Silnik prawy
    targetSpeed2 = constrain(targetSpeed2, -255, 255);

    // Wygładzanie prędkości dla soft start/end
    currentSpeed1 = smoothSpeed(currentSpeed1, targetSpeed1);
    currentSpeed2 = smoothSpeed(currentSpeed2, targetSpeed2);

    // Ustawienie prędkości i kierunku dla silników
    setMotor(PWM_CHANNEL_ENA, IN1_PIN, IN2_PIN, currentSpeed1);
    setMotor(PWM_CHANNEL_ENB, IN3_PIN, IN4_PIN, currentSpeed2);

    // Debugowanie prędkości silników
    debugMotorSpeeds(baseSpeed, turnAdjust, targetSpeed1, currentSpeed1, targetSpeed2, currentSpeed2);
}

/**
 * Callback wywoływany po odebraniu danych przez ESP-NOW
 * @param mac_addr Adres MAC nadajnika
 * @param data Wskaźnik na odebrane dane
 * @param len Długość odebranych danych
 */
void OnDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    // Sprawdzenie zgodności rozmiaru odebranych danych
    if (len == sizeof(struct_message)) {
        memcpy(&receivedData, data, sizeof(receivedData));
        handleReceivedData(receivedData);
    } else {
        Serial.print("Błąd: Niezgodność rozmiaru danych. Oczekiwano: ");
        Serial.print(sizeof(struct_message));
        Serial.print(", Odebrano: ");
        Serial.println(len);
    }
}

// ============================================================================
// Funkcje główne Arduino
// ============================================================================

/**
 * Inicjalizacja programu
 */
void setup() {
    // Inicjalizacja komunikacji szeregowej
    Serial.begin(115200);

    // Konfiguracja pinów sterownika DRI0041 jako wyjścia
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);

    // Konfiguracja kanałów PWM dla silników
    ledcSetup(PWM_CHANNEL_ENA, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_ENB, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(ENA_PIN, PWM_CHANNEL_ENA);
    ledcAttachPin(ENB_PIN, PWM_CHANNEL_ENB);

    // Inicjalizacja serwomechanizmu
    if (!servo.attach(SERVO_PIN)) {
        Serial.println("Błąd inicjalizacji servo! Sprawdź pin lub zasilanie.");
    } else {
        Serial.println("Servo zainicjalizowane poprawnie.");
        servo.write(0); // Ustawienie domyślnej pozycji (0 stopni)
    }

    // Inicjalizacja Wi-Fi w trybie stacji (STA)
    WiFi.mode(WIFI_STA);

    // Inicjalizacja ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Błąd inicjalizacji ESP-NOW");
        while (1); // Zatrzymanie programu w przypadku błędu
    }

    // Rejestracja callbacku dla odbierania danych
    esp_now_register_recv_cb(OnDataRecv);

    // Potwierdzenie gotowości odbiornika
    Serial.println("Odbiornik gotowy");
}

/**
 * Główna pętla programu
 */
void loop() {
    // Opóźnienie dla stabilności
    delay(UPDATE_INTERVAL);
}