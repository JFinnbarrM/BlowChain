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
        self.client = None
        self.device = None
        self.connected = False
        
    async def scan_for_lockbox(self, timeout=10.0):
        """Scan for SecureLockbox device"""
        logger.info("Scanning for SecureLockbox...")
        
        devices = await BleakScanner.discover(timeout=timeout)
        
        for device in devices:
            if device.name and "SecureLockbox" in device.name:
                logger.info(f"Found SecureLockbox: {device.name} ({device.address})")
                self.device = device
                return device
                
        logger.error("SecureLockbox not found")
        return None
    
    async def connect(self):
        """Connect to the lockbox"""
        if not self.device:
            logger.error("No device found. Run scan_for_lockbox() first")
            return False
            
        try:
            logger.info(f"Connecting to {self.device.address}...")
            self.client = BleakClient(self.device.address)
            await self.client.connect()
            self.connected = True
            logger.info("Connected successfully!")
            
            # List all services and characteristics
            await self.discover_services()
            
            return True
            
        except Exception as e:
            logger.error(f"Connection failed: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from the lockbox"""
        if self.client and self.connected:
            await self.client.disconnect()
            self.connected = False
            logger.info("Disconnected")
    
    async def discover_services(self):
        """Discover and print all services and characteristics"""
        if not self.client or not self.connected:
            logger.error("Not connected")
            return
            
        logger.info("Discovering services...")
        
        for service in self.client.services:
            logger.info(f"Service: {service.uuid}")
            for char in service.characteristics:
                logger.info(f"  Characteristic: {char.uuid} - {char.properties}")
    
    async def read_username(self):
        """Read the current username from lockbox"""
        if not self.client or not self.connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading username...")
            data = await self.client.read_gatt_char(USERNAME_CHAR_UUID)
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
        if not self.client or not self.connected:
            logger.error("Not connected")
            return False
            
        try:
            logger.info(f"Setting username to: {username}")
            await self.client.write_gatt_char(USERNAME_CHAR_UUID, username.encode('utf-8'))
            logger.info("Username sent successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to set username: {e}")
            return False
    
    async def read_lock_status(self):
        """Read the current lock status"""
        if not self.client or not self.connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading lock status...")
            data = await self.client.read_gatt_char(LOCK_STATUS_CHAR_UUID)
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
        if not self.client or not self.connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading user status...")
            data = await self.client.read_gatt_char(USER_STATUS_CHAR_UUID)
            if data and len(data) >= 4:
                state, failed_attempts, system_locked, tamper_detected = data[:4]
                
                state_names = {
                    0: "READY",
                    1: "PRESENCE_DETECTED", 
                    2: "WAITING_PASSCODE",
                    3: "LOCKED",
                    4: "SHUTDOWN"
                }
                
                status = {
                    'state': state_names.get(state, f"UNKNOWN({state})"),
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
        if not self.client or not self.connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading passcode...")
            data = await self.client.read_gatt_char(PASSCODE_CHAR_UUID)
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
        if not self.client or not self.connected:
            logger.error("Not connected")
            return False
            
        if len(passcode) != 6:
            logger.error("Passcode must be exactly 6 digits")
            return False
            
        try:
            logger.info(f"Entering passcode: {passcode}")
            await self.client.write_gatt_char(PASSCODE_CHAR_UUID, passcode.encode('utf-8'))
            logger.info("Passcode sent successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to send passcode: {e}")
            return False
    
    async def read_voc_data(self):
        """Read VOC sensor data"""
        if not self.client or not self.connected:
            logger.error("Not connected")
            return None
            
        try:
            logger.info("Reading VOC data...")
            data = await self.client.read_gatt_char(VOC_SENSOR_CHAR_UUID)
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

    async def run(self):
        """Main function demonstrating usage"""
        lockbox = LockboxClient()
        
        try:
            # Scan for lockbox
            device = await lockbox.scan_for_lockbox()
            if not device:
                print("No lockbox found!")
                return
            
            # Connect
            if not await lockbox.connect():
                print("Failed to connect!")
                return
            
            # Identify as PC client by setting username
            await lockbox.set_username("PC_CLIENT")
            
            # Wait a moment for the lockbox to process
            await asyncio.sleep(1)
            
            # Read all status information
            await lockbox.read_all_status()
            
            # Interactive loop
            print("\n=== Interactive Mode ===")
            print("Commands:")
            print("  'username <name>' - Set username")
            print("  'passcode <code>' - Enter passcode")
            print("  'status' - Read all status")
            print("  'quit' - Exit")
            
            while True:
                try:
                    cmd = input("\nEnter command: ").strip().split()
                    if not cmd:
                        continue
                        
                    if cmd[0] == 'quit':
                        break
                    elif cmd[0] == 'username' and len(cmd) > 1:
                        await lockbox.set_username(cmd[1])
                        await asyncio.sleep(0.5)
                        await lockbox.read_passcode()  # Read the generated passcode
                    elif cmd[0] == 'passcode' and len(cmd) > 1:
                        await lockbox.enter_passcode(cmd[1])
                        await asyncio.sleep(0.5)
                        await lockbox.read_lock_status()  # Check if lock opened
                    elif cmd[0] == 'status':
                        await lockbox.read_all_status()
                    else:
                        print("Invalid command")
                        
                except KeyboardInterrupt:
                    break
        
        finally:
            await lockbox.disconnect()

if __name__ == "__main__":    
    lockbox_client = LockboxClient()
    try:
        asyncio.run(lockbox_client.run())
    except Exception as e:
        print(f"Error: {e}")