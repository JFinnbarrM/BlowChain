
# Imports
import asyncio
from bleak import BleakScanner, BleakClient
import datetime
import time
import requests

# Global variable storing the current data.
current_data = {
    "time": "",
    "raw": "",
    "position": (0, 0),
    "velocity": 0,
    "distance": 0,
    "rssi_count": 0,
    "rssis": {},
}

# Helper to print the current data for viewing.
def print_data():
    print(f"Time: {current_data['time']}")
    print(f"Raw Data: {current_data['raw']}")
    print(f"Position: {current_data['position']} cm")
    print(f"Velocity: {current_data['velocity']} cm/s")
    print(f"Distance: {current_data['distance']} cm")
    print(f"RSSI Count: {current_data['rssi_count']}")
    for name, rssi in current_data["rssis"].items():
        print(f"  {name}: {rssi} dBm")
    print()

##########################################################################
### BASE OBSERVER ########################################################

"""https://bleak.readthedocs.io/en/latest/api/scanner.html?highlight=bleakscanner#bleak.BleakScanner"""
# Bleak API documentation for BleakScanner.

TARGET_BASE_MAC = "dc:78:58:8d:01:13"

# Decodes the raw data from the base.
def decode_data(raw_data):
    try:
        byte_data = bytearray(raw_data)
        ptr = 0 

        # First byte: X position (tenths of a meter).
        pos_x = int.from_bytes(byte_data[ptr:ptr+1], 'little') 
        ptr += 1 
        # Second byte: Y position (tenths of a meter).
        pos_y = int.from_bytes(byte_data[ptr:ptr+1], 'little') 
        ptr += 1 
        # Next byte: velocity (cm/s) (little endian).
        # Was reduced from 2 to make room for ultrasonic node RSSI but then we connected that straight to the base.
        velocity = int.from_bytes(byte_data[ptr:ptr+1], 'little') 
        ptr += 1
        # Next 2 bytes: distance (cm) (little endian).
        distance = int.from_bytes(byte_data[ptr:ptr+2], 'little') 
        ptr += 2 

        # Next byte: RSSI count.
        if len(byte_data) > ptr:
            rssi_count = byte_data[ptr]
            ptr += 1
            
            # Next rssi_count*2 bytes: RSSI measurements (pairs of: char, int8_t).
            rssis = {}
            for i in range(rssi_count):
                # Add value to dictionary by character identifier.
                rssis[chr(byte_data[ptr])] = int.from_bytes([byte_data[ptr+1]], byteorder='little', signed=True)
                ptr += 2
        
        # Return the time, a raw hex dump, and all the recieved data.
        return {
            "time": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "raw": raw_data.hex(),
            "position": (pos_x, pos_y),
            "velocity": velocity,
            "distance": distance,
            "rssi_count": rssi_count,
            "rssis": rssis
        }
    
    except Exception as e: # Handles errors and allows raw data examination.
        print(f"  Error parsing data: {e}")
        print(f"  Raw Data: {byte_data.hex()}")
        return None
    
# The callback function for when a bluetooth broadcast is detected.
def detection_callback(device, advertisement_data):

    # If the device is not the base, ignore it.
    mac = device.address.lower()
    if mac != TARGET_BASE_MAC:
        return
    print(f"Detected base broadcast ({device.address}) with RSSI {device.rssi} dBm")

    # If there is no manufacturer data, ignore it.
    mfg_data = advertisement_data.manufacturer_data
    if not mfg_data:
        print(f"...but no data was found.")
        return

    # Get the raw data from the manufacturer data and decode it.
    company_id, raw_data = next(iter(mfg_data.items()))
    decoded_data = decode_data(raw_data)

    # If decoding the data fails, ignore it.
    if decoded_data is None:
        return
    
    # Update the current data with the decoded data.
    current_data.update(decoded_data)
    print_data() 

# Starts the observer which continuously scans for bluetooth adverisements.
async def base_observer():
    # Initialises the scanner and sets its interval/window.
    scanner = BleakScanner(
        detection_callback  = detection_callback, # Sets the callback function for when a bluetooth advertisement is detected.
        interval=100,    # Scan interval in ms.
        window=10        # Scan window in ms.
    )    
    print(f"Scanning for base: {TARGET_BASE_MAC}\n")
    await scanner.start()

    # Keeps the scanning thread alive.
    while True:
        await asyncio.sleep(5)
        # Print a dot every 5 seconds to show that the script is running.
        print(".", end="") 

##########################################################################
### VIEWER CONNECTOR #####################################################

"""https://bleak.readthedocs.io/en/latest/api/client.html?highlight=bleakclient#bleak.BleakClient"""
# Bleak API documentation for BleakClient.

"""https://devzone.nordicsemi.com/guides/short-range-guides/b/bluetooth-low-energy/posts/ble-services-a-beginners-tutorial"""
# How UUID's work.

TARGET_VIEWER_MAC = "dc:78:58:8d:01:42"
TARGET_VIEWER_NAME = "viewer"

# Service and characteristic UUIDs (matching the ones setup in viewer GATT service zephyr code).
SERVICE_UUID = "00000000-0000-0000-0000-000000000000"
X_POS_UUID   = "00000000-0000-0000-0000-0000000000aa"
Y_POS_UUID   = "00000000-0000-0000-0000-0000000000bb"

# Continuously connects to the viewer and sends position values.
async def viewer_connector():
    while True: # Keep trying to connect to the viewer until it succeeds.
        try:    # If the connection fails or times out, catch the error and retry.

            # Discover all bluetooth devices advertising.
            devices = await BleakScanner.discover()
            # Get the first device with the target name.
            target = next((d for d in devices if d.name == TARGET_VIEWER_NAME), None)
            # If none are found, retry.
            if target == None:
                print("No viewer devices found. Retrying...")
                continue

            # Connect to the viewer as a client.
            async with BleakClient(target) as client:
            # async with BleakClient(TARGET_VIEWER_MAC) as client: # MAC address on viewer could not be set.
                print(f"Connected to viewer: {TARGET_VIEWER_MAC}")
                
                # Print the viewer peripherals avaliable services (for debugging).
                print("\nAvailable services and characteristics:")
                for service in client.services:
                    print(f"Service: {service.uuid}")
                    for char in service.characteristics:
                        print(f"  Characteristic: {char.uuid} (Handle: {char.handle})")
                print()
                
                # Continuously send position values.
                while True:
                    x, y = current_data["position"]
                    await client.write_gatt_char(X_POS_UUID, bytes([x]), response=True)
                    await client.write_gatt_char(Y_POS_UUID, bytes([y]), response=True)
                    
                    print(f"Position sent to viewer: (X: {x}, Y: {y})\n")
                    await asyncio.sleep(1)

        except asyncio.TimeoutError: # Prevents timeout errors.
            print("Connecting to viewer ({TARGET_VIEWER_MAC}) timed out. Retrying...")
        except Exception as e: # Prevents other errors.
            print(f"viewer ({TARGET_VIEWER_MAC}) connection error: {e}")

##########################################################################
### TAGO SENDER ##########################################################

"""https://admin.tago.io/dashboards/info/68187982ed2478000aa2d67a?tab=0&edit=yes"""
# TagoIO dashboard link.

"""https://help.tago.io/portal/en/kb/articles/34-sending-data"""
"""https://api.docs.tago.io/#41e953a9-8b0c-4166-aa0f-1db3596b02f7"""
# How to send data to TagoIO.

"""https://www.w3schools.com/python/ref_requests_post.asp"""
# How to send a post request using requests.

TAGO_DEVICE_TOKEN = "69c5220c-6201-4de4-9483-a0871d23aa3e"

# Sends data to TagoIO using its API.
def send_to_tago(data):
    url = "https://api.tago.io/data"
    headers = { "Device-Token": TAGO_DEVICE_TOKEN }

    # Make a TagoIO payload (JSON format).
    payload = [
        {"variable": "time", "value": data["time"]},
        {"variable": "raw", "value": data["raw"]},
        {"variable": "position_x", "value": data["position"][0]},
        {"variable": "position_y", "value": data["position"][1]},
        {"variable": "velocity", "value": data["velocity"]},
        {"variable": "distance", "value": data["distance"]},
        {"variable": "rssi_count", "value": data["rssi_count"]},
    ]
    for name, value in data["rssis"].items():
        payload.append({
            "variable": name,
            "value": value,
        })

    # Send the payload.
    # print(f"\nSending data to TagoIO: {payload}")
    response = requests.post(url, headers=headers, json=payload)
    print("\n##############################################################################")
    print(f"# Sent Status: {response.status_code} | Sent Response: {response.text} #")
    print("##############################################################################\n")

# COntinuously sends data to TagoIO.
async def tago_sender():
    while True:
        send_to_tago(current_data)
        await asyncio.sleep(2)

##########################################################################

"""https://docs.python.org/3/library/asyncio.html"""
# The API for asyncio.

# Creates each thread as a task to run them sumultaneously.
async def main():
    # Create tasks for all required functions.
    task_base_observer = asyncio.create_task(base_observer())
    task_screen_connector = asyncio.create_task(viewer_connector())
    task_tago_sender = asyncio.create_task(tago_sender())
    
    # Wait for both tasks (will run concurrently)
    await asyncio.gather(task_base_observer, 
                         task_screen_connector, 
                         task_tago_sender)

# When the script is run, start the main function.
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"Error: {e}")