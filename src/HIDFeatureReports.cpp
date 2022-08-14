/* 
   USB EHCI Host for Teensy 3.6

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, shiublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "HIDFeatureReports.h"

void HIDFeatureReports::parseHIDReportDescriptor(bool print_verbose ) 
{
  const uint8_t *p = phidp_->getHIDReportDescriptor();
  uint16_t hid_report_size = phidp_->getHIDReportDescriptorSize();
  const uint8_t *pend = p + hid_report_size;
  uint8_t collection_level = 0;
  uint16_t usage_page = 0;
  enum { USAGE_LIST_LEN = 24 };

  uint16_t usage[USAGE_LIST_LEN] = { 0, 0 };
  uint8_t usage_count = 0;
  cnt_feature_reports_ = 0;

  // here is a feature object that we will fill in the pieces as we go.
  FEATURE_REPORTS_t feature = {0,0,0,0, 0, 0, 0};

  
  if (print_verbose) Serial.printf("\nParsing Report descriptor (%p) size: %u\n", p, hid_report_size);
  while (p < pend) {
    uint8_t tag = *p;
    for (uint8_t i = 0; i < collection_level; i++) if (print_verbose) Serial.print("  ");
    if (print_verbose) Serial.printf("  %02X", tag);

    if (tag == 0xFE) {  // Long Item (unsupported)
      p += p[1] + 3;
      continue;
    }
    uint32_t val;
    switch (tag & 0x03) {  // Short Item data
      case 0:
        val = 0;
        p++;
        break;
      case 1:
        val = p[1];
        // could be better;
        if (print_verbose) Serial.printf(" %02X", p[1]);
        p += 2;
        break;
      case 2:
        val = p[1] | (p[2] << 8);
        if (print_verbose) Serial.printf(" %02X %02X", p[1], p[2]);
        p += 3;
        break;
      case 3:
        val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
        if (print_verbose) Serial.printf(" %02X %02X %02X %02X", p[1], p[2], p[3], p[4]);
        p += 5;
        break;
    }
    if (p > pend) break;

    bool reset_local = false;
    switch (tag & 0xfc) {
      case 0x4:  //usage Page
        {
          usage_page = val;
          if (print_verbose) Serial.printf("\t// Usage Page(%x) - ", val);
          switch (usage_page) {
            case 0x01: if (print_verbose) Serial.print("Generic Desktop"); break;
            case 0x07: if (print_verbose) Serial.print("Keycode"); break;
            case 0x08: if (print_verbose) Serial.print("LEDs"); break;
            case 0x09: if (print_verbose) Serial.print("Button"); break;
            case 0x0C: if (print_verbose) Serial.print("Consumer"); break;
            case 0x0D:
            case 0xFF0D: if (print_verbose) Serial.print("Digitizer"); break;
            default: 
              if (usage_page >= 0xFF00) {if (print_verbose) Serial.print("Vendor Defined");}
              else if (print_verbose) Serial.print("Other ?"); 
              break;
          }
        }
        break;
      case 0x08:  //usage
        if (print_verbose) Serial.printf("\t// Usage(%x) -", val);
        printUsageInfo(usage_page, val);
        if (usage_count < USAGE_LIST_LEN) {
          // Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
          // 0x1 - Pointer - A collection... So lets try ignoring these
          if (val > 0x1f) {
            usage[usage_count++] = val;
          }
          feature.usage = val;
        }
        break;
      case 0x14:  // Logical Minimum (global)
        feature.logical_min = val;
        if (print_verbose) Serial.printf("\t// Logical Minimum(%x)", val);
        break;
      case 0x24:  // Logical Maximum (global)
        feature.logical_max = val;
        if (print_verbose) Serial.printf("\t// Logical maximum(%x)", val);
        break;
      case 0x74:  // Report Size (global)
        feature.report_size = val;
        if (print_verbose) Serial.printf("\t// Report Size(%x)", val);
        break;
      case 0x94:  // Report Count (global)
        feature.report_count = val;
        if (print_verbose) Serial.printf("\t// Report Count(%x)", val);
        break;
      case 0x84:  // Report ID (global)
        if (print_verbose) Serial.printf("\t// Report ID(%x)", val);
        feature.report_id = val;
        break;
      case 0x18:  // Usage Minimum (local)
        usage[0] = val;
        usage_count = 255;
        if (print_verbose) Serial.printf("\t// Usage Minimum(%x) - ", val);
        printUsageInfo(usage_page, val);
        break;
      case 0x28:  // Usage Maximum (local)
        usage[1] = val;
        usage_count = 255;
        if (print_verbose) Serial.printf("\t// Usage Maximum(%x) - ", val);
        printUsageInfo(usage_page, val);
        break;
      case 0xA0:  // Collection
        if (print_verbose) Serial.printf("\t// Collection(%x)", val);
        // discard collection info if not top level, hopefully that's ok?
        if (collection_level == 0) {
          feature.top_usage = ((uint32_t)usage_page << 16) | usage[0];
          if (print_verbose) Serial.printf(" top Usage(%x)", feature.top_usage);
          collection_level++;
        }
        reset_local = true;
        break;
      case 0xC0:  // End Collection
        if (print_verbose) Serial.print("\t// End Collection");
        if (collection_level > 0) collection_level--;
        break;

      case 0x80:  // Input
        if (print_verbose) {
          Serial.printf("\t// Input(%x)\t// (", val);
          print_input_output_feature_bits(val);
        }
        reset_local = true;
        break;
      case 0x90:  // Output
        if (print_verbose) {
          Serial.printf("\t// Output(%x)\t// (", val);
          print_input_output_feature_bits(val);
        }
        reset_local = true;
        break;
      case 0xB0:  // Feature
        if (print_verbose) {
          Serial.printf("\t// Feature(%x)\t// (", val);
          print_input_output_feature_bits(val);
        }

        // We found a feature report, so save data for it
        feature.feature_type = val;        
        if (cnt_feature_reports_ < MAX_FEATURE_REPORTS) {
          feature_reports[cnt_feature_reports_] = feature;
          cnt_feature_reports_++;
        }
        reset_local = true;
        break;

      case 0x34:  // Physical Minimum (global)
        if (print_verbose) Serial.printf("\t// Physical Minimum(%x)", val);
        break;
      case 0x44:  // Physical Maximum (global)
        if (print_verbose) Serial.printf("\t// Physical Maximum(%x)", val);
        break;
      case 0x54:  // Unit Exponent (global)
        if (print_verbose) Serial.printf("\t// Unit Exponent(%x)", val);
        break;
      case 0x64:  // Unit (global)
        if (print_verbose) Serial.printf("\t// Unit(%x)", val);
        break;
    }
    if (reset_local) {
      usage_count = 0;
      usage[0] = 0;
      usage[1] = 0;
    }

    Serial.println();
  }
}

void HIDFeatureReports::print_input_output_feature_bits(uint8_t val) {
  Serial.print((val & 0x01)? "Constant" : "Data");  
  Serial.print((val & 0x02)? ", Variable" : ", Array");  
  Serial.print((val & 0x04)? ", Relative" : ", Absolute");  
  if (val & 0x08) Serial.print(", Wrap");
  if (val & 0x10) Serial.print(", Non Linear");
  if (val & 0x20) Serial.print(", No Preferred");
  if (val & 0x40) Serial.print(", Null State");
  if (val & 0x80) Serial.print(", Volatile");
  if (val & 0x100) Serial.print(", Buffered Bytes");
  Serial.print(")");  
}

void HIDFeatureReports::printUsageInfo(uint8_t usage_page, uint16_t usage) {
  switch (usage_page) {
    case 1:  // Generic Desktop control:
      switch (usage) {
        case 0x02: Serial.print("(Mouse)"); break;
        case 0x04: Serial.print("(Joystick)"); break;
        case 0x06: Serial.print("(Keyboard)"); break;

        case 0x30: Serial.print("(X)"); break;
        case 0x31: Serial.print("(Y)"); break;
        case 0x32: Serial.print("(Z)"); break;
        case 0x33: Serial.print("(Rx)"); break;
        case 0x34: Serial.print("(Ry)"); break;
        case 0x35: Serial.print("(Rz)"); break;
        case 0x36: Serial.print("(Slider)"); break;
        case 0x37: Serial.print("(Dial)"); break;
        case 0x38: Serial.print("(Wheel)"); break;
        case 0x39: Serial.print("(Hat)"); break;
        case 0x3D: Serial.print("(Start)"); break;
        case 0x3E: Serial.print("(Select)"); break;
        case 0x40: Serial.print("(Vx)"); break;
        case 0x41: Serial.print("(Vy)"); break;
        case 0x42: Serial.print("(Vz)"); break;
        case 0x43: Serial.print("(Vbrx)"); break;
        case 0x44: Serial.print("(Vbry)"); break;
        case 0x45: Serial.print("(Vbrz)"); break;
        case 0x46: Serial.print("(Vno)"); break;
        case 0x81: Serial.print("(System Power Down)"); break;
        case 0x82: Serial.print("(System Sleep)"); break;
        case 0x83: Serial.print("(System Wake Up)"); break;
        case 0x90: Serial.print("(D-Up)"); break;
        case 0x91: Serial.print("(D-Dn)"); break;
        case 0x92: Serial.print("(D-Right)"); break;
        case 0x93: Serial.print("(D-Left)"); break;
        default:
          Serial.print("(?)");
          break;
      }
      break;
    case 7: // keyboard/keycode
      switch (usage) {
        case 0xE0: Serial.print("(Left Control)"); break;
        case 0xE1: Serial.print("(Left Shift)"); break;
        case 0xE2: Serial.print("(Left Alt)"); break;
        case 0xE3: Serial.print("(Left)"); break;
        case 0xE4: Serial.print("(Right Control)"); break;
        case 0xE5: Serial.print("(Right Shift)"); break;
        case 0xE6: Serial.print("(Right Alt)"); break;
        case 0xE7: Serial.print("(Right)"); break;
        default:
          Serial.printf("(Keycode %u)", usage);
          break;
      }
      break;
    case 9:  // Button
      Serial.printf(" (BUTTON %d)", usage);
      break;
    case 0xC:  // Consummer page
      switch (usage) {
        case 0x01: Serial.print("(Consumer Controls)"); break;
        case 0x20: Serial.print("(+10)"); break;
        case 0x21: Serial.print("(+100)"); break;
        case 0x22: Serial.print("(AM/PM)"); break;
        case 0x30: Serial.print("(Power)"); break;
        case 0x31: Serial.print("(Reset)"); break;
        case 0x32: Serial.print("(Sleep)"); break;
        case 0x33: Serial.print("(Sleep After)"); break;
        case 0x34: Serial.print("(Sleep Mode)"); break;
        case 0x35: Serial.print("(Illumination)"); break;
        case 0x36: Serial.print("(Function Buttons)"); break;
        case 0x40: Serial.print("(Menu)"); break;
        case 0x41: Serial.print("(Menu  Pick)"); break;
        case 0x42: Serial.print("(Menu Up)"); break;
        case 0x43: Serial.print("(Menu Down)"); break;
        case 0x44: Serial.print("(Menu Left)"); break;
        case 0x45: Serial.print("(Menu Right)"); break;
        case 0x46: Serial.print("(Menu Escape)"); break;
        case 0x47: Serial.print("(Menu Value Increase)"); break;
        case 0x48: Serial.print("(Menu Value Decrease)"); break;
        case 0x60: Serial.print("(Data On Screen)"); break;
        case 0x61: Serial.print("(Closed Caption)"); break;
        case 0x62: Serial.print("(Closed Caption Select)"); break;
        case 0x63: Serial.print("(VCR/TV)"); break;
        case 0x64: Serial.print("(Broadcast Mode)"); break;
        case 0x65: Serial.print("(Snapshot)"); break;
        case 0x66: Serial.print("(Still)"); break;
        case 0x80: Serial.print("(Selection)"); break;
        case 0x81: Serial.print("(Assign Selection)"); break;
        case 0x82: Serial.print("(Mode Step)"); break;
        case 0x83: Serial.print("(Recall Last)"); break;
        case 0x84: Serial.print("(Enter Channel)"); break;
        case 0x85: Serial.print("(Order Movie)"); break;
        case 0x86: Serial.print("(Channel)"); break;
        case 0x87: Serial.print("(Media Selection)"); break;
        case 0x88: Serial.print("(Media Select Computer)"); break;
        case 0x89: Serial.print("(Media Select TV)"); break;
        case 0x8A: Serial.print("(Media Select WWW)"); break;
        case 0x8B: Serial.print("(Media Select DVD)"); break;
        case 0x8C: Serial.print("(Media Select Telephone)"); break;
        case 0x8D: Serial.print("(Media Select Program Guide)"); break;
        case 0x8E: Serial.print("(Media Select Video Phone)"); break;
        case 0x8F: Serial.print("(Media Select Games)"); break;
        case 0x90: Serial.print("(Media Select Messages)"); break;
        case 0x91: Serial.print("(Media Select CD)"); break;
        case 0x92: Serial.print("(Media Select VCR)"); break;
        case 0x93: Serial.print("(Media Select Tuner)"); break;
        case 0x94: Serial.print("(Quit)"); break;
        case 0x95: Serial.print("(Help)"); break;
        case 0x96: Serial.print("(Media Select Tape)"); break;
        case 0x97: Serial.print("(Media Select Cable)"); break;
        case 0x98: Serial.print("(Media Select Satellite)"); break;
        case 0x99: Serial.print("(Media Select Security)"); break;
        case 0x9A: Serial.print("(Media Select Home)"); break;
        case 0x9B: Serial.print("(Media Select Call)"); break;
        case 0x9C: Serial.print("(Channel Increment)"); break;
        case 0x9D: Serial.print("(Channel Decrement)"); break;
        case 0x9E: Serial.print("(Media Select SAP)"); break;
        case 0xA0: Serial.print("(VCR Plus)"); break;
        case 0xA1: Serial.print("(Once)"); break;
        case 0xA2: Serial.print("(Daily)"); break;
        case 0xA3: Serial.print("(Weekly)"); break;
        case 0xA4: Serial.print("(Monthly)"); break;
        case 0xB0: Serial.print("(Play)"); break;
        case 0xB1: Serial.print("(Pause)"); break;
        case 0xB2: Serial.print("(Record)"); break;
        case 0xB3: Serial.print("(Fast Forward)"); break;
        case 0xB4: Serial.print("(Rewind)"); break;
        case 0xB5: Serial.print("(Scan Next Track)"); break;
        case 0xB6: Serial.print("(Scan Previous Track)"); break;
        case 0xB7: Serial.print("(Stop)"); break;
        case 0xB8: Serial.print("(Eject)"); break;
        case 0xB9: Serial.print("(Random Play)"); break;
        case 0xBA: Serial.print("(Select DisC)"); break;
        case 0xBB: Serial.print("(Enter Disc)"); break;
        case 0xBC: Serial.print("(Repeat)"); break;
        case 0xBD: Serial.print("(Tracking)"); break;
        case 0xBE: Serial.print("(Track Normal)"); break;
        case 0xBF: Serial.print("(Slow Tracking)"); break;
        case 0xC0: Serial.print("(Frame Forward)"); break;
        case 0xC1: Serial.print("(Frame Back)"); break;
        case 0xC2: Serial.print("(Mark)"); break;
        case 0xC3: Serial.print("(Clear Mark)"); break;
        case 0xC4: Serial.print("(Repeat From Mark)"); break;
        case 0xC5: Serial.print("(Return To Mark)"); break;
        case 0xC6: Serial.print("(Search Mark Forward)"); break;
        case 0xC7: Serial.print("(Search Mark Backwards)"); break;
        case 0xC8: Serial.print("(Counter Reset)"); break;
        case 0xC9: Serial.print("(Show Counter)"); break;
        case 0xCA: Serial.print("(Tracking Increment)"); break;
        case 0xCB: Serial.print("(Tracking Decrement)"); break;
        case 0xCD: Serial.print("(Pause/Continue)"); break;
        case 0xE0: Serial.print("(Volume)"); break;
        case 0xE1: Serial.print("(Balance)"); break;
        case 0xE2: Serial.print("(Mute)"); break;
        case 0xE3: Serial.print("(Bass)"); break;
        case 0xE4: Serial.print("(Treble)"); break;
        case 0xE5: Serial.print("(Bass Boost)"); break;
        case 0xE6: Serial.print("(Surround Mode)"); break;
        case 0xE7: Serial.print("(Loudness)"); break;
        case 0xE8: Serial.print("(MPX)"); break;
        case 0xE9: Serial.print("(Volume Up)"); break;
        case 0xEA: Serial.print("(Volume Down)"); break;
        case 0xF0: Serial.print("(Speed Select)"); break;
        case 0xF1: Serial.print("(Playback Speed)"); break;
        case 0xF2: Serial.print("(Standard Play)"); break;
        case 0xF3: Serial.print("(Long Play)"); break;
        case 0xF4: Serial.print("(Extended Play)"); break;
        case 0xF5: Serial.print("(Slow)"); break;
        case 0x100: Serial.print("(Fan Enable)"); break;
        case 0x101: Serial.print("(Fan Speed)"); break;
        case 0x102: Serial.print("(Light)"); break;
        case 0x103: Serial.print("(Light Illumination Level)"); break;
        case 0x104: Serial.print("(Climate Control Enable)"); break;
        case 0x105: Serial.print("(Room Temperature)"); break;
        case 0x106: Serial.print("(Security Enable)"); break;
        case 0x107: Serial.print("(Fire Alarm)"); break;
        case 0x108: Serial.print("(Police Alarm)"); break;
        case 0x150: Serial.print("(Balance Right)"); break;
        case 0x151: Serial.print("(Balance Left)"); break;
        case 0x152: Serial.print("(Bass Increment)"); break;
        case 0x153: Serial.print("(Bass Decrement)"); break;
        case 0x154: Serial.print("(Treble Increment)"); break;
        case 0x155: Serial.print("(Treble Decrement)"); break;
        case 0x160: Serial.print("(Speaker System)"); break;
        case 0x161: Serial.print("(Channel Left)"); break;
        case 0x162: Serial.print("(Channel Right)"); break;
        case 0x163: Serial.print("(Channel Center)"); break;
        case 0x164: Serial.print("(Channel Front)"); break;
        case 0x165: Serial.print("(Channel Center Front)"); break;
        case 0x166: Serial.print("(Channel Side)"); break;
        case 0x167: Serial.print("(Channel Surround)"); break;
        case 0x168: Serial.print("(Channel Low Frequency Enhancement)"); break;
        case 0x169: Serial.print("(Channel Top)"); break;
        case 0x16A: Serial.print("(Channel Unknown)"); break;
        case 0x170: Serial.print("(Sub-channel)"); break;
        case 0x171: Serial.print("(Sub-channel Increment)"); break;
        case 0x172: Serial.print("(Sub-channel Decrement)"); break;
        case 0x173: Serial.print("(Alternate Audio Increment)"); break;
        case 0x174: Serial.print("(Alternate Audio Decrement)"); break;
        case 0x180: Serial.print("(Application Launch Buttons)"); break;
        case 0x181: Serial.print("(AL Launch Button Configuration Tool)"); break;
        case 0x182: Serial.print("(AL Programmable Button Configuration)"); break;
        case 0x183: Serial.print("(AL Consumer Control Configuration)"); break;
        case 0x184: Serial.print("(AL Word Processor)"); break;
        case 0x185: Serial.print("(AL Text Editor)"); break;
        case 0x186: Serial.print("(AL Spreadsheet)"); break;
        case 0x187: Serial.print("(AL Graphics Editor)"); break;
        case 0x188: Serial.print("(AL Presentation App)"); break;
        case 0x189: Serial.print("(AL Database App)"); break;
        case 0x18A: Serial.print("(AL Email Reader)"); break;
        case 0x18B: Serial.print("(AL Newsreader)"); break;
        case 0x18C: Serial.print("(AL Voicemail)"); break;
        case 0x18D: Serial.print("(AL Contacts/Address Book)"); break;
        case 0x18E: Serial.print("(AL Calendar/Schedule)"); break;
        case 0x18F: Serial.print("(AL Task/Project Manager)"); break;
        case 0x190: Serial.print("(AL Log/Journal/Timecard)"); break;
        case 0x191: Serial.print("(AL Checkbook/Finance)"); break;
        case 0x192: Serial.print("(AL Calculator)"); break;
        case 0x193: Serial.print("(AL A/V Capture/Playback)"); break;
        case 0x194: Serial.print("(AL Local Machine Browser)"); break;
        case 0x195: Serial.print("(AL LAN/WAN Browser)"); break;
        case 0x196: Serial.print("(AL Internet Browser)"); break;
        case 0x197: Serial.print("(AL Remote Networking/ISP Connect)"); break;
        case 0x198: Serial.print("(AL Network Conference)"); break;
        case 0x199: Serial.print("(AL Network Chat)"); break;
        case 0x19A: Serial.print("(AL Telephony/Dialer)"); break;
        case 0x19B: Serial.print("(AL Logon)"); break;
        case 0x19C: Serial.print("(AL Logoff)"); break;
        case 0x19D: Serial.print("(AL Logon/Logoff)"); break;
        case 0x19E: Serial.print("(AL Terminal Lock/Screensaver)"); break;
        case 0x19F: Serial.print("(AL Control Panel)"); break;
        case 0x1A0: Serial.print("(AL Command Line Processor/Run)"); break;
        case 0x1A1: Serial.print("(AL Process/Task Manager)"); break;
        case 0x1A2: Serial.print("(AL Select Tast/Application)"); break;
        case 0x1A3: Serial.print("(AL Next Task/Application)"); break;
        case 0x1A4: Serial.print("(AL Previous Task/Application)"); break;
        case 0x1A5: Serial.print("(AL Preemptive Halt Task/Application)"); break;
        case 0x200: Serial.print("(Generic GUI Application Controls)"); break;
        case 0x201: Serial.print("(AC New)"); break;
        case 0x202: Serial.print("(AC Open)"); break;
        case 0x203: Serial.print("(AC Close)"); break;
        case 0x204: Serial.print("(AC Exit)"); break;
        case 0x205: Serial.print("(AC Maximize)"); break;
        case 0x206: Serial.print("(AC Minimize)"); break;
        case 0x207: Serial.print("(AC Save)"); break;
        case 0x208: Serial.print("(AC Print)"); break;
        case 0x209: Serial.print("(AC Properties)"); break;
        case 0x21A: Serial.print("(AC Undo)"); break;
        case 0x21B: Serial.print("(AC Copy)"); break;
        case 0x21C: Serial.print("(AC Cut)"); break;
        case 0x21D: Serial.print("(AC Paste)"); break;
        case 0x21E: Serial.print("(AC Select All)"); break;
        case 0x21F: Serial.print("(AC Find)"); break;
        case 0x220: Serial.print("(AC Find and Replace)"); break;
        case 0x221: Serial.print("(AC Search)"); break;
        case 0x222: Serial.print("(AC Go To)"); break;
        case 0x223: Serial.print("(AC Home)"); break;
        case 0x224: Serial.print("(AC Back)"); break;
        case 0x225: Serial.print("(AC Forward)"); break;
        case 0x226: Serial.print("(AC Stop)"); break;
        case 0x227: Serial.print("(AC Refresh)"); break;
        case 0x228: Serial.print("(AC Previous Link)"); break;
        case 0x229: Serial.print("(AC Next Link)"); break;
        case 0x22A: Serial.print("(AC Bookmarks)"); break;
        case 0x22B: Serial.print("(AC History)"); break;
        case 0x22C: Serial.print("(AC Subscriptions)"); break;
        case 0x22D: Serial.print("(AC Zoom In)"); break;
        case 0x22E: Serial.print("(AC Zoom Out)"); break;
        case 0x22F: Serial.print("(AC Zoom)"); break;
        case 0x230: Serial.print("(AC Full Screen View)"); break;
        case 0x231: Serial.print("(AC Normal View)"); break;
        case 0x232: Serial.print("(AC View Toggle)"); break;
        case 0x233: Serial.print("(AC Scroll Up)"); break;
        case 0x234: Serial.print("(AC Scroll Down)"); break;
        case 0x235: Serial.print("(AC Scroll)"); break;
        case 0x236: Serial.print("(AC Pan Left)"); break;
        case 0x237: Serial.print("(AC Pan Right)"); break;
        case 0x238: Serial.print("(AC Pan)"); break;
        case 0x239: Serial.print("(AC New Window)"); break;
        case 0x23A: Serial.print("(AC Tile Horizontally)"); break;
        case 0x23B: Serial.print("(AC Tile Vertically)"); break;
        case 0x23C: Serial.print("(AC Format)"); break;
        default: Serial.print("(?)"); break;
      }
      break;
  }
}


void HIDFeatureReports::printFeatureReportList()
{
  if (cnt_feature_reports_ == 0xff) parseHIDReportDescriptor();

  // print out the list of Feature Reports information.
  Serial.println("\n\tTOP\tUsage\tID\tSize\tCount\tMin\tMax\tType");
  for (uint8_t i = 0; i < cnt_feature_reports_; i++) {
    Serial.printf("%u\t%x\t%x\t%u\t%u\t%u\t%u\t%u\t%u\n", i,
          feature_reports[i].top_usage,
          feature_reports[i].usage,
          feature_reports[i].report_id,
          feature_reports[i].report_size,
          feature_reports[i].report_count,
          feature_reports[i].logical_min,
          feature_reports[i].logical_max,
          feature_reports[i].feature_type
      );
  }
  Serial.println("*** End Report ***");
}
