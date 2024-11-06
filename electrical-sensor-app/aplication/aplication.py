import asyncio
import json
from matter_server.client import MatterClient

async def main():
    client = MatterClient("ws://localhost:1234")

    await client.connect()

    # Comisionar un nuevo dispositivo
    await client.send_command({
        "message_id": "1",
        "command": "commission_with_code",
        "args": {
            "code": "MT:Y.ABCDEFG123456789"
        }
    })

    # Obtener nodos existentes
    response = await client.send_command({
        "message_id": "2",
        "command": "get_nodes"
    })
    print("Nodes:", response)

    # Leer atributos de un nodo
    node_id = response['nodes'][0]['node_id']
    node_info = await client.send_command({
        "message_id": "3",
        "command": "get_node",
        "args": {
            "node_id": node_id
        }
    })
    print("Node Info:", node_info)

    await client.disconnect()

asyncio.run(main())
