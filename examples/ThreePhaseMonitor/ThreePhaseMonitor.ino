#include <ThreePhaseMonitor.h>

// --- CẤU HÌNH ---
#define ADS_V_ADDR 0x48      // Địa chỉ I2C của ADS đo điện áp (ADDR nối GND)
#define ADS_I_ADDR 0x49      // Địa chỉ I2C của ADS đo dòng điện (ADDR nối VCC)

// Tạo một instance của thư viện
ThreePhaseMonitor emon;

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Three Phase Energy Monitor (Polling Mode) ---");

  // Khởi tạo thư viện (chỉ có 1 cách duy nhất)
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
                  //(Analog_pin, Vol_ratio, Cur_ratio, burdenRes)
  emon.configurePhase(0, 20.0, 1000.0, 10.0); // Pha R (A0)
  emon.configurePhase(1, 20.0, 1000.0, 10.0); // Pha S (A1)
  emon.configurePhase(2, 20.0, 1000.0, 10.0); // Pha T (A2)
  
  Serial.println("He thong da san sang do luong...");
}

unsigned long lastPrintTime = 0;

void loop() {
  emon.loop();

  if (millis() - lastPrintTime >= 2000) {
    lastPrintTime = millis();
    if (emon.isReady()) {
      Serial.println("==================================================");
      for (int i = 0; i < 3; i++) {
        Serial.printf("PHA %d | V: %.2fV | I: %.3fA | P: %.2fW | PF: %.2f | E: %.6fkWh\n",
          i + 1,
          emon.getVrms(i),
          emon.getIrms(i),
          emon.getRealPower(i),
          emon.getPowerFactor(i),
          emon.getEnergyKWh(i)
        );
      }
      Serial.printf("TONG CONG SUAT THUC: %.2f W\n", emon.getTotalRealPower());
    }
  }
}
