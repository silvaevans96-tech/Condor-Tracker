# Condor Tracker Arduino

Proyecto open-source para rastreo de cóndores grandes con **ESP32-S3** y **T-SIM7080G**, reescrito en **Arduino C++**.

## Descripción

Este proyecto monitoriza el vuelo de cóndores y registra:

- altitud
- distancia recorrida
- velocidad máxima
- posición GPS
- registro periódico cada 5 horas
- envío de datos a la app
- entrada en deep sleep entre ciclos

## Hardware

- ESP32-S3
- Módulo T-SIM7080G (GSM + GNSS)
- Antena GSM
- Antena GNSS
- Batería LiPo
- Alimentación estable para la placa

## Stack

- Arduino IDE / PlatformIO
- C++ para ESP32-S3
- T-SIM7080G como modem GSM/GNSS
- HardwareSerial para lectura NMEA
- Deep sleep del ESP32-S3

## Estructura del proyecto

```text
Condor-Tracker/
├── README.md
├── platformio.ini
├── src/
│   ├── config.h
│   └── main.cpp
├── LICENSE
└── requirements.txt
```

## Configuración rápida

1. Instala PlatformIO o Arduino IDE.
2. Abre esta carpeta como proyecto.
3. Ajusta los valores de `src/config.h`:
   - APN del operador
   - endpoint de la app
   - token de autenticación
   - pines UART
4. Compila y sube el firmware al ESP32-S3.
5. El dispositivo recogerá coordenadas, calculará distancia y velocidad máxima, enviará los datos cada 5 horas y volverá a dormir.

## Lógica de funcionamiento

1. El ESP32-S3 despierta.
2. Inicializa el modem T-SIM7080G.
3. Lee fijaciones GNSS/NMEA del módulo.
4. Calcula:
   - altitud máxima
   - distancia total recorrida
   - velocidad máxima
5. Tras 5 horas, genera un payload JSON con la información.
6. Envía los datos a la app o backend.
7. Entra en deep sleep para ahorrar batería.

## Payload de ejemplo

```json
{
  "device": "condor-tracker",
  "timestamp": "2026-10-05T14:30:00Z",
  "latitude": 40.416775,
  "longitude": -3.703790,
  "altitude": 1300.0,
  "distance_km": 245.8,
  "max_speed_kmh": 85.3,
  "battery_percent": 75,
  "samples": 720
}
```

## Licencia

MIT

## Nota

Este firmware está pensado como primera compilación para un prototipo funcional. A partir de aquí puedes:

- integrar la API exacta de tu app
- ajustar los tiempos de muestreo
- calibrar la ganancia GPS
- mejorar la gestión de batería para vuelos largos
