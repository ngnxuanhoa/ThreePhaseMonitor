#ifndef ThreePhaseMonitor_h
#define ThreePhaseMonitor_h

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Một struct để chứa tất cả dữ liệu cho một pha
struct PhaseData {
    // --- Calibration Constants ---
    float vCal; // Tỷ lệ biến áp V (ví dụ: 220.0/11.0 = 20.0)
    float iCal; // Tỷ lệ biến áp I (ví dụ: 100A/50mA = 2000.0)
    float burdenResistor; // Điện trở gánh (Ohm)
    int vOffset;
    int iOffset;

    // --- Calculation Sums ---
    double sumV_sq;
    double sumI_sq;
    double sumPower;
    long sampleCount;

    // --- Final Results ---
    double Vrms;
    double Irms;
    double realPower;       // W
    double apparentPower;   // VA
    double reactivePower;   // VAR
    double powerFactor;
    double kWh;
};

class ThreePhaseMonitor {
public:
    ThreePhaseMonitor();

    bool begin(uint8_t v_addr, uint8_t i_addr, uint8_t rdyPin, TwoWire &wire = Wire);
    void configurePhase(uint8_t phase, float vCal, float iCal, float burdenResistor);
    void calibrateOffsets(uint16_t numSamples = 1024);
    void loop();

    // --- Getters for results ---
    double getVrms(uint8_t phase);
    double getIrms(uint8_t phase);
    double getRealPower(uint8_t phase);
    double getApparentPower(uint8_t phase);
    double getReactivePower(uint8_t phase);
    double getPowerFactor(uint8_t phase);
    double getEnergyKWh(uint8_t phase);
    double getTotalRealPower();
    bool isReady();

private:
    static void IRAM_ATTR isr_handler();
    void _handleInterrupt();
    void _startNextConversion();
    void _calculatePhaseResults(uint8_t phase);

    static ThreePhaseMonitor* _instance;

    Adafruit_ADS1115 _ads_v;
    Adafruit_ADS1115 _ads_i;
    uint8_t _rdyPin;

    PhaseData _phases[3];

    volatile bool _dataReady;
    volatile uint8_t _currentChannel;

    bool _isReadyFlag;
    unsigned long _lastCalculationTime;
    const unsigned int CALCULATION_INTERVAL = 1000;
};

#endif