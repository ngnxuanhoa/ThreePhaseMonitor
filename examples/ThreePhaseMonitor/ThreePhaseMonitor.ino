#include "ThreePhaseMonitor.h"

ThreePhaseMonitor* ThreePhaseMonitor::_instance = nullptr;

ThreePhaseMonitor::ThreePhaseMonitor() {}

bool ThreePhaseMonitor::begin(uint8_t v_addr, uint8_t i_addr, uint8_t rdyPin, TwoWire &wire) {
    _rdyPin = rdyPin;
    _instance = this;

    if (!_ads_v.begin(v_addr, &wire) || !_ads_i.begin(i_addr, &wire)) {
        return false;
    }

    _ads_v.setGain(GAIN_ONE);
    _ads_v.setDataRate(RATE_ADS1115_860SPS);
    _ads_i.setGain(GAIN_ONE);
    _ads_i.setDataRate(RATE_ADS1115_860SPS);

    // Dùng ADC dòng điện làm nguồn ngắt
    _ads_i.writeRegister(ADS1115_REG_POINTER_HITHRESH, 0x8000);
    _ads_i.writeRegister(ADS1115_REG_POINTER_LOTHRESH, 0x0000);

    pinMode(_rdyPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(_rdyPin), isr_handler, FALLING);

    _currentChannel = 0;
    _dataReady = false;
    _isReadyFlag = false;
    
    return true;
}

void ThreePhaseMonitor::configurePhase(uint8_t phase, float vCal, float iCal, float burdenResistor) {
    if (phase < 3) {
        _phases[phase].vCal = vCal;
        _phases[phase].iCal = iCal;
        _phases[phase].burdenResistor = burdenResistor;
    }
}

void ThreePhaseMonitor::calibrateOffsets(uint16_t numSamples) {
    detachInterrupt(digitalPinToInterrupt(_rdyPin));
    Serial.println("Bat dau hieu chuan Offsets. Dam bao khong co tai!");

    long sum_v[3] = {0, 0, 0};
    long sum_i[3] = {0, 0, 0};

    for (uint16_t i = 0; i < numSamples; i++) {
        for (uint8_t ch = 0; ch < 3; ch++) {
            sum_v[ch] += _ads_v.readADC_SingleEnded(ch);
            sum_i[ch] += _ads_i.readADC_SingleEnded(ch);
        }
        delay(2);
    }

    for (uint8_t ch = 0; ch < 3; ch++) {
        _phases[ch].vOffset = sum_v[ch] / numSamples;
        _phases[ch].iOffset = sum_i[ch] / numSamples;
        Serial.printf("Pha %d -> V_Offset: %d, I_Offset: %d\n", ch + 1, _phases[ch].vOffset, _phases[ch].iOffset);
    }
    
    attachInterrupt(digitalPinToInterrupt(_rdyPin), isr_handler, FALLING);
    _startNextConversion(); // Bắt đầu chu trình đo lường
}

void IRAM_ATTR ThreePhaseMonitor::isr_handler() {
    if (_instance) _instance->_dataReady = true;
}

void ThreePhaseMonitor::_startNextConversion() {
    // Bắt đầu đo đồng thời trên cả hai ADC cho kênh hiện tại
    _ads_v.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0 + _currentChannel, false);
    _ads_i.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0 + _currentChannel, false);
}

void ThreePhaseMonitor::_handleInterrupt() {
    // Đọc kết quả
    int16_t v_raw = _ads_v.getLastConversionResults();
    int16_t i_raw = _ads_i.getLastConversionResults();

    // Lọc DC offset
    double v_sample = v_raw - _phases[_currentChannel].vOffset;
    double i_sample = i_raw - _phases[_currentChannel].iOffset;

    // Tích lũy các giá trị
    _phases[_currentChannel].sumV_sq += v_sample * v_sample;
    _phases[_currentChannel].sumI_sq += i_sample * i_sample;
    _phases[_currentChannel].sumPower += v_sample * i_sample;
    _phases[_currentChannel].sampleCount++;

    // Chuyển kênh và bắt đầu đo tiếp
    _currentChannel = (_currentChannel + 1) % 3;
    _startNextConversion();
}

void ThreePhaseMonitor::_calculatePhaseResults(uint8_t phase) {
    if (_phases[phase].sampleCount == 0) return;

    PhaseData &p = _phases[phase];

    // Tính Vrms và Irms từ các giá trị ADC
    double v_rms_adc = sqrt(p.sumV_sq / p.sampleCount);
    double i_rms_adc = sqrt(p.sumI_sq / p.sampleCount);
    
    // Chuyển đổi sang giá trị thực
    float maxVoltage = 4.096; // Do Gain = 1
    p.Vrms = v_rms_adc * (maxVoltage / 32767.0) * p.vCal;
    p.Irms = i_rms_adc * (maxVoltage / 32767.0) / p.burdenResistor * p.iCal;

    // Tính công suất
    double avg_power_adc = p.sumPower / p.sampleCount;
    p.realPower = avg_power_adc * (p.Vrms / v_rms_adc) * (p.Irms / i_rms_adc);
    p.apparentPower = p.Vrms * p.Irms;
    
    if (p.apparentPower != 0) {
        p.powerFactor = p.realPower / p.apparentPower;
        // Giới hạn giá trị PF trong khoảng -1 đến 1
        if (p.powerFactor > 1.0) p.powerFactor = 1.0;
        if (p.powerFactor < -1.0) p.powerFactor = -1.0;
    } else {
        p.powerFactor = 1.0;
    }

    if (p.apparentPower > p.realPower) {
        p.reactivePower = sqrt(p.apparentPower * p.apparentPower - p.realPower * p.realPower);
    } else {
        p.reactivePower = 0;
    }

    // Tích lũy năng lượng (kWh)
    double elapsedSeconds = (float)CALCULATION_INTERVAL / 1000.0;
    double joules = p.realPower * elapsedSeconds;
    p.kWh += joules / 3600000.0;

    // Reset các biến tổng
    p.sumV_sq = 0;
    p.sumI_sq = 0;
    p.sumPower = 0;
    p.sampleCount = 0;
}

void ThreePhaseMonitor::loop() {
    if (_dataReady) {
        _dataReady = false;
        _handleInterrupt();
    }

    if (millis() - _lastCalculationTime >= CALCULATION_INTERVAL) {
        _lastCalculationTime = millis();
        for (uint8_t i = 0; i < 3; i++) {
            _calculatePhaseResults(i);
        }
        _isReadyFlag = true;
    }
}

// --- Getters ---
double ThreePhaseMonitor::getVrms(uint8_t phase) { return (phase < 3) ? _phases[phase].Vrms : 0; }
double ThreePhaseMonitor::getIrms(uint8_t phase) { return (phase < 3) ? _phases[phase].Irms : 0; }
double ThreePhaseMonitor::getRealPower(uint8_t phase) { return (phase < 3) ? _phases[phase].realPower : 0; }
double ThreePhaseMonitor::getApparentPower(uint8_t phase) { return (phase < 3) ? _phases[phase].apparentPower : 0; }
double ThreePhaseMonitor::getReactivePower(uint8_t phase) { return (phase < 3) ? _phases[phase].reactivePower : 0; }
double ThreePhaseMonitor::getPowerFactor(uint8_t phase) { return (phase < 3) ? _phases[phase].powerFactor : 0; }
double ThreePhaseMonitor::getEnergyKWh(uint8_t phase) { return (phase < 3) ? _phases[phase].kWh : 0; }
bool ThreePhaseMonitor::isReady() { return _isReadyFlag; }
double ThreePhaseMonitor::getTotalRealPower() {
    return _phases[0].realPower + _phases[1].realPower + _phases[2].realPower;
}
