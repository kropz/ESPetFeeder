/*
  Autor: kropz (KRPZ)
  Proyecto: ESPetFeeder
  Materia: Circuitos Eléctricos
*/

#include <WiFi.h> // Librería para la conexión WiFi
#include <WiFiClientSecure.h> // Librería para el cliente WiFi seguro
#include <ESP32Servo.h> // Librería para controlar servos en ESP32
#include <UniversalTelegramBot.h> // Librería para el bot de Telegram
#include <ArduinoJson.h> // Librería para manipular JSON
#include <NTPClient.h> // Librería para sincronización de hora con NTP
#include <HTTPClient.h> // Librería para realizar solicitudes HTTP

Servo myservo; // Crea el objeto servo para controlar el servo
static const int servoPin = 33; // Pin de salida para el servo

struct WiFiNetwork {
  String ssid;
  String password;
};

WiFiNetwork wifiNetworks[] = {
  {"TP-LINK21", "45434r3321234r"},
  {"xperia", "d8nn8jgwviud7gh"},
  {"Fliazw", "35295urnv"},
  {"troniz", "235twf"},
  {"Galaxy A20s5417", "tepe5823"}
};

const int numNetworks = sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);

const char* ssid = ""; // Variable para almacenar el nombre de la red WiFi actual
const char* password = ""; // Variable para almacenar la contraseña de la red WiFi actual

#define BOTtoken "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" //Token del bot
#define CHAT_ID "XXXXXXX" // ID del chat de Telegram

WiFiClientSecure client; // Cliente WiFi seguro
WiFiUDP ntpUDP; // Creamos una instancia de WiFiUDP para la sincronización de hora
NTPClient timeClient(ntpUDP, "pool.ntp.org"); // Configuramos el servidor NTP

UniversalTelegramBot bot(BOTtoken, client); // Crea el objeto bot de Telegram

int dispensacionHora = 0; // Hora programada para la dispensación
int dispensacionMinuto = 0; // Minuto programado para la dispensación
bool programacionActiva = false; // Indicador de si hay una programación activa
bool modoAhorroActivado = false; // Indicador de si el modo de ahorro de energía está activado

const char* accessPassword = "CircuitosElectricos"; // Contraseña de acceso al bot de Telegram
bool passwordCorrect = false; // Indicador de si la contraseña ingresada es correcta

// Función para manejar los nuevos mensajes recibidos
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
      continue;
    }

    String text = bot.messages[i].text;

    if (!passwordCorrect) {
      if (text == accessPassword) {
        passwordCorrect = true;
        bot.sendMessage(chat_id, "Contraseña correcta. Acceso concedido.", "");
        sendWelcomeMessage(chat_id);
      } else {
        bot.sendMessage(chat_id, "Contraseña incorrecta. Introduce la contraseña correcta.", "");
      }
      continue;
    }

    if (text == "/comida") {
      if (modoAhorroActivado) {
        bot.sendMessage(chat_id, "El modo de ahorro de energía está activado. Las funcionalidades están desactivadas.", "");
        continue;
      }
      bot.sendMessage(chat_id, "Alimentando", "");
      myservo.write(180);
      delay(1500);
      myservo.write(0);
      sendFoodNotification(chat_id);
    }
    else if (text == "/comidaextra") {
      if (modoAhorroActivado) {
        bot.sendMessage(chat_id, "El modo de ahorro de energía está activado. Las funcionalidades están desactivadas.", "");
        continue;
      }
      bot.sendMessage(chat_id, "Alimentando con tiempo extra", "");
      myservo.write(180);
      delay(2500);
      myservo.write(0);
      sendFoodNotification(chat_id);
    }
    else if (text.startsWith("/programar")) {
      if (modoAhorroActivado) {
        bot.sendMessage(chat_id, "El modo de ahorro de energía está activado. Las funcionalidades están desactivadas.", "");
        continue;
      }
      String programacion = text.substring(11);
      int hora = programacion.substring(0, 2).toInt();
      int minuto = programacion.substring(3).toInt();

      if (hora >= 0 && hora < 24 && minuto >= 0 && minuto < 60) {
        bot.sendMessage(chat_id, "Programación exitosa: " + programacion, "");
        dispensacionHora = hora;
        dispensacionMinuto = minuto;
        programacionActiva = true;
      }
      else {
        bot.sendMessage(chat_id, "Error de programación. Formato válido: HH:MM", "");
      }
    }
    else if (text == "/detener") {
      if (modoAhorroActivado) {
        bot.sendMessage(chat_id, "El modo de ahorro de energía está activado. Las funcionalidades están desactivadas.", "");
        continue;
      }
      bot.sendMessage(chat_id, "Programación detenida", "");
      programacionActiva = false;
    }
    else if (text.startsWith("/dispensarmas")) {
      if (modoAhorroActivado) {
        bot.sendMessage(chat_id, "El modo de ahorro de energía está activado. Las funcionalidades están desactivadas.", "");
        continue;
      }
      int duracionAdicional = text.substring(13).toInt();

      if (duracionAdicional > 0) {
        bot.sendMessage(chat_id, "Dispensando más comida durante " + String(duracionAdicional) + " segundos", "");
        myservo.write(180);
        delay(duracionAdicional * 1000);
        myservo.write(0);
      }
      else {
        bot.sendMessage(chat_id, "Error de dispensación adicional. La duración debe ser mayor a 0 segundos", "");
      }
    }
    else if (text == "/ubicacion") {
      String currentLocation = getCurrentLocation();
      bot.sendMessage(chat_id, "Ubicación actual: " + currentLocation, "");
    }
    else if (text == "/hora") {
      String currentTime = timeClient.getFormattedTime();
      bot.sendMessage(chat_id, "Hora actual: " + currentTime, "");
    }
    else if (text == "/modoahorro") {
      if (!modoAhorroActivado) {
        bot.sendMessage(chat_id, "Modo de ahorro de energía activado. Las funcionalidades están desactivadas.", "");
        myservo.detach();
        programacionActiva = false;
        modoAhorroActivado = true;
      }
      else {
        bot.sendMessage(chat_id, "El modo de ahorro de energía ya está activado", "");
      }
    }
    else if (text == "/modonormal") {
      if (modoAhorroActivado) {
        bot.sendMessage(chat_id, "Modo de ahorro de energía desactivado", "");
        myservo.attach(servoPin);
        modoAhorroActivado = false;
      }
      else {
        bot.sendMessage(chat_id, "El modo de ahorro de energía ya está desactivado", "");
      }
    }
    else if (text == "/ayuda") {
      String ayudaMessage = "Comandos disponibles:\n"
                        "/comida - Dispensa la cantidad estándar de comida\n"
                        "/comidaextra - Dispensa una cantidad extra de comida\n"
                        "/programar HH:MM - Programa una hora específica de dispensación\n"
                        "/detener - Detiene la programación de dispensación\n"
                        "/dispensarmas segundos - Dispensa comida adicional durante un tiempo especificado\n"
                        "/ubicacion - Obtiene la ubicación actual\n"
                        "/hora - Obtiene la hora actual\n"
                        "/modoahorro - Activa el modo de ahorro de energía\n"
                        "/modonormal - Desactiva el modo de ahorro de energía\n"
                        "/redwifi - Muestra la red en el que está conectado ESPetFeeder\n"
                        "/ayuda - Muestra esta lista de comandos";
      bot.sendMessage(chat_id, ayudaMessage, "");
    }
    else if (text == "/redwifi") {
      if (ssid != "") {
        bot.sendMessage(chat_id, "Conectado a la red WiFi: " + String(ssid), "");
      }
      else {
        bot.sendMessage(chat_id, "No se encuentra conectado a ninguna red WiFi", "");
      }
    }
    else {
      bot.sendMessage(chat_id, "Comando no reconocido. Utiliza /comida, /comidaextra, /programar, /detener, /dispensarmas, /ubicacion, /hora, /modoahorro, /modonormal, /redwifi o /ayuda", "");
    }

  }
}

// Función para obtener la ubicación actual
String getCurrentLocation() {
  String location;

  HTTPClient http;
  http.begin("http://ip-api.com/json");
  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, payload);
    if (!error) {
      const char* city = jsonDoc["city"];
      const char* country = jsonDoc["country"];
      location = String(city) + ", " + String(country);
    }
  }
  http.end();

  return location;
}

// Función para enviar notificación de alimentación
void sendFoodNotification(String chat_id) {
  String message = "Se ha realizado una alimentación";
  bot.sendMessage(chat_id, message, "");
}

// Función para enviar el mensaje de bienvenida
void sendWelcomeMessage(String chat_id) {
  String welcomeMessage = "Bienvenido al producto ESPetFeeder desarrollado por Sebastián Grajales y Jahir Zapata\n"
                          "Ingresa los comandos para interactuar con el sistema.";
  bot.sendMessage(chat_id, welcomeMessage, "");
}

void connectWiFi() {
  Serial.println("Conectando a WiFi");

  for (int i = 0; i < numNetworks; i++) {
    Serial.print("Conectando a la red: ");
    Serial.println(wifiNetworks[i].ssid);

    WiFi.begin(wifiNetworks[i].ssid.c_str(), wifiNetworks[i].password.c_str());

    int wifiConnectAttempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      wifiConnectAttempts++;
      if (wifiConnectAttempts > 60) {
        Serial.println("Fallo al conectar a la red WiFi. Pasando a la siguiente red...");
        break;
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      ssid = wifiNetworks[i].ssid.c_str();
      password = wifiNetworks[i].password.c_str();
      Serial.println("");
      Serial.println("WiFi conectado.");
      Serial.println("Dirección IP: ");
      Serial.println(WiFi.localIP());
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No se pudo conectar a ninguna red WiFi. Modo de ahorro de energía...");
    myservo.detach(); // Desactiva el servo para ahorrar energía
    delay(5000); // Espera 5 segundos antes de volver a intentar la conexión WiFi
    connectWiFi(); // Intenta conectar WiFi nuevamente
  }
}

void setup() {
  Serial.begin(115200);

  myservo.attach(servoPin);

  connectWiFi();

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  Serial.println("Conectando a WiFi");
  int wifiConnectAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiConnectAttempts++;
    if (wifiConnectAttempts > 60) {
      Serial.println("Fallo al conectar a WiFi. Reiniciando ESP.");
      ESP.restart();
    }
  }

  Serial.println("");
  Serial.println("WiFi conectado.");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());

  bot.sendMessage(CHAT_ID, "El bot AnaAI se ha iniciado. Por favor, ingrese la contraseña para acceder al servicio.", "");

  timeClient.begin();
  timeClient.setTimeOffset(-18000);

  delay(2000); // Espera 2 segundos antes de continuar
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    timeClient.update();

    if (programacionActiva) {
      int currentHour = timeClient.getHours();
      int currentMinute = timeClient.getMinutes();

      if (currentHour == dispensacionHora && currentMinute == dispensacionMinuto) {
        bot.sendMessage(CHAT_ID, "Hora de dispensar comida", "");
        myservo.write(180);
        delay(2500);
        myservo.write(0);
        programacionActiva = false;
        sendFoodNotification(CHAT_ID);
      }
    }

    delay(1000); // Espera 1 segundo antes de volver a verificar los mensajes
  }
  else {
    Serial.println("WiFi desconectado. Modo de ahorro de energía...");
    myservo.detach(); // Desactiva el servo para ahorrar energía
    delay(5000); // Espera 5 segundos antes de volver a verificar la conexión WiFi
    connectWiFi();
  }
}
