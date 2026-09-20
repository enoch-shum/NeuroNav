# NeuroNav 🧭🧠
> **Real-Time Haptic Navigation and Posture Monitor**  
> *Built for VTHacks 14 (36-hour hackathon window)*

NeuroNav acts as a digital sixth sense, converting real-time IMU tilt tracking and laser distance measurements into a tactile haptic response matrix. It allows users to read a room or monitor posture without needing to glance at their surroundings.

---

## ⚡ Key Features

* **1-12 Haptic Motor Matrix:** Delivers directional spatial cues straight to the user.
* **40Hz Sensor Poll Rate:** Ultra-low latency data collection for real-time feedback.
* **Proximity Zones:** 3 distinct distance tiers mapped via Time-of-Flight sensors.
* **Live Caretaker Dashboard:** Built with Next.js and WebSockets to stream posture orientation and spatial metrics remotely.
* **Immersive 3D Landing Page:** Interactive Three.js brain visualization demonstrating the haptic network.

---

## 🛠️ Tech Stack & Hardware

### Hardware Components
* **Microcontroller:** ESP32
* **IMU:** MPU-6050 (Accelerometer & Gyroscope for tilt/posture tracking)
* **Distance Sensor:** VL53L0X Time-of-Flight (ToF) Laser Sensor
* **Feedback:** 12x Haptic Vibration Motors

### Software & Frontend
* **Embedded/Firmware:** C++ (Arduino IDE / PlatformIO)
* **Communication:** WebSockets (Real-time telemetry)
* **Dashboard / Web App:** Next.js, Tailwind CSS, Three.js
* **Styling:** Fraunces & Inter typography

---

## 🚀 Getting Started

### 1. Clone the Repository
```bash
git clone [https://github.com/your-username/neuronav.git](https://github.com/your-username/neuronav.git)
cd neuronav
