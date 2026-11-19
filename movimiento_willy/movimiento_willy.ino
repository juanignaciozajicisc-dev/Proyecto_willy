#include <WiFi.h> 
#include <AsyncTCP.h> 
#include <ESPAsyncWebServer.h> 
#include <Preferences.h>
#include <ctype.h>
#include <esp_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
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
Preferences userStore;
bool userStoreReady = false;

constexpr uint8_t MAX_USERS = 20;
constexpr uint8_t MAX_SESSIONS = 8;
constexpr uint32_t SESSION_TTL_MS = 12UL * 60UL * 60UL * 1000UL;
constexpr uint32_t PASSWORD_ITERATIONS = 10000;

struct UserSession {
  String token;
  uint32_t expiresAt = 0;
};

UserSession userSessions[MAX_SESSIONS];
struct AuthenticatedSocket {
  uint32_t id = 0;
  String token;
};
AuthenticatedSocket authenticatedSockets[MAX_SESSIONS];

void userKey(char prefix, uint8_t index, char *key, size_t size) {
  snprintf(key, size, "%c%02u", prefix, index);
}

String getUserField(char prefix, uint8_t index) {
  char key[8];
  userKey(prefix, index, key, sizeof(key));
  return userStore.getString(key, "");
}

int findUser(const String &email) {
  uint8_t count = min(userStore.getUChar("count", 0), MAX_USERS);
  for (uint8_t i = 0; i < count; ++i) {
    if (getUserField('e', i) == email) return i;
  }
  return -1;
}

String bytesToHex(const uint8_t *bytes, size_t length) {
  const char hex[] = "0123456789abcdef";
  String result;
  result.reserve(length * 2);
  for (size_t i = 0; i < length; ++i) {
    result += hex[bytes[i] >> 4];
    result += hex[bytes[i] & 0x0f];
  }
  return result;
}

bool derivePassword(const String &password, const String &saltHex, String &hashHex) {
  uint8_t salt[16];
  if (saltHex.length() != 32) return false;
  for (size_t i = 0; i < sizeof(salt); ++i) {
    char byteText[3] = {saltHex[i * 2], saltHex[i * 2 + 1], '\0'};
    salt[i] = strtoul(byteText, nullptr, 16);
  }

  uint8_t output[32];
  int result = mbedtls_pkcs5_pbkdf2_hmac_ext(
    MBEDTLS_MD_SHA256,
    reinterpret_cast<const unsigned char *>(password.c_str()), password.length(),
    salt, sizeof(salt), PASSWORD_ITERATIONS, sizeof(output), output
  );
  if (result != 0) return false;
  hashHex = bytesToHex(output, sizeof(output));
  return true;
}

String requestSessionToken(AsyncWebServerRequest *request) {
  if (!request->hasHeader("Cookie")) return "";
  String cookies = request->getHeader("Cookie")->value();
  int start = cookies.indexOf("willy_session=");
  if (start < 0) return "";
  start += 14;
  int end = cookies.indexOf(';', start);
  String token = end < 0 ? cookies.substring(start) : cookies.substring(start, end);
  if (token.length() != 64) return "";
  return token;
}

bool isAuthenticated(AsyncWebServerRequest *request) {
  String token = requestSessionToken(request);
  if (token.length() == 0) return false;
  for (UserSession &session : userSessions) {
    if (session.token == token && static_cast<int32_t>(millis() - session.expiresAt) < 0) return true;
  }
  return false;
}

String createSessionToken() {
  uint8_t randomBytes[32];
  for (UserSession &session : userSessions) {
    if (session.token.length() > 0 && static_cast<int32_t>(millis() - session.expiresAt) < 0) continue;
    esp_fill_random(randomBytes, sizeof(randomBytes));
    session.token = bytesToHex(randomBytes, sizeof(randomBytes));
    session.expiresAt = millis() + SESSION_TTL_MS;
    return session.token;
  }
  return "";
}

void redirectToLogin(AsyncWebServerRequest *request) {
  request->redirect("/login");
}

void sendJson(AsyncWebServerRequest *request, int status, const String &body) {
  request->send(status, "application/json; charset=utf-8", body);
}

String jsonMessage(bool ok, const String &message, const String &email = "") {
  String body = String("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":\"" + message + "\"";
  if (email.length() > 0) body += ",\"email\":\"" + email + "\"";
  body += "}";
  return body;
}

bool saveUser(uint8_t index, const String &name, const String &email, const String &salt, const String &hash) {
  char nameKey[8], emailKey[8], saltKey[8], hashKey[8];
  userKey('n', index, nameKey, sizeof(nameKey));
  userKey('e', index, emailKey, sizeof(emailKey));
  userKey('s', index, saltKey, sizeof(saltKey));
  userKey('p', index, hashKey, sizeof(hashKey));
  bool saved = userStore.putString(nameKey, name) && userStore.putString(emailKey, email) &&
               userStore.putString(saltKey, salt) && userStore.putString(hashKey, hash);
  if (saved) userStore.putUChar("count", index + 1);
  return saved;
}

bool isAuthorizedSocket(uint32_t clientId) {
  for (const AuthenticatedSocket &socketSession : authenticatedSockets) {
    if (socketSession.id != clientId) continue;
    for (const UserSession &session : userSessions) {
      if (session.token == socketSession.token && static_cast<int32_t>(millis() - session.expiresAt) < 0) return true;
    }
  }
  return false;
}

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
    
case 'F':  // Adelante
  left = SPEED_FORWARD;
  right = SPEED_FORWARD;
  break;

case 'B':  // Atrás
  left = -SPEED_FORWARD;
  right = -SPEED_FORWARD;
  break;

case 'L':  // Izquierda: rueda izquierda atrás y derecha adelante
  left = -SPEED_TURN_OUTER;
  right = SPEED_TURN_OUTER;
  break;

case 'R':  // Derecha: rueda izquierda adelante y derecha atrás
  left = SPEED_TURN_OUTER;
  right = -SPEED_TURN_OUTER;
  break;
  
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

Serial.printf("[DEBUG] F/B: left=%d right=%d\n", left, right);
Serial.printf("[DEBUG] PWM ENA GPIO14=%d, ENB GPIO32=%d\n",
              abs(left), abs(right));
Serial.printf("[DEBUG] IN1=%d IN2=%d IN3=%d IN4=%d\n",
              digitalRead(IN1_PIN), digitalRead(IN2_PIN),
              digitalRead(IN3_PIN), digitalRead(IN4_PIN));
}  
void onWebSocketEvent(AsyncWebSocket *serverRef, AsyncWebSocketClient *client,                       
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {   
  (void)serverRef;   
  switch (type) {     
    case WS_EVT_CONNECT: {
      AsyncWebServerRequest *request = static_cast<AsyncWebServerRequest *>(arg);
      if (!request || !isAuthenticated(request)) {
        client->close();
        break;
      }
      for (AuthenticatedSocket &socketSession : authenticatedSockets) {
        if (socketSession.id == 0) {
          socketSession.id = client->id();
          socketSession.token = requestSessionToken(request);
          Serial.printf("Control autenticado: cliente #%u\n", client->id());
          return;
        }
      }
      client->close();
      break;     
    }
    case WS_EVT_DISCONNECT:       
      for (AuthenticatedSocket &socketSession : authenticatedSockets) {
        if (socketSession.id == client->id()) {
          socketSession.id = 0;
          socketSession.token = "";
        }
      }
      stopMotors();       
      Serial.println("Control desconectado; motores detenidos.");       
      break;     
    case WS_EVT_DATA:       
      if (!isAuthorizedSocket(client->id())) {
        client->close();
      } else if (len > 0) {         
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

void handleRegistration(AsyncWebServerRequest *request) {
  if (!userStoreReady) {
    sendJson(request, 503, jsonMessage(false, "El almacenamiento de usuarios no está disponible."));
    return;
  }
  String name = request->hasParam("name", true) ? request->getParam("name", true)->value() : "";
  String email = request->hasParam("email", true) ? request->getParam("email", true)->value() : "";
  String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
  name.trim();
  email.trim();
  email.toLowerCase();

  if (name.length() == 0 || email.length() == 0 || password.length() < 8 || password.length() > 64 || name.length() > 64 || email.length() > 64) {
    sendJson(request, 400, jsonMessage(false, "Revisá los datos. La contraseña debe tener entre 8 y 64 caracteres."));
    return;
  }
  bool emailCharsValid = true;
  for (size_t i = 0; i < email.length(); ++i) {
    char c = email[i];
    if (!(isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-' || c == '+' || c == '@')) {
      emailCharsValid = false;
      break;
    }
  }
  if (!emailCharsValid || email.indexOf('@') <= 0 || email.lastIndexOf('@') != email.indexOf('@') ||
      email.indexOf('.') < email.indexOf('@') + 2) {
    sendJson(request, 400, jsonMessage(false, "Ingresá un correo válido."));
    return;
  }
  int existing = findUser(email);
  if (existing >= 0) {
    sendJson(request, 409, jsonMessage(false, "Ese correo ya está registrado."));
    return;
  }
  uint8_t index = min(userStore.getUChar("count", 0), MAX_USERS);
  if (index >= MAX_USERS) {
    sendJson(request, 507, jsonMessage(false, "Se alcanzó el máximo de cuentas para este Willy."));
    return;
  }

  uint8_t saltBytes[16];
  esp_fill_random(saltBytes, sizeof(saltBytes));
  String salt = bytesToHex(saltBytes, sizeof(saltBytes));
  String hash;
  if (!derivePassword(password, salt, hash) || !saveUser(index, name, email, salt, hash)) {
    sendJson(request, 500, jsonMessage(false, "No se pudo guardar la cuenta en Willy."));
    return;
  }
  sendJson(request, 201, jsonMessage(true, "Cuenta creada.", email));
}

void handleLogin(AsyncWebServerRequest *request) {
  if (!userStoreReady) {
    sendJson(request, 503, jsonMessage(false, "El almacenamiento de usuarios no está disponible."));
    return;
  }
  String email = request->hasParam("email", true) ? request->getParam("email", true)->value() : "";
  String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
  email.trim();
  email.toLowerCase();
  int index = findUser(email);
  if (index < 0 || password.length() == 0) {
    sendJson(request, 401, jsonMessage(false, "Correo o contraseña incorrectos."));
    return;
  }
  String hash;
  if (!derivePassword(password, getUserField('s', index), hash) || hash != getUserField('p', index)) {
    sendJson(request, 401, jsonMessage(false, "Correo o contraseña incorrectos."));
    return;
  }
  String token = createSessionToken();
  if (token.length() == 0) {
    sendJson(request, 503, jsonMessage(false, "Hay demasiadas sesiones activas. Probá de nuevo."));
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json; charset=utf-8", "{\"ok\":true}");
  String cookie = "willy_session=";
  cookie += token;
  cookie += "; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict";
  response->addHeader("Set-Cookie", cookie);
  request->send(response);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("No se pudo crear el punto de acceso Wi-Fi.");
    return;
  }
  Serial.printf("Wi-Fi: %s\nClave: %s\n", WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Acceso a Willy: http://%s/\n", WiFi.softAPIP().toString().c_str());

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  bool pwmA = ledcAttach(ENA_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  bool pwmB = ledcAttach(ENB_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  Serial.printf("[PWM] Motor A (GPIO 14): %s\n", pwmA ? "OK" : "ERROR");
  Serial.printf("[PWM] Motor B (GPIO 32): %s\n", pwmB ? "OK" : "ERROR");
  if (!pwmA || !pwmB) {
    Serial.println("[ERROR] Fallo al configurar PWM; Willy seguirá mostrando el portal.");
    stopMotors();
  }
  stopMotors();

  userStoreReady = userStore.begin("willy-users", false);
  if (!userStoreReady) {
    Serial.println("[ERROR] No se pudo abrir el almacenamiento de cuentas; el portal seguirá visible.");
  }

  socket.onEvent(onWebSocketEvent);
  server.addHandler(&socket);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isAuthenticated(request)) request->send_P(200, "text/html; charset=utf-8", PAGE_INDEX);
    else request->send_P(200, "text/html; charset=utf-8", PAGE_LOGIN);
  });
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isAuthenticated(request)) request->redirect("/index.html");
    else request->send_P(200, "text/html; charset=utf-8", PAGE_LOGIN);
  });
  server.on("/register", HTTP_POST, handleRegistration);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    String token = requestSessionToken(request);
    for (UserSession &session : userSessions) if (session.token == token) session.token = "";
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Saliendo");
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "willy_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    request->send(response);
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return redirectToLogin(request);
    request->send_P(200, "text/html; charset=utf-8", PAGE_INDEX);
  });
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return redirectToLogin(request);
    request->send_P(200, "text/html; charset=utf-8", PAGE_CONTROL);
  });
  server.on("/control.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return redirectToLogin(request);
    request->send_P(200, "text/html; charset=utf-8", PAGE_CONTROL);
  });
  server.on("/explicacion.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return redirectToLogin(request);
    request->send_P(200, "text/html; charset=utf-8", PAGE_EXPLICACION);
  });
  server.on("/control.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return redirectToLogin(request);
    request->send_P(200, "text/css; charset=utf-8", CSS_CONTROL);
  });
  server.on("/diseno.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return redirectToLogin(request);
    request->send_P(200, "text/css; charset=utf-8", CSS_DISENO);
  });

  server.begin();
  Serial.println("Portal con registro y login listo.");
}

void loop() {
  if (motorActive && millis() - lastDriveCommandMs > COMMAND_TIMEOUT_MS) {
    stopMotors();
    Serial.println("Tiempo de seguridad agotado; motores detenidos.");
  }
  socket.cleanupClients();
  delay(5);
}

