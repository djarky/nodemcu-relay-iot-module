#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

#include <ArduinoOTA.h>





//------BLYNK


#define BLYNK_TEMPLATE_ID "TMP"
#define BLYNK_TEMPLATE_NAME "iot luces3"
#define BLYNK_AUTH_TOKEN "tokken"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp8266.h>

BlynkTimer timer;





// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
  // Change Web Link Button message to "Congratulations!"
  Blynk.setProperty(V3, "offImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations.png");
  Blynk.setProperty(V3, "onImageUrl",  "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations_pressed.png");
  Blynk.setProperty(V3, "url", "https://docs.blynk.io/en/getting-started/what-do-i-need-to-blynk/how-quickstart-device-was-made");

  Blynk.syncVirtual(V0, V1); // sincroniza los estados al iniciar

}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent()
{
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V2, millis() / 1000);
}

//----


#define EEPROM_SIZE 512

// -------------------- CONFIG STRUCT --------------------
struct Config {
  uint16_t magic; // para validar
  char ssid[32];
  char password[32];
  uint8_t static_ip_check;
  char static_ip[16];
  char gateway[16];
  char subnet[16];
  char dns1[16];
  char dns2[16];
  uint8_t iot_enable;
  char iot_name[64];
  char iot_key[64];
};



Config config;
ESP8266WebServer server(80);

#include "thingProperties.h"

const int ledPins[] = {5, 4}; // D1, D2
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);
bool ledStates[numLeds] = {false};


BLYNK_WRITE(V0) {
  luz_1 = param.asInt();
  ledStates[0] = luz_1;
  digitalWrite(ledPins[0], luz_1 ? LOW : HIGH);
}

BLYNK_WRITE(V1) {
  luz_2 = param.asInt();
  ledStates[1] = luz_2;
  digitalWrite(ledPins[1], luz_2 ? LOW : HIGH);
}


WiFiConnectionHandler* cloudConnection;
bool apMode = false;

// -------------------- EEPROM --------------------
void saveConfig() {
  config.magic = 0xABCE; // valor arbitrario para indicar config válida
  EEPROM.put(0, config);
  EEPROM.commit();
}


void loadConfig() {
  EEPROM.get(0, config);
  if (config.magic != 0xABCE) {  
    Serial.println("EEPROM no inicializada o datos inválidos. Cargando valores por defecto.");

    strncpy(config.ssid, "TuSSID", sizeof(config.ssid));
    strncpy(config.password, "TuPassword", sizeof(config.password));
    config.static_ip_check = 2;
    strncpy(config.static_ip, "192.168.1.100", sizeof(config.static_ip));
    strncpy(config.gateway, "192.168.1.1", sizeof(config.gateway));
    strncpy(config.subnet, "255.255.255.0", sizeof(config.subnet));
    strncpy(config.dns1, "8.8.8.8", sizeof(config.dns1));
    strncpy(config.dns2, "8.8.4.4", sizeof(config.dns2));
    config.iot_enable = 2;
    strncpy(config.iot_name, "device login name", sizeof(config.iot_name));
    strncpy(config.iot_key, "device key", sizeof(config.iot_key));

    saveConfig();
  } else {
    Serial.println("Configuración cargada desde EEPROM correctamente.");
  }
}

void resetConfig() {
  config.magic = 0; // Invalidar para que loadConfig cargue defaults o asignar directamente
  loadConfig();
  Serial.println("Configuración restablecida a valores por defecto.");
}

void handleATCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.startsWith("AT+SSID=")) {
      String val = input.substring(8);
      strncpy(config.ssid, val.c_str(), sizeof(config.ssid));
      Serial.println("OK: SSID set to " + val);
    } else if (input.startsWith("AT+PASS=")) {
      String val = input.substring(8);
      strncpy(config.password, val.c_str(), sizeof(config.password));
      Serial.println("OK: Password set");
    } else if (input == "AT+SAVE") {
      saveConfig();
      Serial.println("OK: Config saved to EEPROM");
    } else if (input == "AT+RESET") {
      resetConfig();
      saveConfig();
      Serial.println("OK: Config reset and saved");
    } else if (input == "AT+RESTART") {
      Serial.println("OK: Restarting...");
      delay(1000);
      ESP.restart();
    } else if (input == "AT+INFO") {
      Serial.println("--- INFO ---");
      Serial.print("SSID: "); Serial.println(config.ssid);
      Serial.print("Mode: "); Serial.println(apMode ? "AP" : "STA");
      Serial.print("IP: "); Serial.println(apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
      Serial.print("IoT Enable: "); Serial.println(config.iot_enable == 1 ? "Yes" : "No");
      Serial.println("------------");
    } else {
      Serial.println("ERROR: Unknown command");
    }
  }
}




// -------------------- /config PAGE --------------------
void handleConfigPage() {
  String html = R"====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuración</title>
      <style>
        body {
          background: #121212;
          color: #fff;
          font-family: sans-serif;
          padding: 20px;
        }
        input[type=text], input[type=password] {
          background: #121212;
          color: #fff;
          width: 100%;
          padding: 10px;
          margin: 5px 0;
          border: 1px solid #555;
        }
        button {
          padding: 10px 20px;
          background: #00CED1;
          border: none;
          color: #fff;
          cursor: pointer;
        }
        .switch {
          display: flex;
          align-items: center;
          margin: 10px 0;
          font-size: 14px;
        }
        .switch input {
          margin-left: 10px;
          transform: scale(1.2);
        }
        fieldset {
          border: 1px solid #555;
          padding: 10px;
          margin-top: 10px;
        }
        legend {
          color: #00CED1;
        }
      </style>
<script>
  function isValidIP(ip) {
    const parts = ip.split('.');
    if (parts.length !== 4) return false;
    for (const part of parts) {
      const num = Number(part);
      if (isNaN(num) || num < 0 || num > 255) return false;
    }
    return true;
  }

  function validateForm(event) {
    const ssid = document.querySelector('input[name="ssid"]').value.trim();
    const password = document.querySelector('input[name="password"]').value;
    const staticIpCheck = document.getElementById('ip_check').checked;
    const ip = document.querySelector('input[name="ip"]').value.trim();
    const gateway = document.querySelector('input[name="gateway"]').value.trim();
    const subnet = document.querySelector('input[name="subnet"]').value.trim();
    const dns1 = document.querySelector('input[name="dns1"]').value.trim();
    const dns2 = document.querySelector('input[name="dns2"]').value.trim();
    const iotEnable = document.getElementById('iot_enable').checked;
    const iotKey = document.querySelector('input[name="iotkey"]').value.trim();

    if (ssid === '') {
      alert('El SSID no puede estar vacío.');
      event.preventDefault();
      return false;
    }

    if (password.length > 0 && password.length < 8) {
      alert('La contraseña debe tener al menos 8 caracteres o estar vacía.');
      event.preventDefault();
      return false;
    }

    if (staticIpCheck) {
      if (!isValidIP(ip)) {
        alert('IP estática inválida.');
        event.preventDefault();
        return false;
      }
      if (!isValidIP(gateway)) {
        alert('Gateway inválido.');
        event.preventDefault();
        return false;
      }
      if (!isValidIP(subnet)) {
        alert('Subnet inválida.');
        event.preventDefault();
        return false;
      }
      if (dns1.length > 0 && !isValidIP(dns1)) {
        alert('DNS1 inválido.');
        event.preventDefault();
        return false;
      }
      if (dns2.length > 0 && !isValidIP(dns2)) {
        alert('DNS2 inválido.');
        event.preventDefault();
        return false;
      }
    }

    if (iotEnable && iotKey === '') {
      alert('El token IoT no puede estar vacío si IoT está habilitado.');
      event.preventDefault();
      return false;
    }

    return true;
  }

  function toggleStaticIP() {
    const isChecked = document.getElementById('ip_check').checked;
    const fields = document.querySelectorAll('.ip-field');
    fields.forEach(el => el.disabled = !isChecked);
  }

  function toggleIotEnable() {
    const isChecked = document.getElementById('iot_enable').checked;
    document.getElementById('iotname').disabled = !isChecked;
    document.getElementById('iotkey').disabled = !isChecked;
  }

  window.onload = function() {
    toggleStaticIP();
    toggleIotEnable();
    document.querySelector('form').addEventListener('submit', validateForm);
  };
</script>

    </head>
    <body>
      <h2>Configuración de Red e IoT</h2>
      <form action="/saveConfig" method="POST">
        <label>SSID:</label><input name="ssid" value=")====" + String(config.ssid) + R"====(">
        <label>Password:</label><input name="password" type="password" value=")====" + String(config.password) + R"====(">

        <div class="switch">
          <label>Usar IP Estática</label>
          <input type="checkbox" name="ip_check" id="ip_check" onchange="toggleStaticIP()" )====" + String(config.static_ip_check == 1 ? "checked" : "") + R"====(>
        </div>

        <fieldset>
          <legend>Configuración IP</legend>
          <label>IP Estática:</label><input name="ip" class="ip-field" value=")====" + String(config.static_ip) + R"====(">
          <label>Gateway:</label><input name="gateway" class="ip-field" value=")====" + String(config.gateway) + R"====(">
          <label>Subnet:</label><input name="subnet" class="ip-field" value=")====" + String(config.subnet) + R"====(">
          <label>DNS 1:</label><input name="dns1" class="ip-field" value=")====" + String(config.dns1) + R"====(">
          <label>DNS 2:</label><input name="dns2" class="ip-field" value=")====" + String(config.dns2) + R"====(">
        </fieldset>

        <div class="switch">
          <label>Habilitar IoT</label>
          <input type="checkbox" name="iot_enable" id="iot_enable" onchange="toggleIotEnable()" )====" + String(config.iot_enable == 1 ? "checked" : "") + R"====(>
        </div>
        <label>IoT device name:</label>
<input name="iotname" id="iotname" value=")====" + String(config.iot_name) + R"====(">

        <label>IoT Token:</label>
        <input name="iotkey" id="iotkey" value=")====" + String(config.iot_key) + R"====(">
        <p>put device name as "blynk" to blynk server</p>

        <button type="submit">Guardar</button>
      </form>
    </body>
    </html>
  )====";

  server.send(200, "text/html", html);
}


// -------------------- SAVE CONFIG --------------------

void handleSaveConfig() {
  strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid));
  strncpy(config.password, server.arg("password").c_str(), sizeof(config.password));
	config.static_ip_check = (server.hasArg("ip_check") && server.arg("ip_check") == "on") ? 1 : 2;
  strncpy(config.static_ip, server.arg("ip").c_str(), sizeof(config.static_ip));
  strncpy(config.gateway, server.arg("gateway").c_str(), sizeof(config.gateway));
  strncpy(config.subnet, server.arg("subnet").c_str(), sizeof(config.subnet));
  strncpy(config.dns1, server.arg("dns1").c_str(), sizeof(config.dns1));
  strncpy(config.dns2, server.arg("dns2").c_str(), sizeof(config.dns2));
  config.iot_enable = (server.hasArg("iot_enable") && server.arg("iot_enable") == "on") ? 1 : 2;
  strncpy(config.iot_name, server.arg("iotname").c_str(), sizeof(config.iot_name));
  strncpy(config.iot_key, server.arg("iotkey").c_str(), sizeof(config.iot_key));
  saveConfig();
  server.send(200, "text/html", "<h3>Guardado. Reinicia el dispositivo.</h3>");
}





// HTML page handler

void handleRoot() {
  String html = R"=====( 
    <!DOCTYPE html>
    <html lang="es">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Control de Luces</title>
      <style>
        body {
          background-color: #121212;
          color: #E0E0E0;
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          display: flex;
          flex-direction: column;
          align-items: center;
          padding: 20px;
        }

        h1 {
          color: #00CED1;
          margin-bottom: 30px;
          font-size: 2em;
        }

        .button-container {
          display: flex;
          flex-direction: column;
          gap: 20px;
          width: 100%;
          max-width: 400px;
        }

        .button-group {
          display: flex;
          flex-direction: column;
          align-items: center;
        }

        button {
          width: 100px;
          height: 100px;
          border-radius: 50%;
          font-size: 18px;
          border: none;
          background-color: #000000;
          color: #E0E0E0;
          cursor: pointer;
          transition: all 0.3s ease;
          box-shadow: 0 0 10px rgba(0, 206, 209, 0); /* sin resplandor por defecto */
        }

        button.on {
          box-shadow: 0 0 20px rgba(0, 206, 209, 0.7); /* resplandor turquesa */
        }

        p {
          margin-bottom: 10px;
          font-size: 1.1em;
          color: #CCCCCC;
        }

        @media (max-width: 600px) {
          button {
            width: 80px;
            height: 80px;
            font-size: 16px;
          }

          h1 {
            font-size: 1.5em;
          }
        }
      </style>
    </head>
    <body>
      <h1>Control de Luces</h1>
      <div class="button-container" id="buttons"></div>

      <script>
        function fetchStatus() {
          fetch('/status')
            .then(response => response.json())
            .then(data => {
              const container = document.getElementById('buttons');
              container.innerHTML = '';
              data.pins.forEach(pin => {
                const group = document.createElement('div');
                group.className = 'button-group';

                const label = document.createElement('p');
                label.textContent = 'Luz en GPIO ' + pin.pin + ':';
                group.appendChild(label);

                const btn = document.createElement('button');
                btn.textContent = pin.state ? 'Apagar' : 'Encender';
                btn.className = pin.state ? 'on' : '';
                btn.onclick = () => togglePin(pin.pin);

                group.appendChild(btn);
                container.appendChild(group);
              });
            });
        }

        function togglePin(pin) {
          fetch('/toggle?pin=' + pin)
            .then(() => fetchStatus());
        }

        setInterval(fetchStatus, 2000);
        fetchStatus();
      </script>
    </body>
    </html>
  )=====";

  server.send(200, "text/html", html);
}


void handleStatus() {
  String json = "{\"pins\":[";
  for (int i = 0; i < numLeds; i++) {
    json += "{\"pin\":" + String(ledPins[i]) + ",\"state\":" + String(ledStates[i]) + "}";
    if (i < numLeds - 1) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleToggle() {
  if (server.hasArg("pin")) {
    int pin = server.arg("pin").toInt();
    for (int i = 0; i < numLeds; i++) {
      if (ledPins[i] == pin) {
        ledStates[i] = !ledStates[i];
        digitalWrite(pin, ledStates[i] ? LOW : HIGH); // LOW enciende
                 // Sincronizar con IoT Cloud
                // Sincronizar con Blynk
        if (i == 0) {
          luz_1 = ledStates[i];
          Blynk.virtualWrite(V0, luz_1);  // V0 para luz_1
        } 
        if (i == 1) {
          luz_2 = ledStates[i];
          Blynk.virtualWrite(V1, luz_2);  // V1 para luz_2
        }
        break;
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleHelpPage() {
  String html = R"====(
    <!DOCTYPE html>
    <html lang="es">
    <head>
      <meta charset="UTF-8">
      <title>Ayuda - Comandos disponibles</title>
      <style>
        body { background: #121212; color: #E0E0E0; font-family: sans-serif; padding: 20px; }
        h1 { color: #00CED1; }
        video { width: 100%; max-width: 600px; border-radius: 10px; margin-bottom: 20px; }
        table { width:100%; border-collapse:collapse; }
        th,td { border:1px solid #555; padding:10px; }
        th { background:#00CED1; color:#121212; }
        tr:nth-child(even){ background:#1e1e1e; }
      </style>
    </head>
    <body>
      <video controls autoplay loop>
        <source src="https://juan-carlos.info/wp-content/uploads/sites/2/2021/11/Rick-Astley-Never-Gonna-Give-You-Up-Official-Music-Video.mp4" type="video/mp4">
        Tu navegador no soporta videos HTML5.
      </video>

      <h2>📘 Comandos disponibles</h2>
      <table>
        <tr><th>Ruta</th><th>Descripción</th></tr>
        <tr><td><code>/</code></td><td>Interfaz principal para controlar las luces.</td></tr>
        <tr><td><code>/config</code></td><td>Configura Wi‑Fi, IP estática y IoT.</td></tr>
        <tr><td><code>/saveConfig</code></td><td>Guarda la configuración enviada desde <code>/config</code>.</td></tr>
        <tr><td><code>/status</code></td><td>Devuelve JSON con el estado de las luces.</td></tr>
        <tr><td><code>/toggle?pin=GPIO</code></td><td>Cambia el estado del pin indicado.</td></tr>
        <tr><td><code>/help</code></td><td>Esta misma página de ayuda.</td></tr>
      </table>

      <p style="margin-top:20px;">¿Dudas o errores? Reinicia el dispositivo o conéctalo en modo AP para reconfigurar.</p>
    </body>
    </html>
  )====";
  server.send(200, "text/html", html);
}



// -------------------- SETUP --------------------
void setup() {
  system_update_cpu_freq(160);
  os_update_cpu_frequency(160);


  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }

  bool staticIpOk = false;
  if(config.static_ip_check == 1) {
    // IP fija
    IPAddress ip, gw, sn, d1, d2;
    if (ip.fromString(config.static_ip) && gw.fromString(config.gateway) && sn.fromString(config.subnet)) {
      d1.fromString(config.dns1);
      d2.fromString(config.dns2);
 
      WiFi.mode(WIFI_STA);
      if (WiFi.config(ip, gw, sn, d1, d2)) {
        staticIpOk = true;
      } else {
        Serial.println("❌ Fallo crítico en WiFi.config. Usando DHCP.");
      }
    } else {
      Serial.println("❌ Formato de IP estática inválido. Usando DHCP.");
    }
  }

  WiFi.begin(config.ssid, config.password);
  Serial.print("Conectando a WiFi (" + String(config.ssid) + ")");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500); Serial.print(".");
  }

  // Robustness Check: If connected but IP is invalid (0.0.0.0 or 169.254.x.x)
  IPAddress currentIP = WiFi.localIP();
  if (WiFi.status() == WL_CONNECTED && (currentIP[0] == 0 || currentIP[0] == 169)) {
    Serial.println("\n⚠️ IP sospechosa detectada: " + currentIP.toString());
    if (staticIpOk) {
      Serial.println("Reintentando con DHCP automáticamente...");
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      WiFi.begin(config.ssid, config.password);
      start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500); Serial.print(".");
      }
    }
  }

  // OTA Setup (Available in both STA and AP mode)
  ArduinoOTA.setHostname("ESP_Luces"); 
  ArduinoOTA.setPassword("Admin");

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Inicio OTA: " + type);
  });

  ArduinoOTA.onEnd([]() { Serial.println("\nOTA finalizada."); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progreso OTA: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error OTA [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Fallo de autenticación");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Error al comenzar");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Error de conexión");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Error de recepción");
    else if (error == OTA_END_ERROR) Serial.println("Error al finalizar");
  });

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado. IP: " + WiFi.localIP().toString());
    
    // Si la conexión es dudosa (static ip o 169.x.x.x), habilitamos AP de respaldo
    if (staticIpOk || WiFi.localIP()[0] == 169) {
      Serial.println("Iniciando AP de respaldo (Modo Híbrido).");
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP("ConfigESP8266_Safe");
    }

    if(config.iot_enable == 1) {
      if(strcmp(config.iot_name, "blynk") == 0){
          Blynk.begin(config.iot_key,config.ssid, config.password);
          timer.setInterval(1000L, myTimerEvent);
      }
    }
  } else {
    Serial.println("\n❌ No se pudo conectar. Iniciando modo AP.");
    apMode = true;
    WiFi.mode(WIFI_AP_STA); // Híbrido para que intente reconectar en segundo plano
    WiFi.softAP("ConfigESP8266");
    delay(1000);
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }

  ArduinoOTA.begin();
  Serial.println("OTA listo.");

  server.on("/", handleRoot);
  server.on("/config", handleConfigPage);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);
  server.on("/status", handleStatus);
  server.on("/toggle", handleToggle);
  server.on("/help", handleHelpPage);
  server.begin();


}

// -------------------- LOOP --------------------
void loop() {
  server.handleClient();
  handleATCommands();
  ArduinoOTA.handle();

  if (!apMode) {
    if (config.iot_enable == 1) {
      if (strcmp(config.iot_name, "blynk") == 0) {
        Blynk.run();
        timer.run();
      }
    }
  }
}



// -------------------- CALLBACKS --------------------

// Llamadas cuando se cambia el valor desde el Dashboard
void onLuz1Change() {
  ledStates[0] = luz_1;
  digitalWrite(ledPins[0], luz_1 ? LOW : HIGH);
}

void onLuz2Change() {
  ledStates[1] = luz_2;
  digitalWrite(ledPins[1], luz_2 ? LOW : HIGH);
}
