from flask import Flask, render_template, request, redirect
import sqlite3
from datetime import datetime

app = Flask(__name__)

def get_data():
    try:
        conn = sqlite3.connect('sensor_data.db')
        c = conn.cursor()
        c.execute("SELECT * FROM measurements ORDER BY timestamp DESC LIMIT 10")  # Muestra de los últimos 10 registros
        data = c.fetchall()
        conn.close()

        if not data:  # Si no hay datos, devuelve una lista vacía
            return []

        # Formato de datos según la estructura de la tabla
        formatted_data = []
        for row in data:
            formatted_row = (
                row[1],  # timestamp: Hora de la medición
                row[2],  # voltage: Voltaje
                row[3],  # active_current: Corriente activa
                row[4],  # active_power: Potencia activa
                row[5],  # current_level: Nivel actual
                row[6],  # updated: Indicador si fue actualizado
                row[7]   # change_level: Cambio de nivel solicitado
            )
            formatted_data.append(formatted_row)

        return formatted_data
    except sqlite3.Error as e:
        print(f"Error al conectar a la base de datos: {e}")
        return []


@app.route('/')
def index():
    data = get_data()

    if not data:
        data = [[None, 0, 0, 0, 0, 0, 0]]  # Valor predeterminado para evitar errores en el template

    return render_template('index.html', data=data)

@app.route('/update_level', methods=['POST'])
def update_level():
    new_level = int(request.form['current_level'])

    # Conexión a la base de datos
    conn = sqlite3.connect('sensor_data.db')
    c = conn.cursor()

    # Actualizar el nuevo nivel en la tabla measurements, estableciendo 'updated' a 1
    c.execute("UPDATE measurements SET change_level = ?, updated = 1 WHERE id = (SELECT MAX(id) FROM measurements)", (new_level,))
    conn.commit()

    conn.close()

    return redirect('/')  

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
