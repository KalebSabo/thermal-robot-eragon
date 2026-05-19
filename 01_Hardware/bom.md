# Project Eragon – Bill of Materials (BOM)
- Below is a reference! NOT what is actually being used.
  - i.e. using an ESP32 rather than an Arduino

## Core Electronics & Control
- **Arduino Uno REV3**  
  Quantity: 1  
  Model/Brand: Official or compatible  
  Purpose: Main microcontroller

- **PCA9685 16-Channel PWM Driver**  
  Quantity: 2  
  Model/Brand: HiLetgo  
  Purpose: Precise PWM control for 4+ servos (I²C)

- **MPU-6050 6-Axis IMU Module**  
  Quantity: 3  
  Model/Brand: HiLetgo GY-521  
  Purpose: Tilt and balance feedback (only 1 needed initially)

- **XL4015 5A Adjustable Buck Converter**  
  Quantity: 2  
  Model/Brand: HiLetgo (with LED voltmeter)  
  Purpose: 5 V logic rail + 6 V servo rail

- **2200 µF 25 V Rubycon Capacitor**  
  Quantity: 2+  
  Model/Brand: Rubycon YXJ series  
  Purpose: Low-ESR smoothing on each buck converter output

## Actuators & Mechanical
- **MG996R Metal-Gear Digital Servo**  
  Quantity: 4  
  Model/Brand: Deegoo  
  Purpose: Hip and knee joints (expandable to 6–8)

- **25T Aluminum Servo Horn**  
  Quantity: 4  
  Model/Brand: Acekeeps (long-arm style)  
  Purpose: Attachment point for linkages

- **5 mm M3 Ball Head / Ball Stud**  
  Quantity: 10  
  Model/Brand: Tamiya DF-02  
  Purpose: Linkage joints (hip-knee to foot)

## Power & Wiring
- **7.4 V 1200 mAh 2S LiPo Battery**  
  Quantity: 1  
  Model/Brand: VICMILE SCX24 style  
  Purpose: Main power source (PH2.0 connector)

- **2S LiPo Balance Charger**  
  Quantity: 1  
  Model/Brand: Included with VICMILE pack  
  Purpose: USB charger (included)

- **PH2.0 JST On/Off Switch Cable**  
  Quantity: 1  
  Model/Brand: Adafruit  
  Purpose: Inline power switch for battery

- **Anker Nano 45 W USB-C PD Charger**  
  Quantity: 1  
  Model/Brand: Anker  
  Purpose: For Pinecil soldering iron

- **ELEGOO Dupont Jumper Wires**  
  Quantity: 120  
  Model/Brand: Multicolored 40-pin set  
  Purpose: Breadboard prototyping

- **18–20 AWG Silicone Wire (optional)**  
  Quantity: —  
  Model/Brand: Red / black  
  Purpose: Custom power harnesses

## Assembly & Tools
- **M3 Heat-Set Brass Inserts**  
  Quantity: 100+  
  Model/Brand: HANGLIFE  
  Purpose: Strong threads in printed parts

- **M2–M8 Heat-Set Insert Tips**  
  Quantity: 1 set  
  Model/Brand: Quicko  
  Purpose: For Pinecil

- **Pinecil Smart Soldering Iron**  
  Quantity: 1  
  Model/Brand: Pine64  
  Purpose: Heat-set insert installation

- **Bondhus Ball-End Hex Key Set**  
  Quantity: 1  
  Model/Brand: 10999 (1.5–10 mm)  
  Purpose: Tightening M3 button-head screws

- **M2/M3/M4/M5 Screw & Nut Assortment**  
  Quantity: ~2200  
  Model/Brand: Fgruh / similar  
  Purpose: Structural and servo mounting

## Notes
- Quantities are sized for the initial 4-servo build; most items support easy expansion (additional servos, ankles, etc.).
- All components are selected for compatibility with MG996R servos and a mostly 3D-printed structure.