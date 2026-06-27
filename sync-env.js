const fs = require('fs');
const path = require('path');

const envPath = path.join(__dirname, '.env');
const secretsPath = path.join(__dirname, 'secrets.h');

const config = {
  WIFI_SSID: 'Wokwi-GUEST',
  WIFI_PASS: '',
  MQTT_BROKER: 'test.mosquitto.org',
  MQTT_PORT: '1883',
  MQTT_CLIENT: 'esp32-tof-caece-v3'
};

if (fs.existsSync(envPath)) {
  const envContent = fs.readFileSync(envPath, 'utf8');
  const lines = envContent.split('\n');
  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith('#') || trimmed.startsWith('//') || trimmed.startsWith('/*')) {
      continue;
    }
    const parts = trimmed.split('=');
    if (parts.length >= 2) {
      const key = parts[0].trim();
      let val = parts.slice(1).join('=').trim();
      if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
        val = val.slice(1, -1);
      }

      if (key === 'WIFI_SSID' || key === 'WIFI_SSD') {
        config.WIFI_SSID = val;
      } else if (key === 'WIFI_PASS') {
        config.WIFI_PASS = val;
      } else if (key === 'MQTT_BROKER') {
        config.MQTT_BROKER = val;
      } else if (key === 'MQTT_PORT') {
        config.MQTT_PORT = val;
      } else if (key === 'MQTT_CLIENT') {
        config.MQTT_CLIENT = val;
      }
    }
  }
  console.log('Loaded configurations from .env');
} else {
  console.log('.env file not found. Generating secrets.h with default simulation values.');
}

const secretsContent = `// Generated automatically from .env. Do not commit this file.
#ifndef SECRETS_H
#define SECRETS_H

const char *WIFI_SSID = "${config.WIFI_SSID.replace(/"/g, '\\"')}";
const char *WIFI_PASS = "${config.WIFI_PASS.replace(/"/g, '\\"')}";
const char *MQTT_BROKER = "${config.MQTT_BROKER.replace(/"/g, '\\"')}";
const int MQTT_PORT = ${parseInt(config.MQTT_PORT, 10) || 1883};
const char *MQTT_CLIENT = "${config.MQTT_CLIENT.replace(/"/g, '\\"')}";

#endif // SECRETS_H
`;

fs.writeFileSync(secretsPath, secretsContent, 'utf8');
console.log('Successfully wrote secrets.h');
