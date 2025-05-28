#!/usr/bin/env python3

import asyncio
import logging
from bleak import BleakScanner, BleakClient
from bleak.backends.characteristic import BleakGATTCharacteristic
import struct
import sys

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Lockbox service and characteristic UUIDs
LOCKBOX_SERVICE_UUID = "00001234-0000-1000-8000-00805f9b34fb"
USERNAME_CHAR_UUID = "0000aa01-0000-1000-8000-00805f9b34fb"
BLOCK_INFO_CHAR_UUID = "0000bb02-0000-1000-8000-00805f9b34fb"
LOCK_STATUS_CHAR_UUID = "0000cc03-0000-1000-8000-00805f9b34fb"
USER_STATUS_CHAR_UUID = "0000dd04-0000-1000-8000-00805f9b34fb"
PASSCODE_CHAR_UUID = "0000ee05-0000-1000-8000-00805f9b34fb"
VOC_SENSOR_CHAR_UUID = "0000ff06-0000-1000-8000-00805f9b34fb"

class LockboxClient:
    def __init__(self):
        self._target_device_name = "SecureLockbox"

        self._client = None
        self._device = None
        self._connected = False

        self._state_names = {
            0: "READY",
            1: "PRESENCE_DETECTED", 
            2: "WAITING_PASSCODE",
            3: "LOCKED",
            4: "SHUTDOWN"
        }

        self._uuid_names = {
            "00001234-0000-1000-8000-00805f9b34fb": "LOCKBOX_SERVICE_UUID",
            "0000aa01-0000-1000-8000-00805f9b34fb": "USERNAME_CHAR_UUID",
            "0000bb02-0000-1000-8000-00805f9b34fb": "BLOCK_INFO_CHAR_UUID",
            "0000cc03-0000-1000-8000-00805f9b34fb": "LOCK_STATUS_CHAR_UUID",
            "0000dd04-0000-1000-8000-00805f9b34fb": "USER_STATUS_CHAR_UUID",
            "0000ee05-0000-1000-8000-00805f9b34fb": "PASSCODE_CHAR_UUID",
            "0000ff06-0000-1000-8000-00805f9b34fb": "VOC_SENSOR_CHAR_UUID",
        }
        
    async def _device_scan(self):
        while True:
            logger.info("Scanning for SecureLockbox...")
            devices = await BleakScanner.discover()
            
            self._device = next((d for d in devices if d.name == self._target_device_name), None)
            if self._device:
                logger.info(f"Found SecureLockbox: {self._device.name} ({self._device.address})")
                return
            else: 
                logger.error("SecureLockbox not found")

    async def _device_connect(self):            
        try:
            logger.info(f"Connecting to {self._device.address}...")
            self._client = BleakClient(self._device.address)
            await self._client.connect()
            self._connected = True
            logger.info("Connected successfully!")
            
            # List all services and characteristics
            await self.discover_services()
            
            return True
            
        except Exception as e:
            logger.error(f"Connection failed: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from the lockbox"""
        if self._client and self._connected:
            await self._client.disconnect()
            self._connected = False
            logger.info("Disconnected")
    
    async def discover_services(self):
        """Discover and print all services and characteristics"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return
            
        logger.info("Discovering services...")
        
        for service in self._client.services:
            logger.info(f"Service: {service.uuid}")
            for char in service.characteristics:
                logger.info(f"  Characteristic: {char.uuid} - {char.properties}")
    
    async def read_username(self):
        """Read the current username from lockbox"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading username...")
            data = await self._client.read_gatt_char(USERNAME_CHAR_UUID)
            if data:
                username = data.decode('utf-8', errors='ignore').strip('\x00')
                logger.info(f"Current username: '{username}'")
                return username
            else:
                logger.warning("No username data received")
                return None
                
        except Exception as e:
            logger.error(f"Failed to read username: {e}")
            return None
    
    async def set_username(self, username):
        """Send username to lockbox (this identifies us as PC client)"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return False
            
        try:
            logger.info(f"Setting username to: {username}")
            await self._client.write_gatt_char(USERNAME_CHAR_UUID, username.encode('utf-8'))
            logger.info("Username sent successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to set username: {e}")
            return False
    
    async def read_lock_status(self):
        """Read the current lock status"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading lock status...")
            data = await self._client.read_gatt_char(LOCK_STATUS_CHAR_UUID)
            if data and len(data) >= 1:
                status = data[0]
                status_text = "OPEN" if status else "CLOSED"
                logger.info(f"Lock status: {status_text}")
                return status_text
            else:
                logger.warning("No lock status data received")
                return None
                
        except Exception as e:
            logger.error(f"Failed to read lock status: {e}")
            return None
    
    async def read_user_status(self):
        """Read the current user status"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading user status...")
            data = await self._client.read_gatt_char(USER_STATUS_CHAR_UUID)
            if data and len(data) >= 4:
                state, failed_attempts, system_locked, tamper_detected = data[:4]
                
                status = {
                    'state': self._state_names.get(state, f"UNKNOWN({state})"),
                    'failed_attempts': failed_attempts,
                    'system_locked': bool(system_locked),
                    'tamper_detected': bool(tamper_detected)
                }
                
                logger.info(f"User status: {status}")
                return status
            else:
                logger.warning("No user status data received")
                return None
                
        except Exception as e:
            logger.error(f"Failed to read user status: {e}")
            return None
    
    async def read_passcode(self):
        """Read the current passcode"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading passcode...")
            data = await self._client.read_gatt_char(PASSCODE_CHAR_UUID)
            if data:
                passcode = data.decode('utf-8', errors='ignore').strip('\x00')
                logger.info(f"Current passcode: {passcode}")
                return passcode
            else:
                logger.warning("No passcode data received")
                return None
                
        except Exception as e:
            logger.error(f"Failed to read passcode: {e}")
            return None
    
    async def enter_passcode(self, passcode):
        """Send passcode to lockbox"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return False
            
        if len(passcode) != 6:
            logger.error("Passcode must be exactly 6 digits")
            return False
            
        try:
            logger.info(f"Entering passcode: {passcode}")
            await self._client.write_gatt_char(PASSCODE_CHAR_UUID, passcode.encode('utf-8'))
            logger.info("Passcode sent successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to send passcode: {e}")
            return False
    
    async def read_voc_data(self):
        """Read VOC sensor data"""
        if not self._client or not self._connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading VOC data...")
            data = await self._client.read_gatt_char(VOC_SENSOR_CHAR_UUID)
            if data and len(data) >= 8:
                # Unpack: current_voc (uint16), threshold (uint16), timestamp (uint32)
                current_voc, threshold, timestamp = struct.unpack('<HHI', data[:8])
                
                voc_data = {
                    'current_voc': current_voc,
                    'threshold': threshold, 
                    'timestamp': timestamp
                }
                
                logger.info(f"VOC data: {current_voc} PPB (threshold: {threshold} PPB)")
                return voc_data
            else:
                logger.warning("No VOC data received")
                return None
                
        except Exception as e:
            logger.error(f"Failed to read VOC data: {e}")
            return None
    
    async def read_all_status(self):
        """Read all status information"""
        logger.info("=== Reading All Status Information ===")
        
        username = await self.read_username()
        lock_status = await self.read_lock_status()
        user_status = await self.read_user_status()
        passcode = await self.read_passcode()
        voc_data = await self.read_voc_data()
        
        print("\n=== LOCKBOX STATUS ===")
        print(f"Username: {username}")
        print(f"Lock Status: {lock_status}")
        print(f"User Status: {user_status}")
        print(f"Passcode: {passcode}")
        print(f"VOC Data: {voc_data}")
        print("=" * 25)

    async def user_loop(self):
        """Main function demonstrating usage"""        
        try:
            # Scan for lockbox
            await self._device_scan()
            if not self._device:
                print("No lockbox found!")
                return
            
            # Connect
            if not await self._device_connect():
                print("Failed to connect!")
                return
            
            # Identify as PC client by setting username
            await self.set_username("PC_CLIENT")
            
            # Wait a moment for the lockbox to process
            await asyncio.sleep(1)
            
            # Read all status information
            await self.read_all_status()
            
            print("\n=== Interactive Mode ===")
            print("Commands:")
            print("  'username <name>' - Set username")
            print("  'passcode <code>' - Enter passcode")
            print("  'status' - Read all status")
            print("  'quit' - Exit")
            
            # Interactive loop

            while True:
                try:
                    cmd = input("\nEnter command: ").strip().split()
                    if not cmd:
                        continue
                        
                    if cmd[0] == 'quit':
                        break
                    elif cmd[0] == 'username' and len(cmd) > 1:
                        await self.set_username(cmd[1])
                        await asyncio.sleep(0.5)
                        await self.read_passcode()  # Read the generated passcode
                    elif cmd[0] == 'passcode' and len(cmd) > 1:
                        await self.enter_passcode(cmd[1])
                        await asyncio.sleep(0.5)
                        await self.read_lock_status()  # Check if lock opened
                    elif cmd[0] == 'status':
                        await self.read_all_status()
                    else:
                        print("Invalid command")
                        
                except KeyboardInterrupt:
                    break
        
        finally:
            await self.disconnect()


# Creates each thread as a task to run them sumultaneously.
async def main():
    # Create tasks for all required functions.
    lockbox_client = LockboxClient()
    task_lockbox_client = asyncio.create_task(lockbox_client.user_loop())
    
    # Wait for all tasks (will run concurrently)
    await asyncio.gather(
        task_lockbox_client
    )

# When the script is run, start the main function.
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"Error: {e}")