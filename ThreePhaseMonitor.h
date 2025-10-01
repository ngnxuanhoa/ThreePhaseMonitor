#ifndef ThreePhaseMonitor_h
#define ThreePhaseMonitor_h

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

struct PhaseData {
    float vCal;
    float iCal;
    float burdenResistor;
    int vOffset;
    int iOffset;
    double sumV_sq;
    double sumI_sq;
    double sumPower;
    long sampleCount;
    double Vrms;
    double Irms;
    double realPower;
    double apparentPower;
    double reactivePower;
    double powerFactor;
    double kWh;
};

class ThreePhaseMonitor {
public:
    ThreePhaseMonitor();

    bool begin(uint8_t v_addr, uint8_t i_addr, TwoWire &wire = Wire);
    
    void configurePhase(uint8_t phase, float vCal, float iCal, float burdenResistor);
    void calibrateOffsets(uint16_t numSamples = 1024);
    void loop();

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
    void _calculatePhaseResults(uint8_t phase);

    Adafruit_ADS1115 _ads_v;
    Adafruit_ADS1115 _ads_i;

    PhaseData _phases[3];

    uint8_t _currentChannel;
    bool _isReadyFlag;
    unsigned long _lastCalculationTime;
    const unsigned int CALCULATION_INTERVAL = 1000;
};

#endif
