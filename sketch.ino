#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

#define PIN_SENSOR_VENTO     35
#define PIN_SENSOR_ENCHENTE  34
#define PIN_SENSOR_FUMACA    33

#define PIN_LED_VERMELHO 25
#define PIN_LED_VERDE    26
#define PIN_LED_AZUL     27
#define PIN_BUZZER       14

unsigned long tempoUltimaLeitura = 0;
const long intervaloLeitura = 1500;

bool alertaUrgenteAtivo = false;
unsigned long tempoUltimoPiscar = 0;
int estadoPiscar = LOW;
int contagemPiscos = 0;

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
}

void reconnect() {
  while (!client.connected()) {
    client.connect("EcoSafe-Wokwi");
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AZUL, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long tempoAtual = millis();

  if (tempoAtual - tempoUltimaLeitura >= intervaloLeitura && !alertaUrgenteAtivo) {
    tempoUltimaLeitura = tempoAtual;

    int leituraVento = analogRead(PIN_SENSOR_VENTO);
    int leituraAgua = analogRead(PIN_SENSOR_ENCHENTE);
    int leituraFumaca = analogRead(PIN_SENSOR_FUMACA);

    float vento_kmh = map(leituraVento, 0, 4095, 0, 120);
    float agua_cm = map(leituraAgua, 0, 4095, 0, 150);
    float fumaca_ppm = map(leituraFumaca, 0, 4095, 0, 1000);

    String riscoVento = classificarNivel(leituraVento);
    String riscoAgua = classificarNivel(leituraAgua);
    String riscoFumaca = classificarNivel(leituraFumaca);


    String payload = "{";
    payload += "\"smoke\":" + String((int)fumaca_ppm) + ",";
    payload += "\"wind\":" + String((int)vento_kmh) + ",";
    payload += "\"water\":" + String((int)agua_cm) + ",";
    payload += "\"risk\":{";
    payload += "\"fumaca\":\"" + riscoFumaca + "\",";
    payload += "\"vento\":\"" + riscoVento + "\",";
    payload += "\"agua\":\"" + riscoAgua + "\"}}";

    // Enviar o mesmo JSON para os três tópicos esperados no Node-RED
    client.publish("ecofire/fumaca", payload.c_str());
    client.publish("ecowind/vento", payload.c_str());
    client.publish("ecoflood/nivel", payload.c_str());

    // Mostrar no terminal
    Serial.println("========================================");
    Serial.print("Velocidade do Vento: "); Serial.print(vento_kmh); Serial.print(" km/h ");
    Serial.print("(" + riscoVento + ") | ");
    Serial.print("Nível da Água: "); Serial.print(agua_cm); Serial.print(" cm ");
    Serial.print("(" + riscoAgua + ") | ");
    Serial.print("Nível de Fumaça: "); Serial.print(fumaca_ppm); Serial.print(" ppm ");
    Serial.println("(" + riscoFumaca + ")");

    int sensoresComRiscoAlto = 0;
    if (leituraVento >= 3500) sensoresComRiscoAlto++;
    if (leituraAgua >= 3500) sensoresComRiscoAlto++;
    if (leituraFumaca >= 3500) sensoresComRiscoAlto++;

    if (leituraVento < 1500 && leituraAgua < 1500 && leituraFumaca < 1500) {
      Serial.println("Baixo risco ");
      definirCorLED(0, 255, 0);
      noTone(PIN_BUZZER);
    } else if (sensoresComRiscoAlto >= 2) {
      Serial.println("ALERTA URGENTE!");
      alertaUrgenteAtivo = true;
      contagemPiscos = 0;
      tempoUltimoPiscar = tempoAtual;
    } else if (leituraVento >= 3000 || leituraAgua >= 3000 || leituraFumaca >= 3000) {
      Serial.print("ALERTA DE RISCO ALTO ");
      if (leituraVento >= 3000) Serial.print("Vento ");
      if (leituraAgua >= 3000) Serial.print("Enchentes ");
      if (leituraFumaca >= 3000) Serial.print("Fumaça ");
      Serial.println();
      definirCorLED(255, 0, 0);
      tone(PIN_BUZZER, 1000, 800);
    } else {
      Serial.print("Risco médio ");
      if (leituraVento >= 1500) Serial.print("Vento ");
      if (leituraAgua >= 1500) Serial.print("Água ");
      if (leituraFumaca >= 1500) Serial.print("Fumaça ");
      Serial.println();
      definirCorLED(255, 255, 0);
      tone(PIN_BUZZER, 500, 200);
    }
  }

  if (alertaUrgenteAtivo && millis() - tempoUltimoPiscar >= 300) {
    tempoUltimoPiscar = millis();
    if (estadoPiscar == LOW) {
      definirCorLED(255, 0, 0);
      tone(PIN_BUZZER, 1500);
      estadoPiscar = HIGH;
    } else {
      definirCorLED(0, 0, 0);
      noTone(PIN_BUZZER);
      estadoPiscar = LOW;
      contagemPiscos++;
    }
    if (contagemPiscos >= 6) {
      alertaUrgenteAtivo = false;
      definirCorLED(0, 0, 0);
      noTone(PIN_BUZZER);
    }
  }
}

String classificarNivel(int valor) {
  if (valor < 1500) return "Normal";
  else if (valor < 3000) return "Médio";
  else return "Alto";
}

void definirCorLED(int vermelho, int verde, int azul) {
  analogWrite(PIN_LED_VERMELHO, vermelho);
  analogWrite(PIN_LED_VERDE, verde);
  analogWrite(PIN_LED_AZUL, azul);
}
