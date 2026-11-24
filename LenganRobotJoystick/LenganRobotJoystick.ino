// ============================================================
//  Lengan Robot Dikontrol Joystick — ESP32 + 2x KY-023 + 4x SG90
//  ------------------------------------------------------------
//  Dua modul joystick KY-023 mengendalikan lengan robot 4 servo:
//    Joystick 1 : sumbu X -> base (putar), sumbu Y -> bahu
//    Joystick 2 : sumbu X -> siku,        sumbu Y -> gripper
//    Tombol joystick 1 (SW1) : kembali ke posisi awal (home)
//    Tombol joystick 2 (SW2) : buka / tutup gripper cepat (toggle)
//
//  Cara kerja: posisi joystick dibaca tiap 20 ms. Kalau stik
//  didorong dari tengah, sudut servo berubah sedikit demi sedikit
//  (kontrol kecepatan) sehingga gerakan lengan halus, tidak melonjak.
//  Zona mati di tengah joystick mencegah lengan bergerak sendiri
//  saat stik dilepas.
//
//  CATU DAYA PENTING: keempat servo dicatu dari adaptor 5 V
//  terpisah (bukan dari pin 5V ESP32) — satu servo SG90 bisa
//  menarik arus sampai 650 mA saat menahan beban. GND adaptor
//  dan GND ESP32 HARUS digabung, kalau tidak sinyal PWM tidak
//  punya acuan dan servo bergerak liar.
//
//  Library: ESP32Servo 3.2.1 (JANGAN 3.0.1 — gagal compile
//  di ESP32 core 4.x).
//  Board: ESP32 DevKit, ESP32 core 4.0.0.
//  Compile: arduino-cli compile --fqbn esp32:esp32:esp32 LenganRobotJoystick
// ============================================================

#include <ESP32Servo.h>

// ---------------- Pin servo (sinyal PWM) ----------------
const int PIN_SERVO_BASE    = 13;  // servo base (putar kiri-kanan)
const int PIN_SERVO_BAHU    = 12;  // servo bahu (angkat-turun)
const int PIN_SERVO_SIKU    = 14;  // servo siku (tekuk-lurus)
const int PIN_SERVO_GRIPPER = 27;  // servo gripper (jepit)

// ---------------- Pin joystick 1 (base + bahu) ----------------
const int PIN_JOY1_VRX = 34;  // sumbu X -> base (ADC1)
const int PIN_JOY1_VRY = 35;  // sumbu Y -> bahu (ADC1)
const int PIN_JOY1_SW  = 25;  // tombol tekan, aktif LOW

// ---------------- Pin joystick 2 (siku + gripper) ----------------
const int PIN_JOY2_VRX = 32;  // sumbu X -> siku (ADC1)
const int PIN_JOY2_VRY = 33;  // sumbu Y -> gripper (ADC1)
const int PIN_JOY2_SW  = 26;  // tombol tekan, aktif LOW

// ---------------- Konstanta kontrol ----------------
const int INTERVAL_BACA_MS   = 20;    // seberapa sering joystick dibaca
const int ADC_TENGAH         = 2048;  // nilai ADC saat stik di tengah (0-4095)
const int ZONA_MATI          = 150;   // dorongan di bawah ini diabaikan
const int LANGKAH_MAKS       = 2;     // perubahan sudut maks per tick (derajat)
const int DEBOUNCE_TOMBOL_MS = 200;   // jeda anti-pantul tombol

// Batas sudut tiap servo (derajat) — mencegah lengan menabrak bodinya sendiri
const int BASE_MIN = 0,   BASE_MAKS = 180;
const int BAHU_MIN = 20,  BAHU_MAKS = 160;
const int SIKU_MIN = 20,  SIKU_MAKS = 160;
const int GRIPPER_BUKA  = 10;  // capit terbuka
const int GRIPPER_TUTUP = 75;  // capit tertutup (menjepit)

// Posisi awal (home): lengan tegak, capit terbuka
const int HOME_BASE = 90, HOME_BAHU = 90, HOME_SIKU = 90;

// ---------------- Objek servo ----------------
Servo servoBase, servoBahu, servoSiku, servoGripper;

// Sudut saat ini (disimpan agar gerakan bertahap, bukan melonjak)
int sudutBase = HOME_BASE;
int sudutBahu = HOME_BAHU;
int sudutSiku = HOME_SIKU;
int sudutGripper = GRIPPER_BUKA;

unsigned long waktuBacaTerakhir = 0;
unsigned long waktuTombolTerakhir = 0;
bool gripperTertutup = false;

// Mengubah satu sudut sesuai dorongan joystick.
//   adc: nilai analog 0-4095; makin jauh dari tengah, makin besar langkahnya.
//   Hasil dibatasi ke [batasMin, batasMaks].
int hitungSudutBaru(int sudut, int adc, int batasMin, int batasMaks) {
  int selisih = adc - ADC_TENGAH;
  if (abs(selisih) < ZONA_MATI) {
    return sudut;  // stik di tengah: diam
  }
  // Dorongan penuh (selisih ~2048) = LANGKAH_MAKS derajat per tick.
  int langkah = map(abs(selisih), ZONA_MATI, ADC_TENGAH, 0, LANGKAH_MAKS);
  langkah = constrain(langkah, 0, LANGKAH_MAKS);
  if (selisih > 0) {
    sudut += langkah;
  } else {
    sudut -= langkah;
  }
  return constrain(sudut, batasMin, batasMaks);
}

// Menulis semua sudut ke servo (dipanggil sekali tiap tick).
void tulisSemuaServo() {
  servoBase.write(sudutBase);
  servoBahu.write(sudutBahu);
  servoSiku.write(sudutSiku);
  servoGripper.write(sudutGripper);
}

// Kembalikan lengan ke posisi awal dan buka capit.
void kembaliKeHome() {
  sudutBase = HOME_BASE;
  sudutBahu = HOME_BAHU;
  sudutSiku = HOME_SIKU;
  sudutGripper = GRIPPER_BUKA;
  gripperTertutup = false;
  tulisSemuaServo();
}

// Balik keadaan capit: tertutup -> buka, terbuka -> tutup.
void toggleGripper() {
  gripperTertutup = !gripperTertutup;
  sudutGripper = gripperTertutup ? GRIPPER_TUTUP : GRIPPER_BUKA;
  servoGripper.write(sudutGripper);
}

void setup() {
  Serial.begin(115200);

  // Tombol joystick: aktif LOW, pakai pull-up internal.
  pinMode(PIN_JOY1_SW, INPUT_PULLUP);
  pinMode(PIN_JOY2_SW, INPUT_PULLUP);

  // Pasang servo ke pin PWM-nya (ESP32Servo memakai timer 50 Hz).
  servoBase.attach(PIN_SERVO_BASE);
  servoBahu.attach(PIN_SERVO_BAHU);
  servoSiku.attach(PIN_SERVO_SIKU);
  servoGripper.attach(PIN_SERVO_GRIPPER);

  kembaliKeHome();
  Serial.println("Lengan robot siap. Gerakkan joystick 1 dan 2.");
  Serial.println("Tombol joystick 1 = home, tombol joystick 2 = buka/tutup gripper.");
}

void loop() {
  unsigned long sekarang = millis();

  // --- Tombol (dicek tiap loop, dengan debounce) ---
  if (sekarang - waktuTombolTerakhir > DEBOUNCE_TOMBOL_MS) {
    if (digitalRead(PIN_JOY1_SW) == LOW) {
      kembaliKeHome();
      waktuTombolTerakhir = sekarang;
      Serial.println("Kembali ke posisi awal.");
    } else if (digitalRead(PIN_JOY2_SW) == LOW) {
      toggleGripper();
      waktuTombolTerakhir = sekarang;
      Serial.println(gripperTertutup ? "Gripper menutup." : "Gripper membuka.");
    }
  }

  // --- Joystick (dibaca tiap INTERVAL_BACA_MS) ---
  if (sekarang - waktuBacaTerakhir >= INTERVAL_BACA_MS) {
    waktuBacaTerakhir = sekarang;

    // Joystick 1: X -> base, Y -> bahu (Y dibalik supaya dorong
    // ke depan = bahu naik, seperti tuas sungguhan).
    sudutBase = hitungSudutBaru(sudutBase, analogRead(PIN_JOY1_VRX),
                                BASE_MIN, BASE_MAKS);
    sudutBahu = hitungSudutBaru(sudutBahu, ADC_TENGAH * 2 - analogRead(PIN_JOY1_VRY),
                                BAHU_MIN, BAHU_MAKS);

    // Joystick 2: X -> siku, Y -> gripper (dorong ke depan = menutup).
    sudutSiku = hitungSudutBaru(sudutSiku, analogRead(PIN_JOY2_VRX),
                                SIKU_MIN, SIKU_MAKS);
    sudutGripper = hitungSudutBaru(sudutGripper,
                                   analogRead(PIN_JOY2_VRY),
                                   GRIPPER_BUKA, GRIPPER_TUTUP);

    tulisSemuaServo();
  }
}
