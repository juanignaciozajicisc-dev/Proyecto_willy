#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "web_pages.h"

// ESP32 Dev Module + L298N. Pines según Diagrama_conexiones_Willy.md.
const char *WIFI_SSID = "Robot_Willy_AP";
const char *WIFI_PASSWORD = "willy2026";

// Motor A = lado izquierdo; motor B = lado derecho.
constexpr uint8_t ENA_PIN = 14;
constexpr uint8_t IN1_PIN = 27;
constexpr uint8_t IN2_PIN = 26;
constexpr uint8_t ENB_PIN = 32;
constexpr uint8_t IN3_PIN = 25;
constexpr uint8_t IN4_PIN = 33;

constexpr uint32_t PWM_FREQUENCY = 5000;
constexpr uint8_t PWM_RESOLUTION = 8;
constexpr int SPEED_FORWARD = 220;
constexpr int SPEED_TURN_OUTER = 220;
constexpr int SPEED_TURN_INNER = 145;
constexpr uint32_t COMMAND_TIMEOUT_MS = 600;

AsyncWebServer server(80);
AsyncWebSocket socket("/ws");

volatile bool motorActive = false;
volatile uint32_t lastDriveCommandMs = 0;

void setMotor(int speed, uint8_t inA, uint8_t inB, uint8_t enablePin) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    digitalWrite(inA, HIGH);
    digitalWrite(inB, LOW);
  } else if (speed < 0) {
    digitalWrite(inA, LOW);
    digitalWrite(inB, HIGH);
  } else {
    digitalWrite(inA, LOW);
    digitalWrite(inB, LOW);
  }
  ledcWrite(enablePin, abs(speed));
}

void setMotors(int leftSpeed, int rightSpeed) {
  setMotor(leftSpeed, IN1_PIN, IN2_PIN, ENA_PIN);
  setMotor(rightSpeed, IN3_PIN, IN4_PIN, ENB_PIN);
}

void stopMotors() {
  setMotors(0, 0);
  motorActive = false;
}

void runCommand(char command) {
  int left = 0;
  int right = 0;
  switch (command) {
    case 'F': left = SPEED_FORWARD; right = SPEED_FORWARD; break;
    case 'B': left = -SPEED_FORWARD; right = -SPEED_FORWARD; break;
    case 'L': left = SPEED_TURN_INNER; right = SPEED_TURN_OUTER; break;
    case 'R': left = SPEED_TURN_OUTER; right = SPEED_TURN_INNER; break;
    case 'S':
      stopMotors();
      Serial.println("[MOTORES] STOP");
      return;
    default:
      stopMotors();
      Serial.printf("[WS] Comando no reconocido: '%c'\n", command);
      return;
  }

  setMotors(left, right);
  lastDriveCommandMs = millis();
  motorActive = true;
  Serial.printf("[MOTORES] comando=%c izquierda=%d derecha=%d\n", command, left, right);
}

void onWebSocketEvent(AsyncWebSocket *serverRef, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  (void)serverRef;
  (void)arg;
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Control conectado: cliente #%u\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      stopMotors();
      Serial.println("Control desconectado; motores detenidos.");
      break;
    case WS_EVT_DATA:
      if (len > 0) {
        Serial.printf("[WS] Dato recibido: '%c' (%u bytes)\n",
                      static_cast<char>(data[0]), static_cast<unsigned int>(len));
        runCommand(static_cast<char>(data[0]));
      } else {
        Serial.println("[WS] Se recibio un mensaje vacio.");
      }
      break;
    default:
      break;
  }
}

void serveControl(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html; charset=utf-8", PAGE_CONTROL);
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  ledcAttach(ENA_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttach(ENB_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  stopMotors();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("No se pudo crear el punto de acceso Wi-Fi.");
    return;
  }
  Serial.printf("Wi-Fi: %s\nClave: %s\n", WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Panel de motores: http://%s/\n", WiFi.softAPIP().toString().c_str());

  socket.onEvent(onWebSocketEvent);
  server.addHandler(&socket);
  server.on("/", AsyncWebRequestMethod::HTTP_GET, serveControl);
  server.on("/control", AsyncWebRequestMethod::HTTP_GET, serveControl);
  server.on("/control.html", AsyncWebRequestMethod::HTTP_GET, serveControl);
  server.on("/index.html", AsyncWebRequestMethod::HTTP_GET,
            [](AsyncWebServerRequest *request) {
              request->send_P(200, "text/html; charset=utf-8", PAGE_INDEX);
            });
  server.on("/explicacion.html", AsyncWebRequestMethod::HTTP_GET,
            [](AsyncWebServerRequest *request) {
              request->send_P(200, "text/html; charset=utf-8", PAGE_EXPLICACION);
            });
  server.on("/control.css", AsyncWebRequestMethod::HTTP_GET,
            [](AsyncWebServerRequest *request) {
              request->send_P(200, "text/css; charset=utf-8", CSS_CONTROL);
            });
  server.on("/diseno.css", AsyncWebRequestMethod::HTTP_GET,
            [](AsyncWebServerRequest *request) {
              request->send_P(200, "text/css; charset=utf-8", CSS_DISENO);
            });
  server.begin();
  Serial.println("Control de motores listo.");
}

void loop() {
  if (motorActive && millis() - lastDriveCommandMs > COMMAND_TIMEOUT_MS) {
    stopMotors();
    Serial.println("Tiempo de seguridad agotado; motores detenidos.");
  }
  socket.cleanupClients();
  delay(5);
}
