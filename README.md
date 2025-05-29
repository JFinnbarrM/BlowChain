# BlowChain: LockBox

Authors: James McAuley, Niall Waller, Dhanan Balasaravanan

Oizys - Gold

Topic: Tamper-Proof Secure-Logging Lock-Box

## Overview

Blowchain proposes an advanced digital lockbox, combining environmental sensing, offline architecture, and blockchain based accountability to remove user error from lockbox security

### System Architecture: 
- Mobile Node: Nordic Thingy 52 - The Volatile Organic Compound sensor node located inside the box, this monitors environmental parameters time and off-gassing of volatile organic compounds. 

- Mobile Node: disco_l475_iot1 - The tampering node is located inside the box with a seperate battery, this monitors environmental parameters such as magnometer and accelerometer. 

- Base Node: Receives BLE packets and uploads the data to a secure offline blockchain, checks users, inputted passwords, sensor thresholds and sends it by GATT to the PC.

- M5Core2 Display Unit: Displays

- Desktop Dashboard: Provides a more detailed interface for visualizing historical and live data from the VOC supply. 

- Actuator: Opens and locks the box when receving signals from the base node.

Key Features:
- Real-time BLE communication
- Secure data logging using blockchain technology
- Multi-platform visualization (M5Core2, Desktop)