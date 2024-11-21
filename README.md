# Controlador de dispositivo Matter

## Descripción general
Este proyecto se centra en el desarrollo de un nodo sensor basado en el estándar Matter (versión 1.3) diseñado para simular valores de consumo energético y gestionar todo el proceso mediante un sistema integral. El proyecto se divide en dos partes:

1. **Desarrollo del nodo sensor**:
   - Implementación en un microcontrolador **ESP32-C6-DevKitM-1**.
   - Programación de una aplicación para simular valores de consumo energético, ajustables según el modo de operación del dispositivo.

2. **Sistema de gestión de datos**:
   - Desarrollo de una interfaz web que permite tanto la visualización de los datos de consumo energético simulados como la modificación del modo de operación del dispositivo para ajustar su consumo simulado.
   - Gestión de los datos mediante una base de datos integrada para almacenamiento y análisis.

## Requisitos previos
Para ejecutar el proyecto se requiere:
- **Entorno de Ubuntu 24.02**.
- **Repositorio Connected Home IP**: clonar desde [https://github.com/project-chip/connectedhomeip](https://github.com/project-chip/connectedhomeip).
- **Repositorio Espressif**: clonar e instalar desde [https://github.com/espressif/esp-idf](https://github.com/espressif/esp-idf).
- **Docker Compose instalado**.

## **Instalación**
1. Descarga los requisitos previos mencionados.
2. Clona los archivos de este repositorio dentro del directorio del repositorio clonado de **Connected Home IP**:

## **Estructura del Proyecto**
```plaintext
electrical-sensor-matter/
│   ├── electrical-sensor-app/    # Aplicación desarrollada para el microcontrolador
│   ├── templates/                # Carpeta donde se guarda el HTML de la interfaz web
│   ├── control_chip-repl.sh      # Shell necesario para ejecutar el proceso de control del dispositivo en Docker
│   ├── control.py                # Código Python encargado de gestionar la comunicación con el dispositivo
│   ├── docker-compose.yml        # Docker Compose configurado para ambos contenedores
│   ├── Dockerfile_chip-repl      # Configuración Docker para el contenedor que gestiona el dispositivo
│   ├── Dockerfile_web            # Configuración Docker para el contenedor de la interfaz web
│   ├── web.py                    # Código Python que gestiona la interfaz web
│   ├── README.md                 # Documentación general del proyecto
```

## **Compilación y Despliegue de la aplicación**
Es fundamental haber completado previamente los pasos descritos en el apartado de requisitos previos y contar con las herramientas esenciales para el desarrollo del proyecto. Adicionalmente, es indispensable disponer de un microcontrolador **ESP32C6-DevKitM-1** para llevar a cabo las implementaciones requeridas. Los pasos a seguir son los siguientes:

1. Activar ESP-IDF:
  ```bash
   get_idf
  ```
2. Configuración modelo específico   
  ```bash
   idf.py set-target esp32c6
  ```
3-Compilaion del proyecto
  ```bash
   idf.py build
  ```
4- Flasheo y monitorizacion
  ```bash
   idf.py flash monitor
  ```

## **Compilación y despliegue del sistema al completo**

#### **1. Configuración Inicial**
1. En primer lugar, se han de haber seguido las instrucciones indicadas previamente y se tiene que haber cargado la aplicación desarollada.

2. Personalizar las credenciales Wi-Fi en el archivo `control.py`
   ```
   WIFI_SSID = "nombre_red"
   WIFI_PASSWORD = "contraseña_red"
   ```
3. Construir los contenedores empleando Docker Compose:
   ```
   docker-compose build
   ```

#### **2. Inicialización del sistema**
Al iniciar el sistema completo con Docker Compose, existen dos opciones: 
   - Realizar el commissioning del dispositivo
   ```
   MODE=1 docker-compose up 
   ```
   -Reconectar al dispositivo (**requiere que haya sido comisionado previamente**)
   ```
   docker-compose up
   ```

#### **3. Interacción con el sistema**
Para interactuar con el sistema, se accede a la interfaz web desde la dirección:
```
192.168.1.38:5000
````
La interfaz web centraliza todas las interacciones, proporcionando una experiencia intuitiva para gestionar y monitorizar el dispositivo Matter.

