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

function parseLine(line) {
  const trimmed = line.trim();
  if (!trimmed || trimmed.startsWith('#') || trimmed.startsWith('//') || trimmed.startsWith('/*')) {
    return null;
  }
  const eqIndex = trimmed.indexOf('=');
  if (eqIndex === -1) return null;

  const key = trimmed.slice(0, eqIndex).trim();
  let val = trimmed.slice(eqIndex + 1).trim();

  // Strip trailing semicolon
  if (val.endsWith(';')) val = val.slice(0, -1).trim();

  // Strip surrounding quotes
  if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
    val = val.slice(1, -1);
  }

  return { key, val };
}

if (fs.existsSync(envPath)) {
  const envContent = fs.readFileSync(envPath, 'utf8');
  const lines = envContent.split('\n');
  const cleanLines = [];

  for (const line of lines) {
    const parsed = parseLine(line);
    if (!parsed) {
      cleanLines.push(line);
      continue;
    }

    const { key, val } = parsed;

    // Build clean line for .env rewrite (no quotes, no semicolons)
    cleanLines.push(`${key}=${val}`);

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

  // Rewrite .env sanitized so Docker/shell can read it cleanly
  fs.writeFileSync(envPath, cleanLines.join('\n'), 'utf8');
  console.log('Loaded and sanitized .env (removed semicolons and surrounding quotes)');
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
