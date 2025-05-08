#define SINRICPRO

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include "AudioFileSourceID3.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "Free_Fonts.h"  // Include the header file attached to this sketch

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <JPEGDecoder.h>

#ifdef SINRICPRO
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <SinricProSignature.cpp>  // ✅ forçage du lien
#endif

#define RELAYPIN_WATCH 21
#define BUTTON_ALARM 39      //BUTTON A : alarm
#define BUTTON_WEBSERVER 38  //BUTTON B : active webserver
#define BUTTON_IDWATCH 37    //BUTTON C : affiche le numéro de série de la montre

#define BAUD_RATE 115200  // Change baudrate to your need
#define WAIT_BLINK 1000
#define WAIT_LOGO 3000
#define WAIT_BUTTON 500
#define WAIT_WATCH 20000  //20 secondes

#define UART_FLUSH 0
#define UART_LOG 1

#define M5TXTDEBUG 0
#define M5STACK_FIRE_NEO_NUM_LEDS 10
#define M5STACK_FIRE_NEO_DATA_PIN 15

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(
  M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN, NEO_GRB + NEO_KHZ800);

WebServer server(80);
Preferences prefs;
bool serverRunning = false;
bool wifiRunning = true;

bool lastButtonBState = HIGH;

const char* auth_user = "admin";         // Login 192.168.4.1
const char* auth_pass = "secureaccess";  // Mot de passe

//acces wifi
const char* ssidgtway = "internal_server";  //gateway wifi
const char* passwordgtway = "passe123";

String ssidOptions;

String strwifissid;
String strwifipass;
String appkeysinric;
String appsecretsinric;
String appswitchidsinric;
String stridwatch;
String ministridwatch;

int MAJdonnees = 0;
int flipconfig = 0;
int flipconfigidwatch = 0;

unsigned long previousMillis = 0;
unsigned long previousMillisLogo = 0;
unsigned long previousMillisButton = 0;
unsigned long previousMillisButtonwatch = 0;

const long interval = 5000;  // Vérifie toutes les 5 secondes
int change = 0;
unsigned long currentMillis = 0;
int buttonLevel = 0;
int i = 0;
bool bool_LittleFS_error = false;
int buttonAlarm = 0;      // variable pour le bouton alarme
int buttonwebserver = 0;  // variable pour le bouton webserver interne
int buttonidwatch = 0;    // variable pour le bouton id montre
int activeWifiLoop = 1;
int buttonAction = 0;


//audio
AudioGeneratorMP3* mp3;
AudioFileSourceLittleFS* file;
AudioOutputI2S* out;
AudioFileSourceID3* id3;


//FUNTIONS
void ledRouge(void);
void ledVerte(void);
void ledBleu(void);
void ledOff(void);
void runappel(void);


/*
    .___                        ____.                     
  __| _/___________ __  _  __  |    |_____   ____   ____  
 / __ |\_  __ \__  \\ \/ \/ /  |    \____ \_/ __ \ / ___\ 
/ /_/ | |  | \// __ \\     /\__|    |  |_> >  ___// /_/  >
\____ | |__|  (____  /\/\_/\________|   __/ \___  >___  / 
     \/            \/               |__|        \/_____/  
*/
void drawJpeg(const char* filename, int xpos, int ypos) {
  if (UART_LOG == 1) {
    Serial.printf("🖼️ Chargement de : %s\n", filename);
  }
  // Vérifie que le fichier existe
  if (!LittleFS.exists(filename)) {
    if (UART_LOG == 1) {
      Serial.println("❌ Fichier JPG introuvable !");
    }
    return;
  }

  // Décode le fichier JPEG
  if (JpegDec.decodeFsFile(filename) != 1) {
    if (UART_LOG == 1) {
      Serial.println("❌ Erreur de décodage JPEG !");
    }
    return;
  }

  // Récupère les dimensions d’un bloc (MCU)
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width + xpos;
  uint32_t max_y = JpegDec.height + ypos;

  // Buffer pointeur vers l’image
  uint16_t* pImg;

  while (JpegDec.read()) {
    pImg = JpegDec.pImage;
    uint32_t mcu_x = JpegDec.MCUx * mcu_w + xpos;
    uint32_t mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if ((mcu_x + mcu_w) <= max_x && (mcu_y + mcu_h) <= max_y) {
      // 🌀 Correction des couleurs : swap bytes pour pushImage()
      for (int i = 0; i < mcu_w * mcu_h; i++) {
        uint16_t color = pImg[i];
        pImg[i] = (color >> 8) | (color << 8);
      }

      M5.Lcd.pushImage(mcu_x, mcu_y, mcu_w, mcu_h, pImg);
    } else {
      // Affichage manuel si bloc tronqué
      for (int y = 0; y < mcu_h; y++) {
        for (int x = 0; x < mcu_w; x++) {
          int px = mcu_x + x;
          int py = mcu_y + y;
          if (px < max_x && py < max_y) {
            uint16_t color = *pImg++;
            color = (color >> 8) | (color << 8);  // correction couleur
            M5.Lcd.drawPixel(px, py, color);
          } else {
            pImg++;
          }
        }
      }
    }
  }
  if (UART_LOG == 1) {
    Serial.println("✅ Image mise à jour !");
  }
}



/*
___.           __    __                       .__                        
\_ |__  __ ___/  |__/  |_  ____   ____ _____  |  | _____ _______  _____  
 | __ \|  |  \   __\   __\/  _ \ /    \\__  \ |  | \__  \\_  __ \/     \ 
 | \_\ \  |  /|  |  |  | (  <_> )   |  \/ __ \|  |__/ __ \|  | \/  Y Y  \
 |___  /____/ |__|  |__|  \____/|___|  (____  /____(____  /__|  |__|_|  /
     \/                              \/     \/          \/            \/ 
*/
void buttonalarm(void) {
  buttonAlarm = digitalRead(BUTTON_ALARM);
  if (buttonAlarm == 0) {
    buttonAction = 1;
    if (millis() >= previousMillisButton) {
      previousMillisButton = millis() + WAIT_BUTTON;
      //change = 1;
      ledRouge();
      if (UART_LOG == 1) {
        Serial.print("\n🔳 Bouton alarm appuyé - Action ");
        Serial.println(buttonAction);
      }
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/alarm2.jpg", 0, 0);  // adapte x, y selon besoin
      pinMode(RELAYPIN_WATCH, OUTPUT);
      digitalWrite(RELAYPIN_WATCH, LOW);
      //delay(2000);

      //appel en cours
      runappel();

      pinMode(RELAYPIN_WATCH, INPUT);
      digitalWrite(RELAYPIN_WATCH, LOW);
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/logo3.jpg", 0, 0);  // adapte x, y selon besoin
      delay(50);
      ledOff();
    }
  }
}



/*
___.           __    __                                __         .__     
\_ |__  __ ___/  |__/  |_  ____   ______  _  _______ _/  |_  ____ |  |__  
 | __ \|  |  \   __\   __\/  _ \ /    \ \/ \/ /\__  \\   __\/ ___\|  |  \ 
 | \_\ \  |  /|  |  |  | (  <_> )   |  \     /  / __ \|  | \  \___|   Y  \
 |___  /____/ |__|  |__|  \____/|___|  /\/\_/  (____  /__|  \___  >___|  /
     \/                              \/             \/          \/     \/ 
*/
void buttonwatch(void) {
  buttonidwatch = digitalRead(BUTTON_IDWATCH);
  if (buttonidwatch == 0) {
    if (millis() >= previousMillisButton) {
      previousMillisButton = millis() + WAIT_BUTTON;
      previousMillisButtonwatch = millis() + WAIT_WATCH;

      if (buttonAction == 0) {
        buttonAction = 2;
      } else if (buttonAction == 1) {
        buttonAction = 2;
      } else if (buttonAction == 2) {
        buttonAction = 1;
      }

      //change = 1;
      ledBleu();
      if (UART_LOG == 1) {
        Serial.print("\n🔳 Bouton ID WATCH appuyé - Action ");
        Serial.println(buttonAction);
      }

      //traitement matrix et affichage
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/logo_mini.jpg", 13, 13);  // adapte x, y selon besoin
      // Set text colour to orange with black background
      M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      //M5.Lcd.setCursor(23, 191);
      //M5.Lcd.printf("%s", stridwatch.c_str());
      M5.Lcd.setFreeFont(FF18);  // Select the font
      M5.Lcd.drawString("SMILE id:", 23, 152, GFXFF);


      M5.Lcd.drawString(stridwatch.c_str(), 23, 191, GFXFF);
      ministridwatch = stridwatch.substring(stridwatch.length() - 4);

      M5.Lcd.setFreeFont(FF24);  // Select the font
      M5.Lcd.drawString(ministridwatch.c_str(), 190, 191, GFXFF);

      M5.Lcd.qrcode(
        stridwatch, 148, 31, 150,
        1);  // Create a QR code with a width of 150 QR code with version 6 at
             // (0, 0).
    }
  }
}


/*
             __________                           _________ __          __         ____ 
  ____   ____\______   \______  _  __ ___________/   _____//  |______ _/  |_  ____/_   |
 /  _ \ /    \|     ___/  _ \ \/ \/ // __ \_  __ \_____  \\   __\__  \\   __\/ __ \|   |
(  <_> )   |  \    |  (  <_> )     /\  ___/|  | \/        \|  |  / __ \|  | \  ___/|   |
 \____/|___|  /____|   \____/ \/\_/  \___  >__| /_______  /|__| (____  /__|  \___  >___|
            \/                           \/             \/           \/          \/     
*/
bool onPowerState1(const String& deviceId, bool& state) {
  Serial.printf("📢 STATUS APPEL SERVICE %s", state ? "ACTIF\n" : "ARRET\n");
  //digitalWrite(RELAYPIN_WATCH, state ? HIGH : LOW);
  if (state) change = 1;
  return true;  // request handled properly
}


/*
               __               __      __._____________.__ 
  ______ _____/  |_ __ ________/  \    /  \__\_   _____/|__|
 /  ___// __ \   __\  |  \____ \   \/\/   /  ||    __)  |  |
 \___ \\  ___/|  | |  |  /  |_> >        /|  ||     \   |  |
/____  >\___  >__| |____/|   __/ \__/\  / |__|\___  /   |__|
     \/     \/           |__|         \/          \/        
*/
// setup function for WiFi connection
void setupWiFi(void) {

  //esp_task_wdt_reset();  // 👈 important ! réinitialise le compteur watchdog

  Serial.println("🔌 Wi-Fi déconnecté, reconnexion en cours...");
  if (UART_FLUSH == 1) Serial.flush();

  WiFi.disconnect();

  WiFi.begin(strwifissid, strwifipass);
  delay(100);

  M5.Lcd.fillScreen(BLACK);
  drawJpeg("/nowifi2.jpg", 0, 0);  // adapte x, y selon besoin

  for (i = 0; i < 200; i++) {
    buttonalarm();
    buttonwatch();
    server.handleClient();

    //button alarm pressed
    if (buttonAction == 1) {
      buttonAction = 0;
      delay(1000);
      ledRouge();
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/nowifi2.jpg", 0, 0);  // adapte x, y selon besoin
    }

    //button watch pressed
    if (buttonAction == 2) {
      if (millis() >= previousMillisButtonwatch) {
        buttonAction = 0;
        delay(1000);
        ledRouge();
        M5.Lcd.fillScreen(BLACK);
        drawJpeg("/nowifi2.jpg", 0, 0);  // adapte x, y selon besoin
      }
    }

    delay(10);
  }

  //esp_task_wdt_reset();  // 👈 important ! réinitialise le compteur watchdog

  while (WiFi.status() != WL_CONNECTED) {

    buttonwebserver = digitalRead(BUTTON_WEBSERVER);
    if (buttonwebserver == 0) {
      //passe en mode webserver
      serverRunning = true;
      wifiRunning = false;
      WiFi.disconnect();
      break;
    }

    i++;
    server.handleClient();
    if (i % 50 == 0) {
      WiFi.disconnect();
      WiFi.begin(strwifissid, strwifipass);
      delay(100);
      Serial.println();
      //esp_task_wdt_reset();  // 👈 important ! réinitialise le compteur watchdog
    }
    Serial.printf(".");

    buttonalarm();
    buttonwatch();

    //buttoon alarm pressed
    if (buttonAction == 1) {
      buttonAction = 0;
      delay(1000);
      ledRouge();
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/nowifi2.jpg", 0, 0);  // adapte x, y selon besoin
    }

    //button watch pressed
    if (buttonAction == 2) {
      if (millis() >= previousMillisButtonwatch) {
        buttonAction = 0;
        delay(1000);
        ledRouge();
        M5.Lcd.fillScreen(BLACK);
        drawJpeg("/nowifi2.jpg", 0, 0);  // adapte x, y selon besoin
      }
    }
    delay(100);
  }

  if (wifiRunning == true) {
    Serial.printf("🛜 connected : IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
    M5.Lcd.fillScreen(BLACK);
    drawJpeg("/wifiok2.jpg", 0, 0);  // adapte x, y selon besoin
    delay(2000);
  }
}


/*
               __                _________.__             .__      __________                
  ______ _____/  |_ __ ________ /   _____/|__| ___________|__| ____\______   \_______  ____  
 /  ___// __ \   __\  |  \____ \\_____  \ |  |/    \_  __ \  |/ ___\|     ___/\_  __ \/  _ \ 
 \___ \\  ___/|  | |  |  /  |_> >        \|  |   |  \  | \/  \  \___|    |     |  | \(  <_> )
/____  >\___  >__| |____/|   __/_______  /|__|___|  /__|  |__|\___  >____|     |__|   \____/ 
     \/     \/           |__|          \/         \/              \/                         
*/
// setup function for SinricPro


void setupSinricPro() {
#ifdef SINRICPRO

  Serial.println("Chargement serveur SinricPro");

  if (appkeysinric.length() == 0 || appsecretsinric.length() == 0 || appswitchidsinric.length() == 0) {
    Serial.println("❌ Erreur : clés SinricPro manquantes !");
    return;
  }

  if (stridwatch.length() == 0) {
    Serial.println("❌ Erreur : numéro identifiant montre manquante !");
    return;
  }

  if (UART_LOG == 1) {
    Serial.print("appkeysinric:");
    Serial.println(appkeysinric);
    Serial.print("appsecretsinric:");
    Serial.println(appsecretsinric);
    Serial.print("appswitchidsinric:");
    Serial.println(appswitchidsinric);
    Serial.print("stridwatch:");
    Serial.println(stridwatch);
  }

  // add devices and callbacks to SinricPro
  pinMode(RELAYPIN_WATCH, INPUT);
  digitalWrite(RELAYPIN_WATCH, LOW);

  SinricProSwitch& mySwitch1 = SinricPro[appswitchidsinric];
  mySwitch1.onPowerState(onPowerState1);

  // setup SinricPro
  SinricPro.onConnected([]() {
    Serial.printf("🌐✔️ Connected to SinricPro\r\n");
    ledVerte();
    M5.Lcd.fillScreen(BLACK);
    drawJpeg("/internetok2.jpg", 0, 0);  // adapte x, y selon besoin
    delay(2000);
    ledVerte();
    change = 3;
  });

  SinricPro.onDisconnected([]() {
    Serial.printf("🌐❌ Disconnected from SinricPro\r\n");
    M5.Lcd.fillScreen(BLACK);
    ledRouge();
    drawJpeg("/nointernet2.jpg", 0, 0);  // adapte x, y selon besoin
    delay(2000);
    change = 0;
  });

  SinricPro.begin(appkeysinric, appsecretsinric);
#endif
}



/*
          __                 __   _________                                
  _______/  |______ ________/  |_/   _____/ ______________  __ ___________ 
 /  ___/\   __\__  \\_  __ \   __\_____  \_/ __ \_  __ \  \/ // __ \_  __ \
 \___ \  |  |  / __ \|  | \/|  | /        \  ___/|  | \/\   /\  ___/|  | \/
/____  > |__| (____  /__|   |__|/_______  /\___  >__|    \_/  \___  >__|   
     \/            \/                   \/     \/                 \/       
*/
void startServer() {

  serverRunning = true;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssidgtway, passwordgtway);
  delay(20);

  scanNetworks();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleStyle);
  server.on("/logo.png", HTTP_GET, []() {
    handleImage("/logo.png", "image/png");
  });
  server.on("/fond.jpg", HTTP_GET, []() {
    handleImage("/fond.jpg", "image/jpeg");
  });
  server.on("/save", HTTP_POST, handleSave);

  server.begin();
  Serial.println("Serveur HTTP sécurisé lancé sur port 80");
}


/*
          __                 _________                                
  _______/  |_  ____ ______ /   _____/ ______________  __ ___________ 
 /  ___/\   __\/  _ \\____ \\_____  \_/ __ \_  __ \  \/ // __ \_  __ \
 \___ \  |  | (  <_> )  |_> >        \  ___/|  | \/\   /\  ___/|  | \/
/____  > |__|  \____/|   __/_______  /\___  >__|    \_/  \___  >__|   
     \/              |__|          \/     \/                 \/       
*/
void stopServer() {
  server.stop();
  serverRunning = false;
  Serial.println("Serveur HTTP sécurisé ARRÊTÉ");
}



/*
.__  .__          __  .____    .__  __    __  .__        ____________________
|  | |__| _______/  |_|    |   |__|/  |__/  |_|  |   ____\_   _____/   _____/
|  | |  |/  ___/\   __\    |   |  \   __\   __\  | _/ __ \|    __) \_____  \ 
|  |_|  |\___ \  |  | |    |___|  ||  |  |  | |  |_\  ___/|     \  /        \
|____/__/____  > |__| |_______ \__||__|  |__| |____/\___  >___  / /_______  /
             \/               \/                        \/    \/          \/ 
*/
void listLittleFS(const char* dirname = "/") {
  Serial.println("📂 Liste des fichiers LittleFS :");

  File root = LittleFS.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("❌ Impossible d’ouvrir LittleFS ou ce n’est pas un dossier !");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      Serial.print("📄 ");
      Serial.print(file.name());
      Serial.print("\t");
      Serial.print(file.size());
      Serial.println(" octets");
      //Serial.flush();
    }
    file = root.openNextFile();
  }
}



/*
                              _______          __                       __            
  ______ ____ _____    ____   \      \   _____/  |___  _  _____________|  | __  ______
 /  ___// ___\\__  \  /    \  /   |   \_/ __ \   __\ \/ \/ /  _ \_  __ \  |/ / /  ___/
 \___ \\  \___ / __ \|   |  \/    |    \  ___/|  |  \     (  <_> )  | \/    <  \___ \ 
/____  >\___  >____  /___|  /\____|__  /\___  >__|   \/\_/ \____/|__|  |__|_ \/____  >
     \/     \/     \/     \/         \/     \/                              \/     \/ 
*/
void scanNetworks() {
  int n = WiFi.scanNetworks();
  ssidOptions = "";
  for (i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    ssidOptions += "<option value=\"" + ssid + "\">" + ssid + "</option>\n";
  }
  Serial.println(ssidOptions);
}



/*
.__            __               __   _________ _________.___________   ________          __  .__                      
|__| ____     |__| ____   _____/  |_/   _____//   _____/|   \______ \  \_____  \ _______/  |_|__| ____   ____   ______
|  |/    \    |  |/ __ \_/ ___\   __\_____  \ \_____  \ |   ||    |  \  /   |   \\____ \   __\  |/  _ \ /    \ /  ___/
|  |   |  \   |  \  ___/\  \___|  | /        \/        \|   ||    `   \/    |    \  |_> >  | |  (  <_> )   |  \\___ \ 
|__|___|  /\__|  |\___  >\___  >__|/_______  /_______  /|___/_______  /\_______  /   __/|__| |__|\____/|___|  /____  >
        \/\______|    \/     \/            \/        \/             \/         \/|__|                       \/     \/ 
*/
// ⬇️ Injection des <option> SSID dans la page HTML
String injectSSIDOptions(String html) {
  int start = html.indexOf("<datalist id=\"ssid_list\">");
  if (start == -1) return html;

  int end = html.indexOf("</datalist>", start);
  if (end == -1) return html;

  String before = html.substring(0, end);
  String after = html.substring(end);
  return before + ssidOptions + after;
}



/*
.__                       .___.__        __________               __   
|  |__ _____    ____    __| _/|  |   ____\______   \ ____   _____/  |_ 
|  |  \\__  \  /    \  / __ | |  | _/ __ \|       _//  _ \ /  _ \   __\
|   Y  \/ __ \|   |  \/ /_/ | |  |_\  ___/|    |   (  <_> |  <_> )  |  
|___|  (____  /___|  /\____ | |____/\___  >____|_  /\____/ \____/|__|  
     \/     \/     \/      \/           \/       \/                    
*/
// ⬇️ Page principale protégée
void handleRoot() {
  if (!server.authenticate(auth_user, auth_pass)) {
    return server.requestAuthentication();
  }

  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Erreur de chargement de la page.");
    return;
  }

  String html = file.readString();
  file.close();

  html = injectSSIDOptions(html);
  server.send(200, "text/html", html);
}



/*
.__                       .___.__           _________ __          .__          
|  |__ _____    ____    __| _/|  |   ____  /   _____//  |_ ___.__.|  |   ____  
|  |  \\__  \  /    \  / __ | |  | _/ __ \ \_____  \\   __<   |  ||  | _/ __ \ 
|   Y  \/ __ \|   |  \/ /_/ | |  |_\  ___/ /        \|  |  \___  ||  |_\  ___/ 
|___|  (____  /___|  /\____ | |____/\___  >_______  /|__|  / ____||____/\___  >
     \/     \/     \/      \/           \/        \/       \/               \/ 
**/
// ⬇️ CSS sécurisé
void handleStyle() {
  if (!server.authenticate(auth_user, auth_pass)) {
    return server.requestAuthentication();
  }

  File file = LittleFS.open("/style.css", "r");
  if (!file) {
    server.send(404, "text/plain", "Fichier CSS non trouvé");
    return;
  }

  server.streamFile(file, "text/css");
  file.close();
}



/*
.__                       .___.__         .___                               
|  |__ _____    ____    __| _/|  |   ____ |   | _____ _____     ____   ____  
|  |  \\__  \  /    \  / __ | |  | _/ __ \|   |/     \\__  \   / ___\_/ __ \ 
|   Y  \/ __ \|   |  \/ /_/ | |  |_\  ___/|   |  Y Y  \/ __ \_/ /_/  >  ___/ 
|___|  (____  /___|  /\____ | |____/\___  >___|__|_|  (____  /\___  / \___  >
     \/     \/     \/      \/           \/          \/     \//_____/      \/ 
*/
// ⬇️ Gestion des images (fond + logo) protégée
void handleImage(String path, String type) {
  if (!server.authenticate(auth_user, auth_pass)) {
    return server.requestAuthentication();
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    server.send(404, "text/plain", "Image non trouvée");
    return;
  }

  server.sendHeader("Cache-Control", "public, max-age=31536000");  // 1 an
  server.streamFile(file, type);
  file.close();
}



/*
.__                       .___.__           _________                   
|  |__ _____    ____    __| _/|  |   ____  /   _____/____ ___  __ ____  
|  |  \\__  \  /    \  / __ | |  | _/ __ \ \_____  \\__  \\  \/ // __ \ 
|   Y  \/ __ \|   |  \/ /_/ | |  |_\  ___/ /        \/ __ \\   /\  ___/ 
|___|  (____  /___|  /\____ | |____/\___  >_______  (____  /\_/  \___  >
     \/     \/     \/      \/           \/        \/     \/          \/ 
*/
// ⬇️ Traitement du formulaire et sauvegarde en Preferences
void handleSave() {
  if (!server.authenticate(auth_user, auth_pass)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Méthode non autorisée");
    return;
  }

  MAJdonnees = 1;

  prefs.begin("wifi", false);
  prefs.putString("ssid", server.arg("ssid"));
  prefs.putString("pass", server.arg("password"));
  prefs.putString("app_key", server.arg("app_key"));
  prefs.putString("app_secret", server.arg("app_secret"));
  prefs.putString("device_id", server.arg("device_id"));
  prefs.putString("id_watch", server.arg("id_watch"));
  prefs.end();

  server.send(200, "text/html", "<h2>Donnees enregistrees</h2><a href='/'>Retour</a>");
}



/*
               __                
  ______ _____/  |_ __ ________  
 /  ___// __ \   __\  |  \____ \ 
 \___ \\  ___/|  | |  |  /  |_> >
/____  >\___  >__| |____/|   __/ 
     \/     \/           |__|    
*/
void setup() {
  M5.begin();

  Serial.begin(BAUD_RATE);
  delay(20);
  Serial.printf("\n🚀 DEMARRAGE ALERTE\r\n\r\n");

  //led neopixel
  pixels.begin();
  delay(10);
  ledRouge();

  if (!LittleFS.begin(true)) {
    Serial.println("❌ LittleFS mount failed!");
    return;
  }
  Serial.println("✅ LittleFS mount successful");
  delay(100);

  //afficher le logo
  // Écran
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);

  // Affiche l'image JPG
  drawJpeg("/logo3.jpg", 0, 0);  // adapte x, y selon besoin

  // Affiche la liste des fichiers
  //listLittleFS();
  //Serial.println("✅ !FIN Lecture de la liste des fichiers LittleFS");

  // Charger les valeurs Preferences
  prefs.begin("wifi", true);
  strwifissid = prefs.getString("ssid", "ssid");
  strwifipass = prefs.getString("pass", "passwordssid");
  appkeysinric = prefs.getString("app_key", "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
  appsecretsinric = prefs.getString("app_secret", "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
  appswitchidsinric = prefs.getString("device_id", "xxxxxxxxxxxxxxxxxxxxxxxx");
  stridwatch = prefs.getString("id_watch", "xxxxxxxxxx");
  prefs.end();

  if (UART_LOG == 1) {
    Serial.print("strwifissid:");
    Serial.println(strwifissid);
    Serial.print("strwifipass:");
    Serial.println(strwifipass);
    Serial.print("appkeysinric:");
    Serial.println(appkeysinric);
    Serial.print("appsecretsinric:");
    Serial.println(appsecretsinric);
    Serial.print("appswitchidsinric:");
    Serial.println(appswitchidsinric);
    Serial.print("stridwatch:");
    Serial.println(stridwatch);
  }

  // Mode AP + Station
  WiFi.mode(WIFI_AP_STA);

  //start server interne
  //startServer();

  if (UART_LOG == 1) {
    Serial.println("🔌 Wi-Fi déconnecté, premiére connexion en cours...");
  }
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  delay(100);

  //init wifi
  ledRouge();
  setupWiFi();

//init sinric
#ifdef SINRICPRO
  setupSinricPro();
#endif
}



/*
.__                        
|  |   ____   ____ ______  
|  |  /  _ \ /  _ \\____ \ 
|  |_(  <_> |  <_> )  |_> >
|____/\____/ \____/|   __/ 
                   |__|    
*/
void loop() {

  M5.update();

  if (M5.BtnA.wasReleased() || M5.BtnA.pressedFor(1000, 200)) {
    //button force alarm
    buttonAlarm = 0;
    ledRouge();

  } else if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(1000, 200)) {
    //start server interne
    flipconfig ^= 0x01;
    change = 3;
    if (flipconfig == 1) {
      serverRunning = true;
      wifiRunning = false;
      WiFi.disconnect();
      startServer();
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/config.jpg", 0, 0);  // adapte x, y selon besoin
      ledBleu();
    } else {
      serverRunning = false;
      wifiRunning = true;
      stopServer();
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/logo3.jpg", 0, 0);  // adapte x, y selon besoin
      ledOff();
    }


  } else if (M5.BtnC.wasReleased() || M5.BtnC.pressedFor(1000, 200)) {
    //button C : button watch pressed flipconfigidwatch
    flipconfigidwatch ^= 0x01;

    if (flipconfigidwatch == 1) {
    } else {
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/logo3.jpg", 0, 0);  // adapte x, y selon besoin
      ledOff();
    }
  }

  //button watch pressed
  if (flipconfigidwatch == 1) {
    if (millis() >= previousMillisButtonwatch) {
      flipconfigidwatch = 0;
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/logo3.jpg", 0, 0);  // adapte x, y selon besoin
      ledOff();
    }
  }

  if (serverRunning == true) {
    server.handleClient();
  }

  // Vérifie toutes les X secondes si le Wi-Fi est toujours connecté
  if (WiFi.status() != WL_CONNECTED && (wifiRunning == true)) {
    ledRouge();
    //start server interne
    //startServer();
    setupWiFi();
  }

  if (wifiRunning == true) {
#ifdef SINRICPRO
    SinricPro.handle();
#endif
  }

  buttonalarm();
  buttonwatch();

  //mise à jour données
  if (MAJdonnees == 1) {
    MAJdonnees == 0;

    // Charger les valeurs Preferences
    prefs.begin("wifi", true);
    strwifissid = prefs.getString("ssid", "ssid");
    strwifipass = prefs.getString("pass", "passwordssid");
    appkeysinric = prefs.getString("app_key", "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
    appsecretsinric = prefs.getString("app_secret", "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
    appswitchidsinric = prefs.getString("device_id", "xxxxxxxxxxxxxxxxxxxxxxxx");
    stridwatch = prefs.getString("id_watch", "xxxxxxxxxx");
    prefs.end();
  }

  if (millis() >= previousMillis) {
    previousMillis = millis() + WAIT_BLINK;

    buttonLevel = digitalRead(RELAYPIN_WATCH);
    //Serial.print(buttonLevel);
    //Serial.print(",");
    //Serial.println(activeWifiLoop);

    if (change == 2) {
      pinMode(RELAYPIN_WATCH, INPUT);
      digitalWrite(RELAYPIN_WATCH, LOW);
      change = 3;

      //appel en cours
      runappel();

      //M5.Lcd.fillScreen(BLACK);
      //drawJpeg("/alarm1.jpg", 0, 0);  // adapte x, y selon besoin
    }

    if (change == 1) {
      pinMode(RELAYPIN_WATCH, OUTPUT);
      digitalWrite(RELAYPIN_WATCH, LOW);
      ledRouge();
      change = 2;
      M5.Lcd.fillScreen(BLACK);
      drawJpeg("/alarm2.jpg", 0, 0);  // adapte x, y selon besoin
      previousMillisLogo = millis() + WAIT_LOGO;
    }
  }

  if (millis() >= previousMillisLogo && (change == 3)) {
    change = 0;
    M5.Lcd.fillScreen(BLACK);

    if (wifiRunning == false) {
      drawJpeg("/config.jpg", 0, 0);  // adapte x, y selon besoin
      delay(50);
      ledBleu();
      delay(20);
    } else {
      drawJpeg("/logo3.jpg", 0, 0);  // adapte x, y selon besoin
      delay(50);
      ledOff();
      delay(20);
    }
  }
}



/*
.__             ._____________                            
|  |   ____   __| _/\______   \ ____  __ __  ____   ____  
|  | _/ __ \ / __ |  |       _//  _ \|  |  \/ ___\_/ __ \ 
|  |_\  ___// /_/ |  |    |   (  <_> )  |  / /_/  >  ___/ 
|____/\___  >____ |  |____|_  /\____/|____/\___  / \___  >
          \/     \/         \/            /_____/      \/ 
*/
void ledRouge(void) {
  for (i = 0; i < 10; i++) {
    pixels.setPixelColor(i, pixels.Color(150, 0, 0));
  }
  pixels.show();
  delay(20);
}


/*
.__             ._______   ____             __          
|  |   ____   __| _/\   \ /   /____________/  |_  ____  
|  | _/ __ \ / __ |  \   Y   // __ \_  __ \   __\/ __ \ 
|  |_\  ___// /_/ |   \     /\  ___/|  | \/|  | \  ___/ 
|____/\___  >____ |    \___/  \___  >__|   |__|  \___  >
          \/     \/               \/                 \/ 
*/
void ledVerte(void) {
  for (i = 0; i < 10; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
  }
  pixels.show();
  delay(20);
}


/*
.__             .___________   _____  _____ 
|  |   ____   __| _/\_____  \_/ ____\/ ____\
|  | _/ __ \ / __ |  /   |   \   __\\   __\ 
|  |_\  ___// /_/ | /    |    \  |   |  |   
|____/\___  >____ | \_______  /__|   |__|   
          \/     \/         \/              
*/
void ledOff(void) {
  for (i = 0; i < 10; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
  delay(20);
}


/*
.__             ._____________.__                
|  |   ____   __| _/\______   \  |   ____  __ __ 
|  | _/ __ \ / __ |  |    |  _/  | _/ __ \|  |  \
|  |_\  ___// /_/ |  |    |   \  |_\  ___/|  |  /
|____/\___  >____ |  |______  /____/\___  >____/ 
          \/     \/         \/          \/       
*/
void ledBleu(void) {
  for (i = 0; i < 10; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 150));
  }
  pixels.show();
  delay(20);
}


void runappel(void) {
  file = new AudioFileSourceLittleFS("/bip.mp3");
  id3 = new AudioFileSourceID3(file);
  out = new AudioOutputI2S(0, 1);  // Output to builtInDAC
  out->SetOutputModeMono(true);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);

  while (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  }
  Serial.printf("MP3 done\n");
}