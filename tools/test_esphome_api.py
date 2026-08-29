import asyncio
import sys
from aioesphomeapi.client import APIClient

HOST = sys.argv[1] if len(sys.argv) > 1 else "10.10.33.15"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 6053

async def main():
    print(f"Connecting to live ESPHome server at {HOST}:{PORT}...")
    client = APIClient(HOST, PORT, password="", noise_psk=None)
    await client.connect(login=True)
    print("Connected and logged in successfully!")

    device_info = await client.device_info()
    print("\n--- Live Device Info ---")
    print("Name:", device_info.name)
    print("Model:", device_info.model)
    print("Manufacturer:", device_info.manufacturer)
    print("MAC:", device_info.mac_address)
    print("ESPHome Version:", device_info.esphome_version)
    print("Project Name:", getattr(device_info, 'project_name', None))
    print("Project Version:", getattr(device_info, 'project_version', None))

    entities, services = await client.list_entities_services()
    print(f"\n--- Discovered {len(entities)} Entities ---")
    for ent in entities:
        print(f"[{type(ent).__name__}] key={ent.key}, name='{ent.name}', object_id='{ent.object_id}'")

    # Subscribe to states
    print("\nSubscribing to states for 3 seconds...")
    def state_callback(state):
        print(f"[State Update] {state}")

    client.subscribe_states(state_callback)
    await asyncio.sleep(3)

    print("\nDisconnecting cleanly...")
    await client.disconnect()
    print("ALL LIVE TESTS COMPLETED SUCCESSFULLY!")

if __name__ == "__main__":
    asyncio.run(main())
