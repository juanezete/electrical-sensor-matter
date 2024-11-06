#!/bin/bash
export TERM=xterm

echo "Configurando directorios..."
mkdir -p /run/dbus /run/lock /run/avahi-daemon
rm -f /run/dbus/pid /run/dbus/system_bus_socket

echo "Iniciando servicios y chip-repl..."

# Inicio D-Bus y Avahi
echo "Iniciando D-Bus..."
dbus-daemon --system --fork

echo "Iniciando Avahi..."
avahi-daemon --daemonize --no-chroot

echo "Iniciando Bluetooth..."
bluetoothd &

sleep 2

# Verficicacion bluetooth
echo "Verificando adaptador Bluetooth..."
hciconfig hci0 up

# Activacion del entorno virtual que contiene chip-repl
echo "Activando el entorno virtual de Python..."
source /home/juan/matter/connectedhomeip/separate/bin/activate

pip install ipython pytz 

# Ejecucion de chip-repl
echo "Ejecutando chip-repl"
    
    if [ "$MODE" = "1" ]; then
    echo "Realizando commissioning..."
    chip-repl << EOF
    %run control.py -c
EOF
    else
    echo "Conectando a dispositivo"
    chip-repl << EOF
    %run control.py
EOF
    fi

# Mantener el contenedor vivo
tail -f /dev/null
