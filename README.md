# Marble Metrics

Marble Metrics is an autonomous computer vision system designed to track and count people using a Raspberry Pi and camera module. The system captures video footage, applies real-time person detection and tracking using the **Ultralytics YOLOv5n** model, and uploads the resulting analytics to a **MongoDB cluster** for long-term storage and analysis.

The project is designed to be fully hands-off: once powered on, Marble Metrics runs automatically without requiring any user interaction.
<img width="1737" height="1193" alt="image" src="https://github.com/user-attachments/assets/2e48eb6c-bfeb-40d5-b7de-087f5b9d8c54" />

---

## Project Overview

Marble Metrics operates as an embedded system with the following core goals:

- Capture video data using a Raspberry Pi camera
- Detect and track people in the frame using a lightweight neural network
- Aggregate person-counting data over time
- Upload collected data to a centralized database on a daily schedule
- Run continuously and autonomously after system startup

---

## System Architecture

### Hardware
- Raspberry Pi
- Camera module

### Software & Tools
- Ultralytics **YOLOv5n** (lightweight person detection model)
- MongoDB (cloud-hosted cluster)
- Raspberry Pi OS (Linux)

### Data Flow
1. Camera captures continuous footage
2. YOLOv5n processes frames to identify and track people
3. Person-counting data is aggregated locally on the Raspberry Pi
4. At **midnight each night**, collected data is automatically uploaded to MongoDB
5. Data is stored for later analysis and visualization

---

## Key Features

- **Fully Autonomous Operation**  
  Marble Metrics launches automatically on Raspberry Pi startup and requires no user input.

- **Lightweight & Efficient**  
  Uses YOLOv5n, optimized for low-power hardware like the Raspberry Pi.

- **Daily Data Synchronization**  
  Collected data is securely transmitted to a MongoDB cluster every night at midnight.

- **Scalable Data Storage**  
  Cloud-hosted MongoDB allows for long-term storage and future analytics expansion.

- **Designed for Real-World Deployment**  
  Suitable for storefronts, hallways, or other environments where people flow needs to be measured.

---

## Web Dashboard

Marble Metrics includes a companion **web-based dashboard** used to visualize and analyze stored metrics from the MongoDB database.

Dashboard repository:  
👉 https://github.com/asanders005/MarbleWebMetrics

The dashboard provides:
- Historical foot-traffic visualization
- Centralized data access
- A clean interface for reviewing collected metrics

---

## Setup & Deployment

Marble Metrics is configured to run automatically on boot using system services. Once deployed:

1. Power on the Raspberry Pi  
2. The application starts automatically  
3. Video processing and data collection begin immediately  
4. No additional user interaction is required

> **Note:** MongoDB credentials and network access must be configured prior to deployment.

---

## Use Cases

- Foot traffic analysis
- Storefront analytics
- Campus or building occupancy tracking
- Research and academic data collection

---

## Future Improvements

- Live statistics streaming to the dashboard
- Improved multi-camera support
- Advanced analytics and heatmaps
- Hardware acceleration for inference
- Extended model evaluation and tuning

---

## Author

**Marble Metrics**  
Capstone Project  
Developed by **Aiden Sanders**
