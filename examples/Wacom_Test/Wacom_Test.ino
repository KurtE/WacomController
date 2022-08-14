// Test for some Wacom Tablets
// This is a WIP.
//
// This example is in the public domain

#include "USBHost_t36.h"
#include "WacomController.h"
#include "HIDFeatureReports.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
WacomController digi1(myusb);

int axis_prev[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

USBDriver *drivers[] = { &hub1, &hub2, &hid1, &hid2, &hid3, &hid4, &hid5 };
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = { "Hub1", "Hub2", "HID1", "HID2", "HID3", "HID4", "HID5" };
bool driver_active[CNT_DEVICES] = { false, false, false, false };

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = { &digi1 };
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_DEVICES] = { "digi1" };
bool hid_driver_active[CNT_DEVICES] = { false };
bool show_changed_only = false;
void setup() {
  Serial1.begin(2000000);
  while (!Serial)
    ;  // wait for Arduino Serial Monitor
  Serial.println("\n\nWacom Tablet class Testing");
  if (CrashReport)Serial.print(CrashReport);
  myusb.begin();
}


void loop() {
  myusb.Task();

  if (Serial.available()) {
    int ch = Serial.read();  // get the first char.
    while (Serial.read() != -1)
      ;
    if (ch == 'c') {
      show_changed_only = !show_changed_only;
      if (show_changed_only) Serial.println("\n** Turned on Show Changed Only mode **");
      else
        Serial.println("\n** Turned off Show Changed Only mode **");
    } else if (ch == 'h') {
      PrintHIDFeaturesInfo();
    } else {
      if (digi1.debugPrint()) {
        digi1.debugPrint(false);
        Serial.println("\n*** Turned off debug printing ***");
      } else {
        digi1.debugPrint(true);
        Serial.println("\n*** Turned on debug printing ***");
      }
    }
  }

  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }



  if (digi1.available()) {
    int touch_count = digi1.getTouchCount();
    int touch_index;
    uint16_t buttons_bin = digi1.getButtons();
    bool pen_button[3];
    Serial.printf("Digitizer: ");
    for (int i = 0; i < 3; i++) {
      pen_button[i] = (buttons_bin >> i) & 0x1;

      Serial.printf("Pen_Btn%d:%d ", i, pen_button[i]);
    }

    switch (digi1.eventType()) {
      case WacomController::TOUCH:
        Serial.print(" Touch:");
        for (touch_index = 0; touch_index < touch_count; touch_index++) {
          Serial.printf(" (%d, %d)", digi1.getX(touch_index), digi1.getY(touch_index));
        }
        break;
      case WacomController::PEN:
        Serial.printf(" Pen: (%d, %d) Pressure: %u Distance: %u", digi1.getX(), digi1.getY(),
            digi1.getPenPressure(), digi1.getPenDistance());
        Serial.printf(" TiltX: %d TiltY: %d",digi1.getTiltX(), digi1.getTiltY());
        break;
      case WacomController::SIDE_CTRL:
        {
          //wheel data 0-71
          Serial.printf(" W hl: %d ", digi1.getIntuosWheel());
          //wheel button binary, no touch functionality
          Serial.printf(" WhlBtn: %d ", digi1.getIntuosWheelButton());

          //buttons are saved within one byte for all the 8 buttons, here is a decoding example
          uint16_t button_touch_bin = digi1.getIntuosButtonTouch();
          uint16_t button_press_bin = digi1.getIntuosButtonPress();
          bool button_touched[8];
          bool button_pressed[8];
          for (int i = 0; i < 8; i++) {
            button_touched[i] = (button_touch_bin >> i) & 0x1;
            button_pressed[i] = (button_press_bin >> i) & 0x1;
            Serial.printf("Btn%d: T:%d P:%d ", i, button_touched[i], button_pressed[i]);
          }
        }          
        break;
      default:
        Serial.printf(",  X = %u, Y = %u", digi1.getX(), digi1.getY());
        Serial.print(",  Pressure: = ");
        Serial.print(",  wheel = ");
        Serial.print(digi1.getWheel());
        Serial.print(",  wheelH = ");
        Serial.print(digi1.getWheelH());
        if (show_changed_only) Serial.print("\t: ");
        else
          Serial.println();
        for (uint8_t i = 0; i < 16; i++) {
          int axis = digi1.getAxis(i);
          if (show_changed_only) {
            if (axis != axis_prev[i]) Serial.printf(" %d#%d(%x)", i, axis, axis);
          } else
            Serial.printf(" %d%c%d(%x)", i, (axis == axis_prev[i]) ? ':' : '#', axis, axis);
          axis_prev[i] = axis;
        }
        break;
    }
    Serial.println();
    digi1.digitizerDataClear();
  }
}

//=============================================================================
// Hack to see if we can enumerate the HID Feature Reports.
//=============================================================================
void PrintHIDFeaturesInfo() {

  HIDFeatureReports hfr;


  // This is a real giant hack!
  uint8_t drivers_cnt = digi1.getDriverCnt();

  for (uint8_t i = 0; i < drivers_cnt; i++ ) {
    Serial.println("\n*********************************************");
    hfr.setHIDParser(digi1.getDriver(i));
    hfr.parseHIDReportDescriptor(true);
    hfr.printFeatureReportList();
  }

}
