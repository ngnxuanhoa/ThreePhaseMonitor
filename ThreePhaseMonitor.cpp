#include "ThreePhaseMonitor.h"

ThreePhaseMonitor::ThreePhaseMonitor() {}

bool ThreePhaseMonitor::begin(uint8_t v_addr, uint8_t i_addr, TwoWire &wire) {
    if (!_ads_v.begin(v_addr, &wire) || !_ads_i.begin(i_addr, &wire)) {
        return false;
    }

    _ads_v.setGain(GAIN_ONE);
    _ads_v.setDataRate(RATE_ADS1115_860SPS);
    _ads_i.setGain(GAIN_ONE);
    _ads_i.setDataRate(RATE_ADS1115_860SPS);

    _currentChannel = 0;
    _isReadyFlag = false;
    _lastCalculationTime = 0;

    // Khởi tạo các giá trị ban đầu cho các pha
    for (int i = 0; i < 3; i++) {
        _phases[i] = {0}; // Zero out the struct
	_phases[i].vOffset = 13200; 
        _phases[i].iOffset = 13200;
    }
    
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
    long sum_v[3] = {0, 0, 0};
    long sum_i[3] = {0, 0, 0};

    for (uint16_t i = 0; i < numSamples; i++) {
        for (uint8_t ch = 0; ch < 3; ch++) {
            sum_v[ch] += _ads_v.readADC_SingleEnded(ch);
            sum_i[ch] += _ads_i.readADC_SingleEnded(ch);
        }
    }

    for (uint8_t ch = 0; ch < 3; ch++) {
        _phases[ch].vOffset = sum_v[ch] / numSamples;
        _phases[ch].iOffset = sum_i[ch] / numSamples;
        Serial.printf("Pha %d -> V_Offset: %d, I_Offset: %d\n", ch + 1, _phases[ch].vOffset, _phases[ch].iOffset);
    }
}


void ThreePhaseMonitor::_calculatePhaseResults(uint8_t phase) {
    if (_phases[phase].sampleCount < 10) { 
        // Đặt các giá trị về 0 nếu không đủ mẫu
        _phases[phase].Vrms = 0; _phases[phase].Irms = 0; _phases[phase].realPower = 0;
        _phases[phase].apparentPower = 0; _phases[phase].reactivePower = 0; _phases[phase].powerFactor = 1.0;
    } else {
        PhaseData &p = _phases[phase];

        double v_rms_adc = sqrt(p.sumV_sq / p.sampleCount);
        double i_rms_adc = sqrt(p.sumI_sq / p.sampleCount);
        
        float maxVoltage = 4.096;
        p.Vrms = v_rms_adc * (maxVoltage / 32767.0) * p.vCal;
        p.Irms = i_rms_adc * (maxVoltage / 32767.0) / p.burdenResistor * p.iCal;

        double avg_power_adc = p.sumPower / p.sampleCount;
        
        p.apparentPower = p.Vrms * p.Irms;
        
        if (p.apparentPower > 1.0) { // Tránh chia cho 0 và các giá trị nhiễu nhỏ
            // Tính lại P từ các giá trị RMS đã hiệu chuẩn để có độ chính xác cao hơn
            double calibration_factor = (p.Vrms / v_rms_adc) * (p.Irms / i_rms_adc);
            p.realPower = avg_power_adc * calibration_factor;
            p.powerFactor = p.realPower / p.apparentPower;
        } else {
            p.realPower = 0;
            p.powerFactor = 1.0;
        }

        if (p.powerFactor > 1.0) p.powerFactor = 1.0;
        if (p.powerFactor < -1.0) p.powerFactor = -1.0;
        
        if (p.apparentPower > fabs(p.realPower)) {
            p.reactivePower = sqrt(p.apparentPower * p.apparentPower - p.realPower * p.realPower);
        } else {
            p.reactivePower = 0;
        }

        double elapsedSeconds = (float)CALCULATION_INTERVAL / 1000.0;
        double joules = p.realPower * elapsedSeconds;
        if (joules > 0) {
            p.kWh += joules / 3600000.0;
        }
    }

    // Reset các biến tổng cho chu kỳ tiếp theo
    _phases[phase].sumV_sq = 0;
    _phases[phase].sumI_sq = 0;
    _phases[phase].sumPower = 0;
    _phases[phase].sampleCount = 0;
}

void ThreePhaseMonitor::loop() {
    // Chỉ lấy mẫu khi chưa đến lúc tính toán
    if (millis() - _lastCalculationTime < CALCULATION_INTERVAL) {
        // Đọc đồng thời cặp V, I của kênh hiện tại
        int16_t v_raw = _ads_v.readADC_SingleEnded(_currentChannel);
        int16_t i_raw = _ads_i.readADC_SingleEnded(_currentChannel);

	// --- BỘ LỌC HIỆU CHUẨN OFFSET ĐỘNG ---
        // Sử dụng bộ lọc thông thấp IIR đơn giản để liên tục cập nhật offset
        const float alpha = 0.001; // Hệ số lọc. Càng nhỏ, bộ lọc càng "lì" và ổn định.
        
        PhaseData &p = _phases[_currentChannel]; // Tham chiếu đến pha hiện tại
        p.vOffset = (1.0 - alpha) * p.vOffset + alpha * v_raw;
        p.iOffset = (1.0 - alpha) * p.iOffset + alpha * i_raw;
        
        // --- Lọc DC Offset khỏi mẫu ---
        double v_sample = v_raw - p.vOffset;
        double i_sample = i_raw - p.iOffset;

        // --- Tích lũy các giá trị ---
        p.sumV_sq += v_sample * v_sample;
        p.sumI_sq += i_sample * i_sample;
        p.sumPower += v_sample * i_sample;
        p.sampleCount++;

        // Chuyển sang kênh tiếp theo
        _currentChannel = (_currentChannel + 1) % 3;
    } else {
        // Đã đến lúc tính toán và reset
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
