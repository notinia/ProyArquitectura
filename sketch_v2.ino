/*
  ====================================================================
  Sistema HC-SR04 - ESP32 Firmware 
  Trabajo Final · ARQUITECTURA AVANZADA / COMPLEJIDAD ALGORÍTMICA
  Universidad CAECE · Mar del Plata
  ====================================================================
  Hardware SIMULADO: ESP32 + HC-SR04 
  ====================================================================
  Tópicos MQTT:
    PUBLICA  caece/tof/distancia   -> valor numérico puro en mm
    SUSCRIBE caece/tof/config      -> JSON con nueva config
    SUSCRIBE caece/tof/cmd         -> JSON con {activo: bool}
    SUSCRIBE caece/tof/buzzer      -> "auto" | "manual" | "off" | "SILENCIAR"
  ====================================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Pines
const int TRIG_PIN   = 5;
const int ECHO_PIN   = 18;
const int LED_PIN    = 2;
const int BUZZER_PIN = 4;

// Parámetros configurables
int  umbralMM      = 200;
int  muestreoSegS  = 1;
bool sistemaActivo = true;
char sistemaID[32] = "SENSOR-01";

// WiFi / MQTT
const char* WIFI_SSID   = "Wokwi-GUEST";
const char* WIFI_PASS   = "";
const char* MQTT_BROKER = "test.mosquitto.org";
const int   MQTT_PORT   = 1883;
const char* MQTT_CLIENT = "esp32-tof-caece-v3";

const char* TOPIC_DIST   = "caece/tof/distancia";
const char* TOPIC_CONFIG = "caece/tof/config";
const char* TOPIC_CMD    = "caece/tof/cmd";
const char* TOPIC_BUZZER = "caece/tof/buzzer";

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// Estado interno
bool buzzerSilenciadoManual = false;
char buzzerModo[8]         = "auto";
unsigned long ultimaLect   = 0;

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  conectarWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(onMQTTMessage);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!mqttClient.connected())        conectarMQTT();
  mqttClient.loop();

  if (!sistemaActivo) {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
    delay(500);
    return;
  }

  unsigned long ahora = millis();
  long intervaloMs    = (long)muestreoSegS * 1000;
  
  if (ahora - ultimaLect >= intervaloMs) {
    ultimaLect = ahora;
    long dist = leerDistanciaMM();
    if (dist > 0) procesarDistancia(dist);
  }
}

long leerDistanciaMM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long durUS = pulseIn(ECHO_PIN, HIGH, 30000);
  if (durUS == 0) return -1;
  return (durUS * 343L) / 2000;
}

void procesarDistancia(long distMM) {
  Serial.print("[ToF] Distancia: ");
  Serial.print(distMM);
  Serial.println(" mm");

  bool dentroUmbral = (distMM <= umbralMM);
  digitalWrite(LED_PIN, dentroUmbral ? HIGH : LOW);

  // Lógica local del actuador
  if (strcmp(buzzerModo, "off") == 0) {
    noTone(BUZZER_PIN);
  } else if (strcmp(buzzerModo, "auto") == 0) {
    if (dentroUmbral) {
      tone(BUZZER_PIN, 1000);
    } else {
      noTone(BUZZER_PIN);
      buzzerSilenciadoManual = false;
    }
  } else if (strcmp(buzzerModo, "manual") == 0) {
    if (dentroUmbral && !buzzerSilenciadoManual) {
      tone(BUZZER_PIN, 1000);
    } else if (!dentroUmbral) {
      buzzerSilenciadoManual = false;
      noTone(BUZZER_PIN);
    }
  }

  // Se delega todo el procesamiento avanzado de la alerta al orquestador
  publicarDistancia(distMM);
}

void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  char buf[256] = {0};
  memcpy(buf, payload, min((unsigned int)255, length));

  Serial.print("[MQTT IN] ");
  Serial.print(topicStr);
  Serial.print(" -> ");
  Serial.println(buf);

  if (topicStr == TOPIC_BUZZER) {
    String cmd = String(buf);
    cmd.trim();
    if (cmd == "off") {
      strlcpy(buzzerModo, "off", sizeof(buzzerModo));
      noTone(BUZZER_PIN);
    } else if (cmd == "auto") {
      strlcpy(buzzerModo, "auto", sizeof(buzzerModo));
      buzzerSilenciadoManual = false;
    } else if (cmd == "manual") {
      strlcpy(buzzerModo, "manual", sizeof(buzzerModo));
      buzzerSilenciadoManual = false;
    } else if (cmd == "SILENCIAR") {
      buzzerSilenciadoManual = true;
      noTone(BUZZER_PIN);
    }
    return;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, buf) != DeserializationError::Ok) return;

  if (topicStr == TOPIC_CONFIG) {
    if (doc.containsKey("umbral_mm"))         umbralMM     = doc["umbral_mm"];
    if (doc.containsKey("tiempo_muestreo_s")) muestreoSegS = doc["tiempo_muestreo_s"];
    if (doc.containsKey("sistema_id")) {
      strlcpy(sistemaID, doc["sistema_id"] | "SENSOR-01", sizeof(sistemaID));
    }
  }

  if (topicStr == TOPIC_CMD) {
    if (doc.containsKey("activo")) {
      sistemaActivo = doc["activo"];
    }
  }
}

void conectarWiFi() {
  Serial.print("[WiFi] Conectando");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(250); Serial.print(".");
    intentos++;
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\n[WiFi] Conectado." : "\n[WiFi] Fallo.");
}

void conectarMQTT() {
  Serial.print("[MQTT] Conectando...");
  if (mqttClient.connect(MQTT_CLIENT)) {
    Serial.println(" OK");
    mqttClient.subscribe(TOPIC_CONFIG);
    mqttClient.subscribe(TOPIC_CMD);
    mqttClient.subscribe(TOPIC_BUZZER);
  } else {
    Serial.printf(" FALLO rc=%d\n", mqttClient.state());
  }
}

void publicarDistancia(long distMM) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", distMM);
  mqttClient.publish(TOPIC_DIST, buf);
}