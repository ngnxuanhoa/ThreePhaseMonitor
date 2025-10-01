# ESP32 Three Phase Energy Monitor Library

An advanced, non-blocking, interrupt-driven Arduino library for the ESP32 to monitor three-phase AC energy. This library is designed for high-accuracy measurements using two external ADS1115 16-bit ADCs.

This library provides a robust foundation for building sophisticated energy monitoring projects, from smart home dashboards to industrial IoT applications.

## Key Features

-   **True Three-Phase Monitoring**: Simultaneously measures Voltage and Current for all three phases (L1, L2, L3).
-   **High Accuracy**: Leverages external 16-bit ADS1115 ADCs for superior precision compared to the internal ESP32 ADC.
-   **Comprehensive Calculations**: Automatically computes all essential power metrics for each phase:
    -   RMS Voltage (Vrms)
    -   RMS Current (Irms)
    -   Real Power (W)
    -   Apparent Power (VA)
    -   Reactive Power (VAR)
    -   Power Factor (PF)
    -   Cumulative Energy Consumption (kWh)
-   **Auto-Calibration**: Includes a startup calibration routine to automatically determine and compensate for DC offsets, removing errors from component tolerances.
-   **Clean API**: Designed with a simple and intuitive API for easy configuration and data retrieval.

## Hardware Requirements

| Component | Quantity | Recommended Model | Purpose |
| :--- | :--- | :--- | :--- |
| Microcontroller | 1 | Any ESP32 Dev Board | Main Processor |
| ADC Module | **2** | ADS1115 | 1 for Voltage, 1 for Current |
| Voltage Sensor | 3 | ZMPT101B / T220V1W2G2-01 | Isolated Voltage Sensing |
| Current Sensor | 3 | SCT-013-000 (Current Output) | Non-invasive Current Sensing |
| Resistors, Caps | Various | - | Signal Conditioning Circuits|

## Library Installation

1.  **Download:** Download this repository as a ZIP file from the GitHub page.
2.  **Install in Arduino IDE:**
    -   Open the Arduino IDE.
    -   Go to `Sketch` -> `Include Library` -> `Add .ZIP Library...`
    -   Select the downloaded ZIP file.
3.  **Install Dependencies:** This library requires the `Adafruit ADS1X15` library. Please install it via the Arduino Library Manager:
    -   Go to `Sketch` -> `Include Library` -> `Manage Libraries...`
    -   Search for "Adafruit ADS1X15" and click "Install".

## Hardware Wiring Diagram

Here is the essential wiring guide for connecting the ESP32 to the two ADS1115 modules.

#### 1. I2C Bus Connection (Shared)
*   `ESP32 3.3V` -> `VCC` on **both** ADS1115 modules.
*   `ESP32 GND`  -> `GND` on **both** ADS1115 modules.
*   `ESP32 SDA (GPIO21)` -> `SDA` on **both** ADS1115 modules.
*   `ESP32 SCL (GPIO22)` -> `SCL` on **both** ADS1115 modules.

#### 2. I2C Address Configuration (Crucial for differentiation)
*   **Voltage ADC (ADS_V):** Connect the `ADDR` pin to `GND`. This sets the I2C address to `0x48`.
*   **Current ADC (ADS_I):** Connect the `ADDR` pin to `VCC (3.3V)`. This sets the I2C address to `0x49`.

#### 3. Analog Signal Connections
*   Phase 1 Voltage Signal -> `A0` of Voltage ADC.
*   Phase 1 Current Signal -> `A0` of Current ADC.
*   ...and so on for Phase 2 (`A1`) and Phase 3 (`A2`).

## Quick Start Guide & API

### 1. Include and Instantiate
    ```cpp
    #include "ThreePhaseMonitor.h"
    
    // Define your hardware configuration
    #define VOLTAGE_ADC_ADDR  0x48
    #define CURRENT_ADC_ADDR  0x49
    
    // Create an instance of the library
    ThreePhaseMonitor emon;
### 2. Initialize in setup()
    code
    C++
    void setup() {
      Serial.begin(115200);
      
      // Initialize the library
      if (!emon.begin(ADS_V_ADDR, ADS_I_ADDR)) {
        Serial.println("Loi khoi tao he thong do luong! Kiem tra ket noi I2C.");
        while(1);
      }
    
      // --- Hiệu chuẩn ---
      Serial.println("Chuan bi hieu chuan. Vui long dam bao KHONG CO TAI tren ca 3 pha!");
      delay(3000); 
      emon.calibrateOffsets();
      delay(1000);
    
      // --- Cấu hình các hằng số cho từng pha ---
      emon.configurePhase(0, 20.0, 2000.0, 22.0); // Pha R (A0)
      emon.configurePhase(1, 20.0, 2000.0, 22.0); // Pha S (A1)
      emon.configurePhase(2, 20.0, 2000.0, 22.0); // Pha T (A2)
    }
### 3. Run in loop()
    The emon.loop() function must be called continuously. It handles all background sampling and calculations.
    code
        C++
        void loop() {
          // This function drives the entire measurement process.
          emon.loop();
        }
### 4. Retrieve Results
    Use the various getter functions to access the calculated data.
    code
        C++
        void printResults() {
          Serial.println("----------------------------------------");
          for (int i = 0; i < 3; i++) {
            Serial.printf("PHASE %d | V: %.2fV | I: %.3fA | P: %.1fW | PF: %.2f | E: %.4fkWh\n",
              i + 1,
              emon.getVrms(i),
              emon.getIrms(i),
              emon.getRealPower(i),
              emon.getPowerFactor(i),
              emon.getEnergyKWh(i)
            );
          }
          Serial.printf("TOTAL POWER: %.2f W\n", emon.getTotalRealPower());
        }
### Contributing
Contributions are welcome! If you have suggestions for improvements or find any bugs, please feel free to open an issue or submit a pull request.
### License
This project is licensed under the MIT License. See the LICENSE file for details.
