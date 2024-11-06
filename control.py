import argparse
import asyncio
import logging
import sqlite3
import subprocess
from datetime import datetime

import pytz
from chip import clusters
from chip.clusters import Objects

#Definir constantes para colores y negrita usando ANSI escape codes
RESET = "\033[0m"  # Color original terminal
BOLD = "\033[1m"   # Negrita
BLUE = "\033[34m"  # Color azul
ORANGE = "\033[38;5;214m" #Color naranja

# Configuración del logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s',force=True)

#Filtro para evitar los logs raiz de chip-repl
class ExcludeChipFilter(logging.Filter):
    """Filtro para excluir logs de los módulos 'chip' y 'chip.clusters'."""
    def filter(self, record):
        return not record.name.startswith('chip')

# Obtener el logger raíz
root_logger = logging.getLogger()

# Aplicar el filtro a todos los manejadores del logger raíz
for handler in root_logger.handlers:
    handler.addFilter(ExcludeChipFilter())

# Definición de credenciales Wi-Fi y configuración del dispositivo
WIFI_SSID = 'MOVISTAR-WIFI6-****'
WIFI_PASSWORD = '********************'

SETUP_PIN_CODE = 20202021
NODE_ID = 1234
DISCRIMINATOR = 3840

def initialize_database():
    """Inicializa la base de datos y crea la tabla measurements si no existe."""
    with sqlite3.connect('sensor_data.db') as connection:
        cursor = connection.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS measurements (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT,
                voltage REAL,
                active_current REAL,
                active_power REAL,
                current_level INTEGER,
                updated INTEGER,
                change_level INTEGER DEFAULT NULL
            )
        ''')
        connection.commit()

async def read_attributes():
    """Función para leer atributos del dispositivo cada 30 segundos."""
    while True:
        # Lectura de atributos
        response = await devCtrl.ReadAttribute(
            NODE_ID, [
                (1, clusters.ElectricalPowerMeasurement.Attributes.Voltage),
                (1, clusters.ElectricalPowerMeasurement.Attributes.ActiveCurrent),
                (1, clusters.ElectricalPowerMeasurement.Attributes.ActivePower),
                (1, clusters.EnergyPreference.Attributes.CurrentEnergyBalance)
            ]
        )

        # Extracción de valores
        voltage = response[1][clusters.ElectricalPowerMeasurement][clusters.ElectricalPowerMeasurement.Attributes.Voltage]
        active_current = response[1][clusters.ElectricalPowerMeasurement][clusters.ElectricalPowerMeasurement.Attributes.ActiveCurrent]
        active_power = response[1][clusters.ElectricalPowerMeasurement][clusters.ElectricalPowerMeasurement.Attributes.ActivePower]
        current_energy_balance = response[1][clusters.EnergyPreference][clusters.EnergyPreference.Attributes.CurrentEnergyBalance]

        # Muestra de los valores en formato decimal
        logging.info(f"{BLUE}{BOLD}------ VALORES MEDIDOS ------{RESET}")
        if voltage is not None:
            logging.info(f"Voltaje: {voltage / 1000:.3f} V")
        else:
            logging.info("Voltaje: No disponible")

        if active_current is not None:
            logging.info(f"Corriente Activa: {active_current / 1000:.3f} A")
        else:
            logging.info("Corriente Activa: No disponible")

        if active_power is not None:
            logging.info(f"Potencia Activa: {active_power / 1000:.3f} W")
        else:
            logging.info("Potencia Activa: No disponible")

        if current_energy_balance is not None:
            logging.info(f"Nivel Actual: {current_energy_balance}")
        else:
            logging.info("Nivel Actual: No disponible")
        logging.info(f"{BLUE}{BOLD}------------------------------{RESET}")

        # Obtención de la hora local ajustada al horario de España
        timezone = pytz.timezone('Europe/Madrid')
        timestamp = datetime.now(timezone).strftime('%Y-%m-%d %H:%M:%S')

        # Conexión a la base de datos para guardar los datos
        try:
            with sqlite3.connect('sensor_data.db') as connection:
                cursor = connection.cursor()

                if current_energy_balance is not None:
                    current_level = current_energy_balance  
                else:
                    current_level = 0  # Valor por defecto    

                # Almacenar los datos en la base de datos
                cursor.execute(
                    '''
                    INSERT INTO measurements (
                        timestamp, voltage, active_current, active_power, current_level, updated
                    ) VALUES (?, ?, ?, ?, ?, 0)
                    ''',
                    (timestamp, voltage / 1000, active_current / 1000, active_power / 1000, current_level)
                )
                connection.commit()
        except sqlite3.Error as db_error:
            logging.error(f"Error al interactuar con la base de datos: {db_error}")

        # Esperar 30 segundos antes de realizar la siguiente lectura
        await asyncio.sleep(30)

async def control_mode():
    """Función para verificar y actualizar el modo del dispositivo."""
    while True:
        # Conectar a la base de datos para leer y actualizar el nivel
        try:
            with sqlite3.connect('sensor_data.db') as connection:
                cursor = connection.cursor()

                # Verificar si hay un nuevo nivel para actualizar
                cursor.execute("SELECT id, current_level, change_level FROM measurements WHERE change_level IS NOT NULL AND updated = 1  ORDER BY id DESC LIMIT 1")
                change_pending = cursor.fetchone()

                if change_pending:
                    measurement_id, current_level, change_level = change_pending
                    logging.info(f"{ORANGE}{BOLD}Cambio de nivel detectado: de {current_level} a {change_level}{RESET}")

                    attribute_instance = clusters.EnergyPreference.Attributes.CurrentEnergyBalance(value=change_level)

                    try:
                        # Actualizar el valor usando el método WriteAttribute
                        await devCtrl.WriteAttribute(
                            nodeid=NODE_ID,
                            attributes=[
                                (1, attribute_instance)
                            ],
                            interactionTimeoutMs=10000
                        )
                        cursor.execute("UPDATE measurements SET updated = 0 WHERE id = ?", (measurement_id,))
                        connection.commit()

                        logging.info(f"{ORANGE}Nivel actualizado en el dispositivo y marcado como procesado.{RESET}")
                    except Exception as write_error:
                        logging.error(f"Error al escribir el nivel en el dispositivo: {write_error}")    
        except sqlite3.Error as db_error:
            logging.error(f"Error al interactuar con la base de datos: {db_error}")

        # Añadir un pequeño retraso para evitar sobrecarga
        await asyncio.sleep(0.1)

async def main():
    """Función principal para el control del dispositivo."""
    # Manejo de argumentos de línea de comandos
    parser = argparse.ArgumentParser(description="Control del dispositivo")
    parser.add_argument("-c", "--commissioning", action="store_true", help="Realizar el commissioning del dispositivo")
    args = parser.parse_args()

    # Inicializar la base de datos
    initialize_database()

    if args.commissioning:
        logging.info("Realizando el commissioning del dispositivo...")

        # Configuración de la red Wi-Fi
        devCtrl.SetWiFiCredentials(WIFI_SSID, WIFI_PASSWORD)

        # Commissioning del dispositivo utilizando BLE
        await devCtrl.ConnectBLE(discriminator=DISCRIMINATOR, setupPinCode=SETUP_PIN_CODE, nodeid=NODE_ID)
    else:
        logging.info("Conectando al dispositivo ya comisionado...")

        try:
            # Intentar reconectar al dispositivo ya comisionado
            await devCtrl.GetConnectedDevice(nodeid=NODE_ID, allowPASE=True, timeoutMs=20000)
            logging.info("Dispositivo reconectado exitosamente.")
        except Exception as e:
            logging.error(f"Error al reconectar el dispositivo: {e}")

            # Intentar restablecer la sesión PASE
            try:
                await devCtrl.FindOrEstablishPASESession(setupCode=str(SETUP_PIN_CODE), nodeid=NODE_ID)
                logging.info("Sesión PASE restablecida exitosamente.")
            except Exception as pase_error:
                logging.error(f"Error al restablecer la sesión PASE: {pase_error}")

                # Como último recurso, resolver el nodo para obtener una nueva dirección
                try:
                    await devCtrl.ResolveNode(nodeid=NODE_ID)
                    logging.info("Nodo resuelto correctamente.")
                except Exception as resolve_error:
                    logging.error(f"Error al resolver el nodo: {resolve_error}")
                    return

    # Tareas en paralelo
    await asyncio.gather(
        read_attributes(),
        control_mode()
    )

if __name__ == "__main__":
    # Ejecutar la función principal
    asyncio.run(main())
