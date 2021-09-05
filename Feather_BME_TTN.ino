#include <lmic.h>
#include <hal/hal.h>
#include <Wire.h>
#include <SPI.h>
#include <SHT85.h>



#define TTN_APPEUI 0xFB, 0x41, 0x52, 0xB1, 0x10, 0xB6, 0x76, 0x98
#define TTN_DEVEUI 0xFB, 0x41, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70// 70B3D57ED00441FB
#define TTN_APPKEY 0x6C, 0x7C, 0xDF, 0x73,0xA9, 0x4F, 0xF4, 0x25, 0xFE, 0x84, 0xDA, 0x54, 0xEC, 0x29, 0x43, 0x55
#define SEALEVELPRESSURE_HPA (1013.25)
#define SHT85_ADDRESS 0x44

SHT35 sht;

static const u1_t PROGMEM APPEUI[8] = {TTN_APPEUI};
void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}

static const u1_t PROGMEM DEVEUI[8] = {TTN_DEVEUI};
void os_getDevEui (u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}

static const u1_t PROGMEM APPKEY[16] = {TTN_APPKEY};
void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

static uint8_t payload [5];
static osjob_t sendjob;
const unsigned TX_INTERVAL = 30;

const lmic_pinmap lmic_pins = {
  .nss = 8,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 4,
  .dio = {3, 6, LMIC_UNUSED_PIN},
  .rxtx_rx_active = 0,
  .rssi_cal = 8,        // LBT cal for the Adafruit Feather M0 LoRa, in dB
  .spi_freq = 8000000,  // 8 Million
};

void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
}

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      Serial.println(F("EV_JOINED"));
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devaddr: ");
        Serial.println(devaddr, HEX);
        Serial.print("AppSKey: ");
        for (size_t i = 0; i < sizeof(artKey); ++i) {
          if (i != 0)
            Serial.print("-");
          printHex2(artKey[i]);
        }
        Serial.println("");
        Serial.print("NwkSKey: ");
        for (size_t i = 0; i < sizeof(nwkKey); ++i) {
          if (i != 0)
            Serial.print("-");
          printHex2(nwkKey[i]);
        }
        Serial.println();
      }
      // Disable link check validation (automatically enabled
      // during join, but because slow data rates change max TX
      // size, we don't use it in this example.
      LMIC_setLinkCheckMode(0);
      break;
    /*
      || This event is defined but not used in the code. No
      || point in wasting codespace on it.
      ||
      || case EV_RFU1:
      ||     Serial.println(F("EV_RFU1"));
      ||     break;
    */
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        Serial.println(F("Received "));
        Serial.println(LMIC.dataLen);
        Serial.println(F(" bytes of payload"));
      }
      // Schedule next transmission
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    /*
      || This event is defined but not used in the code. No
      || point in wasting codespace on it.
      ||
      || case EV_SCAN_FOUND:
      ||    Serial.println(F("EV_SCAN_FOUND"));
      ||    break;
    */
    case EV_TXSTART:
      Serial.println(F("EV_TXSTART"));
      break;
    case EV_TXCANCELED:
      Serial.println(F("EV_TXCANCELED"));
      break;
    case EV_RXSTART:
      /* do not print anything -- it wrecks timing */
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
      break;

    default:
      Serial.print(F("Unknown event: "));
      Serial.println((unsigned) ev);
      break;
  }
}

void do_send(osjob_t* j) {
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  }
  else {
    sht.read();
    float temperature = sht.getTemperature();
    Serial.print("Temperature: "); Serial.print(temperature);
    Serial.println(" °C");
    temperature = temperature / 100;

    float rHumidity = sht.getHumidity();
    Serial.print("%RH ");
    Serial.println(rHumidity);

    rHumidity = rHumidity / 100;

    // float -> int
    uint16_t payloadTemp = LMIC_f2sflt16(temperature);
    // int -> bytes
    byte tempLow = lowByte(payloadTemp);
    byte tempHigh = highByte(payloadTemp);
    payload[0] = tempLow;
    payload[1] = tempHigh;

    uint16_t payloadHum = LMIC_f2sflt16(rHumidity);
    byte humLow = lowByte(payloadHum);
    byte humHigh = highByte(payloadHum);
    payload[2] = humLow;
    payload[3] = humHigh;
    Serial.print("Pld: ");
    for (int i = 0; i < sizeof(payload); i++){
      Serial.print(payload[i]);
      Serial.print(" ");
    }
    Serial.println();
    LMIC_setTxData2(1, payload, sizeof(payload) - 1, 0);
  }
  // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
  // put your setup code here, to run once:
  delay(5000);
  while (!Serial);
  Serial.begin(9600);
  Serial.println(F("Starting..."));
  Serial.println(__FILE__);
  Serial.print("SHT_LIB_VERSION: \t");
  Serial.println(SHT_LIB_VERSION);

  Wire.begin();
  sht.begin(SHT85_ADDRESS);
  Wire.setClock(100000);
  uint16_t stat = sht.readStatus();
  Serial.print(stat, HEX);
  Serial.println();
  
  os_init();
  LMIC_reset();
  LMIC_setLinkCheckMode(0);
  LMIC_setDrTxpow(DR_SF7, 14);
  //    LMIC_selectSubBand(1);
  do_send(&sendjob);
}

void loop() {
  // put your main code here, to run repeatedly:
  os_runloop_once();
}
