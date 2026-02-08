# Circuit Wiring Documentation
ESP32 WROOM (38-PIN)

                    +-----------------------+
                    | [ ] CLK        V5 [ ] |
                    | [ ] SDO       CMD [ ] |
                    | [ ] SD1       SD3 [ ] |
       (RFID) NSS <-| [X] G15       SD2 [ ] |
                    | [ ] G2        G13 [X] |-> MOSI (RFID)
                    | [ ] G0        GND [X] |-- GND (OLED/RFID)
                    | [ ] G4        G12 [X] |<- MISO (RFID)
       (OLED) RES <-| [X] G16       G14 [X] |-> SCK  (RFID)
       (OLED) D/C <-| [X] G17       G27 [X] |-> RST  (RFID)
       (OLED) CS  <-| [X] G5        G26 [ ] |
       (OLED) CLK <-| [X] G18       G25 [ ] |
                    | [ ] G19       G33 [ ] |
                    | [X] GND       G32 [ ] |
                    | [ ] G21       G35 [ ] |
                    | [ ] RXD       G34 [ ] |
                    | [ ] TXD       SN  [ ] |
                    | [ ] G22       SP  [ ] |
      (OLED)  DIN <-| [X] G23       EN  [ ] |
                    | [X] GND       3V3 [X] |-- 3.3V (OLED/RFID)
                    +-----------------------+
