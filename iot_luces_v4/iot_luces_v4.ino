#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>


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
  char iot_key[64];
};



Config config;
ESP8266WebServer server(80);

#include "thingProperties.h"

const int ledPins[] = {5, 4}; // D1, D2
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);
bool ledStates[numLeds] = {false};



WiFiConnectionHandler* cloudConnection;
bool apMode = false;

// -------------------- EEPROM --------------------
void saveConfig() {
  config.magic = 0xABCC; // valor arbitrario para indicar config válida
  EEPROM.put(0, config);
  EEPROM.commit();
}


void loadConfig() {
  EEPROM.get(0, config);
  if (config.magic != 0xABCC) {  
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
    strncpy(config.iot_key, "key", sizeof(config.iot_key));

    // Guarda esta configuración por defecto en EEPROM
    saveConfig();
  } else {
    Serial.println("Configuración cargada desde EEPROM correctamente.");
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

        <label>IoT Token:</label>
        <input name="iotkey" id="iotkey" value=")====" + String(config.iot_key) + R"====(">

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
        if (i == 0) luz_1 = ledStates[i];
        if (i == 1) luz_2 = ledStates[i];
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
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }

  if(config.static_ip_check == 1) {
    // IP fija
    IPAddress ip, gw, sn, d1, d2;
    ip.fromString(config.static_ip);
    gw.fromString(config.gateway);
    sn.fromString(config.subnet);
    d1.fromString(config.dns1);
    d2.fromString(config.dns2);
 

    WiFi.mode(WIFI_STA);
    if (!WiFi.config(ip, gw, sn, d1, d2)) {
      Serial.println("Error en IP fija");
    }

  }

  WiFi.begin(config.ssid, config.password);
  Serial.print("Conectando a WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500); Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado. IP: " + WiFi.localIP().toString());
    
    if(config.iot_enable == 1) {
      initProperties();
      cloudConnection = new WiFiConnectionHandler(config.ssid, config.password);
      ArduinoCloud.begin(*cloudConnection);
    }
    
  } else {
    Serial.println("\n❌ No se pudo conectar. Iniciando modo AP.");
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ConfigESP8266");
    delay(1000);
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }

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
  if (!apMode ) {
    if(config.iot_enable == 1){ArduinoCloud.update();}
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
