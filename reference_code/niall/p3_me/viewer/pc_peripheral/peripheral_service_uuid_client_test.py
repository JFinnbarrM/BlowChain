import asyncio
from bleak import BleakClient

# Device MAC address (must match your Zephyr device)
DEVICE_ADDRESS = "dc:78:58:8d:01:42"

# UUIDs (must match exactly what's in your Zephyr code)
SERVICE_UUID = "00000000-0000-0000-0000-000000000000"
X_POS_UUID   = "00000000-0000-0000-0000-0000000000aa"
Y_POS_UUID   = "00000000-0000-0000-0000-0000000000bb"

def print_avaliable_services(services: BleakClient.services):
    print("\nAvailable services and characteristics:")
    for service in services:
        print(f"Service: {service.uuid}")
        for char in service.characteristics:
            print(f"  Characteristic: {char.uuid} (Handle: {char.handle})")

async def write_positions():
    async with BleakClient(DEVICE_ADDRESS) as client:
        print(f"Connected to {DEVICE_ADDRESS}")
        
        # First let's discover services (for debugging)
        print_avaliable_services(client.services)
        
        # Continuously send test values
        x, y = 0, 0
        while True:
            # Write X position (as 1-byte unsigned integer)
            await client.write_gatt_char(X_POS_UUID, bytes([x]), response=False)
            # Write Y position
            await client.write_gatt_char(Y_POS_UUID, bytes([y]), response=False)
            
            print(f"Sent position: ({x}, {y})")
            
            # Increment values (wrapping at 255)
            x = (x + 10) % 256
            y = (y + 5) % 256
            
            await asyncio.sleep(1.0)  # Send every second

            xrb = await client.read_gatt_char(X_POS_UUID)
            yrb = await client.read_gatt_char(Y_POS_UUID)

            print("READ X:", int.from_bytes(xrb, byteorder="little"))
            print("READ Y:", int.from_bytes(yrb, byteorder="little"))

if __name__ == "__main__":
    try:
        asyncio.run(write_positions())
    except Exception as e:
        print(f"Error: {e}")