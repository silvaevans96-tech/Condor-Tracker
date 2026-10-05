# Condor Tracker 🦅

Sistema de rastreo de cóndores grandes utilizando dispositivo **T-SIM7080G** con **ESP32-S3**.

## Descripción

Condor Tracker es un proyecto open-source en Python diseñado para monitorear y registrar métricas de vuelo de cóndores en tiempo real, incluyendo:

- **Altitud**: Medición continua de altura sobre el nivel del mar
- **Distancia recorrida**: Cálculo de distancia mediante GPS
- **Velocidad máxima**: Registro de la velocidad máxima alcanzada durante el vuelo
- **Transmisión de datos**: Envío de información cada 5 horas a la aplicación
- **Deep Sleep**: Modo de bajo consumo entre transmisiones

## Características principales

✅ Medición de altitud con GPS/GNSS  
✅ Cálculo de distancia usando haversine formula  
✅ Detección y registro de velocidad máxima  
✅ Transmisión de datos cada 5 horas  
✅ Deep sleep mode para optimizar batería  
✅ Logging local de datos  
✅ Compatible con T-SIM7080G + ESP32-S3  

## Requisitos

### Hardware
- ESP32-S3
- Módulo T-SIM7080G (GSM/GNSS)
- Batería LiPo
- Antena GPS/GNSS
- Antena GSM

### Software
- MicroPython 1.20+
- Librería `umachine` (built-in)
- Librería `ujson` (built-in)
- Librerías personalizadas del proyecto

## Estructura del proyecto

```
Condor-Tracker/
├── README.md
├── requirements.txt
├── firmware/
│   ├── main.py              # Código principal
│   ├── boot.py              # Inicialización del dispositivo
│   ├── config.py            # Configuración del proyecto
│   ├── gps_module.py        # Módulo GPS/GNSS
│   ├── gsm_module.py        # Módulo GSM
│   ├── sensors.py           # Sensores del dispositivo
│   ├── data_processor.py    # Procesamiento de datos
│   ├── sleep_manager.py     # Gestor de deep sleep
│   └── logger.py            # Sistema de logging
├── tests/
│   ├── test_gps.py
│   ├── test_gsm.py
│   └── test_data_processor.py
├── docs/
│   ├── INSTALLATION.md      # Guía de instalación
│   ├── API.md               # Documentación de API
│   └── TROUBLESHOOTING.md   # Solución de problemas
└── examples/
    ├── basic_tracking.py
    └── advanced_config.py
```

## Instalación rápida

1. Clona el repositorio:
```bash
git clone https://github.com/silvaevans96-tech/Condor-Tracker.git
cd Condor-Tracker
```

2. Copia los archivos del firmware a tu ESP32-S3:
```bash
ampy --port /dev/ttyUSB0 put firmware/
```

3. Reinista el dispositivo y comienza a rastrear.

## Uso básico

```python
from firmware.main import CondorTracker

# Inicializar el rastreador
tracker = CondorTracker(
    gps_interval=10,  # segundos entre lecturas GPS
    tx_interval=18000,  # 5 horas en segundos
    debug=True
)

# Iniciar rastreo
tracker.start()
```

## Configuración

Edita `firmware/config.py` para personalizar:

```python
# Intervalos de tiempo
GPS_INTERVAL = 10  # segundos
TX_INTERVAL = 18000  # 5 horas = 18000 segundos
SLEEP_MODE = True

# Credenciales GSM
APN = "your_apn"
PHONE = "your_phone"

# Servidor de datos
SERVER_HOST = "your_server.com"
SERVER_PORT = 8080
```

## API de datos

El dispositivo envía JSON en este formato cada 5 horas:

```json
{
  "timestamp": "2026-10-05T14:30:00Z",
  "latitude": -12.0464,
  "longitude": -75.7489,
  "altitude": 3850.5,
  "distance_km": 245.8,
  "max_speed_kmh": 85.3,
  "battery_percent": 75,
  "signal_strength": -95,
  "data_points": 500
}
```

## Contribución

Las contribuciones son bienvenidas. Por favor:

1. Fork el proyecto
2. Crea una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abre un Pull Request

## Licencia

Este proyecto está bajo licencia MIT. Ver `LICENSE` para más detalles.

## Contacto

**Evans y equipo de desarrollo**  
📧 silvaevans96-tech@github.com  
🐦 Twitter: @CondorTracker

---

**Nota**: Este proyecto es parte de un esfuerzo de conservación de cóndores grandes. Los datos recopilados ayudan a entender mejor los patrones de vuelo y el comportamiento de estas especies en peligro de extinción.
