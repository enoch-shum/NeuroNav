# NeuroNav: Spatial Telemetry & Haptic Matrix

NeuroNav is a 36-hour hackathon project designed to provide intuitive spatial awareness through haptic feedback while broadcasting live posture and proximity telemetry to a remote caregiver dashboard.

## 🛠 Architecture
- **Hardware:** ESP32 Dev Module, VL53L0X Time-of-Flight sensor, MPU-6050 6-axis IMU, 12x vibration motors driven by 2N2222 NPN transistors.
- **Backend:** C++ WebSockets via PlatformIO.
- **Frontend:** Next.js (React), Node.js, Tailwind CSS.

## ⚡ Haptic Matrix Pinout

| Motor ID | ESP32 GPIO | Transistor Base | Note |
|----------|------------|-----------------|------|
| Motor 1  | GPIO 2     | 2N2222 Base     | Safe for output |
| Motor 2  | GPIO 4     | 2N2222 Base     | Safe for output |
| Motor 3  | GPIO 5     | 2N2222 Base     | Safe for output |
| Motor 4  | GPIO 12    | 2N2222 Base     | JTAG / Safe |
| Motor 5  | GPIO 13    | 2N2222 Base     | JTAG / Safe |
| Motor 6  | GPIO 14    | 2N2222 Base     | JTAG / Safe |
| Motor 7  | GPIO 15    | 2N2222 Base     | JTAG / Safe |
| Motor 8  | GPIO 18    | 2N2222 Base     | SPI / Safe |
| Motor 9  | GPIO 19    | 2N2222 Base     | SPI / Safe |
| Motor 10 | GPIO 25    | 2N2222 Base     | DAC / Safe |
| Motor 11 | GPIO 26    | 2N2222 Base     | DAC / Safe |
| Motor 12 | GPIO 27    | 2N2222 Base     | ADC / Safe |

*Note: I2C bus occupies GPIO 21 (SDA) and GPIO 22 (SCL). System powered by a 2.1A+ USB power bank via VIN.*

## 🚀 Quick Start Guide

### 1. Caregiver Dashboard (Frontend)
Ensure you have Node.js installed.
```bash
cd dashboard
npm install
npm run dev