# Lengan Robot Dikontrol Joystick — ESP32 + 2 Joystick KY-023 + 4 Servo SG90

Lengan robot yang bergerak mengikuti gerakan jarimu. Dua modul joystick
KY-023 menjadi "tangan" pengendali: joystick pertama menggerakkan base
(putar) dan bahu (angkat), joystick kedua menggerakkan siku (tekuk) dan
gripper (jepit). Dorong stik sedikit, lengan langsung mengikuti — tanpa
mengetik perintah, tanpa tombol yang ribet.

Video penjelasan (60 detik):
link menyusul setelah upload

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak lengan robot |
| Joystick KY-023 (2x) | Tiap modul: 2 sumbu analog (VRx, VRy) + 1 tombol tekan (SW) |
| Servo SG90 (4x) | Base, bahu, siku, gripper — PWM 50 Hz |
| Adaptor 5 V 2 A | Catu khusus servo (bukan dari ESP32) |
| Lengan robot 4 servo | Rangka base, bahu, siku, dan gripper |

## Wiring

| Dari | Ke |
|---|---|
| Joystick 1 VRx | ESP32 GPIO34 |
| Joystick 1 VRy | ESP32 GPIO35 |
| Joystick 1 SW | ESP32 GPIO25 |
| Joystick 2 VRx | ESP32 GPIO32 |
| Joystick 2 VRy | ESP32 GPIO33 |
| Joystick 2 SW | ESP32 GPIO26 |
| Joystick VCC (keduanya) | ESP32 3V3 |
| Servo base SIG | ESP32 GPIO13 |
| Servo bahu SIG | ESP32 GPIO12 |
| Servo siku SIG | ESP32 GPIO14 |
| Servo gripper SIG | ESP32 GPIO27 |
| Semua servo VCC | Adaptor 5 V (+) |
| Semua GND | Digabung (joystick, ESP32, adaptor -, servo) |

Skema lengkap: [schematic.png](schematic.png)

Perhatian daya: keempat servo dicatu dari adaptor 5 V 2 A terpisah.
Satu servo SG90 bisa menarik arus sampai 650 mA saat menahan beban,
jadi jangan catu servo dari pin 5V ESP32 — regulatornya tidak kuat.
GND adaptor dan GND ESP32 HARUS digabung, kalau tidak sinyal PWM
tidak punya acuan dan servo bergerak liar. ESP32 sendiri dicatu
lewat USB.

## Cara kerja

Posisi joystick dibaca tiap 20 milidetik. Kalau stik didorong dari
tengah, sudut servo berubah sedikit demi sedikit (kontrol kecepatan):
makin jauh dorongan, makin cepat gerakan lengan — jadi gerakannya
halus, tidak melonjak. Zona mati di tengah joystick (±150 dari 2048)
mencegah lengan bergerak sendiri saat stik dilepas.

| Joystick | Gerakan |
|---|---|
| Joystick 1, sumbu X | Base berputar kiri-kanan (0–180 derajat) |
| Joystick 1, sumbu Y | Bahu naik-turun (20–160 derajat) |
| Joystick 2, sumbu X | Siku menekuk-meluruskan (20–160 derajat) |
| Joystick 2, sumbu Y | Gripper membuka-menutup (10–75 derajat) |
| Tombol joystick 1 | Kembali ke posisi awal (home) |
| Tombol joystick 2 | Buka/tutup gripper cepat (toggle) |

Batas sudut bahu dan siku (20–160 derajat) mencegah lengan menabrak
bodinya sendiri. Gripper menutup di 75 derajat dan membuka di
10 derajat — cukup untuk menjepit benda kecil seperti tutup botol
atau balok mainan.

## Konfigurasi (di awal sketch)

- `INTERVAL_BACA_MS` (20) — seberapa sering joystick dibaca.
- `ZONA_MATI` (150) — dorongan di bawah ini diabaikan.
- `LANGKAH_MAKS` (2) — perubahan sudut maks per tick (derajat).
- `DEBOUNCE_TOMBOL_MS` (200) — jeda anti-pantul tombol.
- `HOME_BASE` / `HOME_BAHU` / `HOME_SIKU` (90) — posisi awal lengan.
- `GRIPPER_BUKA` (10) / `GRIPPER_TUTUP` (75) — sudut capit.

Kalau lengan bergerak sendiri saat stik dilepas, naikkan `ZONA_MATI`;
kalau gerakan terasa lambat, naikkan `LANGKAH_MAKS`; kalau lengan
bergetar di ujung gerakan, kecilkan batas `BAHU_MAKS` / `SIKU_MAKS`.

## Library yang dibutuhkan

- ESP32Servo 3.2.1 — sudah disertakan di [libraries/ESP32Servo.zip](libraries/ESP32Servo.zip).
  JANGAN pakai ESP32Servo 3.0.1 — gagal compile di ESP32 core 4.x.
  Cara install: di Arduino IDE, Sketch > Include Library >
  Add .ZIP Library, lalu pilih file ZIP di atas.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 LenganRobotJoystick
```

Terverifikasi 24 Nov 2026: sketch 308.595 byte (23% flash),
RAM 23.948 byte (7%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
