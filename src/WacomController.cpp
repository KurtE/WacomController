#include "WacomController.h"
// lokki *** Device HID1 56a:27 Intuos5 touch M
// My Bamboo: *** Device HID2 56a:d8  CTH-661
// Mikes 056A, ProductID = 0302, Wacom Intuos PT S
// My Intuous S** Device HID1 56a:374 Intuos S
enum {
	PENPARTNER = 0,
	INTUOS5S,
	INTUOS5,
	INTUOS5L,
	BAMBOO_PEN,
	INTUOSHT,
	INTUOSHT2,
	BAMBOO_TOUCH,
	BAMBOO_PT,
	WACOM_24HDT,
	WACOM_27QHDT,
	BAMBOO_PAD,
	H640P,
	INTUOS4100,
	MAX_TYPE,
};

#define WACOM_INTUOS_RES	100
#define WACOM_INTUOS3_RES	200

const WacomController::tablet_info_t WacomController::s_tablets_info[] = {
  {0x056A, 0x27 /*"Wacom Intuos5 touch M"*/, 44704, 27940, 2047, 63, 2, 2, INTUOS5, WACOM_INTUOS3_RES, WACOM_INTUOS3_RES,  16 },
  {0x056A, 0xD8 /*"Wacom Bamboo Comic 2FG"*/, 21648, 13700, 1023, 31, 2, 2, BAMBOO_PT, WACOM_INTUOS_RES, WACOM_INTUOS_RES, 2},
  {0x056A, 0x302 /*"Wacom Intuos PT S*/, 15200, 9500, 1023, 31,   2, 2, INTUOSHT, WACOM_INTUOS_RES, WACOM_INTUOS_RES, 16},
  {0x256c, 0x006d /* "Wacom Bamboo Pen 6x8"*/, 32767*2, 32767, 8192, 10, 0, 0, H640P, WACOM_INTUOS_RES, WACOM_INTUOS_RES },
   // Added for 4100, data to be verified.
  {0x056A, 0x374, 15200, 9500, 1023, 31, 0, 0, INTUOS4100,  WACOM_INTUOS_RES, WACOM_INTUOS_RES, 0}
 };

//static const struct wacom_features wacom_features_HID_ANY_ID =
//	{ "Wacom HID", .type = HID_GENERIC, .oVid = HID_ANY_ID, .oPid = HID_ANY_ID };



void WacomController::init() {
  USBHIDParser::driver_ready_for_hid_collection(this);
}


hidclaim_t WacomController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage) {
  // only claim The mouse like usage, plus the Digitizer and their special one
  if (dev->idVendor != 0x056A && dev->idVendor != 0x256c ) return CLAIM_NO;  //  NOT Wacom

  // only claim from one physical device
  if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
  Serial.printf("WacomController::claim_collection(USBHIDParser %p, %p, %x) - (%x:%x) Claimed\n", driver, dev, topusage, 
        dev->idVendor, dev->idProduct);


  if (tablet_info_index_ == 0xff) {
    for (uint8_t i = 0; i < (sizeof(s_tablets_info)/sizeof(s_tablets_info[0])); i++) {
      if (s_tablets_info[i].idProduct == dev->idProduct) {
        tablet_info_index_ = i;
        Serial.printf(">>> set tablet_info_index_ = %u <<<\n", i);
        break;
      }
    }
  }
  
  if (tablet_info_index_ == 0xff) {
    // Not needed if we claimed the interface, but...
    switch (topusage) {
      case 0xFF0D0001:
      case 0x10002:
      case 0xFF000080:
      case 0xd0001:
      case 0xff000001:
        break;
      default:
        return CLAIM_NO;
      }
  }

  collections_claimed++;
  mydevice = dev;

  // See if maybe we have multiple HIDParsers calling us:
  uint8_t i;
  for (i = 0; i < drivers_cnt_; i++) {
    if (drivers_[i] == driver); // already in list
  }
  if ((i == drivers_cnt_) && (drivers_cnt_ < MAX_DRIVERS)) drivers_[drivers_cnt_++] = driver;
  idProduct_ = dev->idProduct;
  return (tablet_info_index_ == 0xff)? CLAIM_REPORT : CLAIM_INTERFACE;
}


void WacomController::disconnect_collection(Device_t *dev) {
  if (--collections_claimed == 0) {
    mydevice = NULL;
    tablet_info_index_ = 0xff;
    drivers_cnt_ = 0;
  }
}

void WacomController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {
  // TODO: check if absolute coordinates
  if (debugPrint_) Serial.printf("$HIB:%x type:%u min:%d max:%d\n", topusage, type, lgmin, lgmax);
  hid_input_begin_count_++;

  // BUGBUG - see if first input and thry to force a report
  if (firstInput_) {
    firstInput_ = false;
    	//bmRequestType=0x21 Data direction=Host to device, Type=Class, Recipient=Interface
	    //bRequest=0x09 SET_REPORT (HID class)
	    //wValue=0x0302 Report type=Feature, Report ID=0x02
	    //wIndex=0x0000 Interface=0x00
	    //wLength=0x0002
	    //byte=0x02
	    //byte=0x02
    Serial.println("Setup to send different report");
	
	if(s_tablets_info[tablet_info_index_].report_id != 0) {
		static const uint8_t set_report_data[2] = {s_tablets_info[tablet_info_index_].report_id, s_tablets_info[tablet_info_index_].report_value};
    // will assume first one for now...
		drivers_[0]->sendControlPacket(0x21, 9, 0x0302, 0, 2, (void*)set_report_data); 
  }
 }
}

#define WACOM_HID_SP_PAD 0x00040000
#define WACOM_HID_SP_BUTTON 0x00090000
#define WACOM_HID_SP_DIGITIZER 0x000d0000
#define WACOM_HID_SP_DIGITIZERINFO 0x00100000

uint32_t WacomController::wacom_equivalent_usage(uint32_t usage) {
  if ((usage & 0xFFFF0000) == 0xFF0D0000) {
    uint32_t subpage = (usage & 0xFF00) << 8;
    uint32_t subusage = (usage & 0xFF);

    switch (subpage) {
      case WACOM_HID_SP_PAD:
      case 0x00010000:  // Mouse stuff I think
      case WACOM_HID_SP_BUTTON:
      case WACOM_HID_SP_DIGITIZER:
      case WACOM_HID_SP_DIGITIZERINFO:
        if (debugPrint_) Serial.print("########");
        return subpage | subusage;
    }
    if (debugPrint_) Serial.print("********");
    // remove the the leading FF
    return usage & 0x00FFFFFF;
  }
  return usage;
}

bool WacomController::hid_process_in_data(const Transfer_t *transfer) 
{
  uint16_t len = transfer->length;
  const uint8_t *buffer = (const uint8_t *)transfer->buffer;
  if (debugPrint_) {
    const uint8_t *p = buffer;
    Serial.printf("HPID(%u):", len);
  //  if (len > 32) len = 32;
    while (len--) Serial.printf(" %02X", *p++);
    Serial.println();
  }
  // see if we wish to process buffer
  // Only proess if we have a known tablet
  if (tablet_info_index_ == 0xff) return false;
    
  switch (s_tablets_info[tablet_info_index_].type) {
    case INTUOS5:
      return decodeIntuos5(buffer, transfer->length);
      break;
    
    case BAMBOO_PT:
      return decodeBamboo_PT(buffer, transfer->length);
      break;
    case INTUOSHT:
      return decodeIntuosHT(buffer, transfer->length);
      break;
    case H640P:
      return decodeH640P(buffer, transfer->length);
      break;
    case INTUOS4100:
      return decodeIntuos4100(buffer, transfer->length);
      break;
    default:
      return false;  
  }
  
  return false;  
}

void WacomController::hid_input_data(uint32_t usage, int32_t value) {
//  if (debugPrint_) Serial.printf("Digitizer: usage=%X, value=%d(%02x)\n", usage, value, value);

  uint32_t usage_in = usage;
  usage = wacom_equivalent_usage(usage);

  uint32_t usage_page = usage >> 16;
  usage &= 0xFFFF;
  if (debugPrint_) Serial.printf("Digitizer: &usage=%X(in:%X), usage_page=%x, value=%d(%x)\n", usage, usage_in, usage_page, value, value);

  if (usage_page == 0x1) {
    // This is main desktop page:
    switch (usage) {
      case 0x30:
        touch_x_[0] = value;
        break;
      case 0x31:
        touch_y_[0] = value;
        break;
	  case 0x32: // Apple uses this for horizontal scroll
		wheelH = value;
        break;
      case 0x38:
        wheel = value;
        break;
      default:
        Serial.printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
        break;
    }
//    digiAxes[usage & 0xf] = value;

  } else if (usage_page == 0x9) {
    // Button Page.
    if (usage >= 0x1 && usage <= 0x32) {
      if (value == 0) {
        buttons &= ~(1 << (usage - 1));
      } else {
        buttons |= (1 << (usage - 1));
      }
    } else
      Serial.printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
  } else if (usage_page == 0xFF00) {
    // going to ignore index for now and just store them out.
    //if ((usage >= 0x100) && (usage <= 0x10F )) digiAxes[usage & 0xf] = value;
    if (digiAxes_index_ < (sizeof(digiAxes)/ sizeof(digiAxes[0])))  digiAxes[digiAxes_index_++] = value;
    else Serial.printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);

  } else if (usage_page == 0xD) {
			switch (usage) {
			case 0x30: digiAxes[0] = value; break;
			case 0x32: digiAxes[1] = value; break;
			case 0x36: digiAxes[2] = value; break;
			case 0x42: digiAxes[3] = value; break;
			case 0x44: digiAxes[4] = value; break;
			case 0x5A: digiAxes[5] = value; break;
			case 0x5B: digiAxes[6] = value; break;
			case 0x5C: digiAxes[7] = value; break;
			case 0x77: digiAxes[8] = value; break;
      default:
        Serial.printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
        break;
      }
  } else {
    Serial.printf(">>Not Processed usage:%X page:%x value:%x\n", usage, usage_page, value);
  }
}

void WacomController::hid_input_end() {
  if (debugPrint_) Serial.printf("$HIE\n");
  digitizerEvent = true;
  hid_input_begin_count_ = 0;
  digiAxes_index_ = 0;
}

inline uint16_t __get_unaligned_be16(const uint8_t *p)
{
	return p[0] << 8 | p[1];
}

inline uint16_t __get_unaligned_le16(const uint8_t *p)
{
	return p[0] | p[1] << 8;
}

bool WacomController::decodeBamboo_PT(const uint8_t *data, uint16_t len) {
  // only process report 2
  if (data[0] != 2) return false;

  // long format
  //HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  //if (debugPrint_) Serial.printf("BAMBOO PT %p, %u\n", data, len);
  uint8_t offset = 0; 
  if (len == 64) {
    buttons = data[1] & 0xf;
    touch_count_ = 0;
    if (debugPrint_) Serial.printf("BAMBOO PT(64): BTNS: %x", buttons);
    for (uint8_t i = 0; i < 2; i++) {
      bool touch = data[offset + 3] & 0x80;
      if (touch) {
        touch_x_[touch_count_] = ((data[offset + 3] << 8) | (data[offset + 4])) & 0x7ff;
        touch_y_[touch_count_] = ((data[offset + 5] << 8) | (data[offset + 6])) & 0x7ff;
        if (debugPrint_) Serial.printf(" %u:(%u, %u)", i, touch_x_[touch_count_], touch_y_[touch_count_]);
        touch_count_++;
  		  offset += (data[1] & 0x80) ? 8 : 9;
      }
    }
    if (debugPrint_) Serial.println();
    event_type_ = TOUCH;
    digitizerEvent = true;
    return true;
  } else if (len == 9) {
    if (debugPrint_) Serial.print("BAMBOO PT(9): ");
    // the pen
    //HPID(9): 02 F0 1A 20 62 1B 00 00 0
    bool range = (data[1] & 0x80) == 0x80;
    bool prox = (data[1] & 0x40) == 0x40;
    bool rdy = (data[1] & 0x20) == 0x20;
    buttons = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (rdy) {
      buttons = data[1] & 0xf;
      pen_pressure_ = __get_unaligned_le16(&data[6]);
      if (debugPrint_) Serial.printf(" BTNS: %x Pressure: %u", buttons, pen_pressure_);
    }
    if (prox) {
        touch_x_[0] = __get_unaligned_le16(&data[2]);
        touch_y_[0] = __get_unaligned_le16(&data[4]);
        if (debugPrint_) Serial.printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
        touch_count_ = 1;
      digitizerEvent = true;  // only set true if we are close enough...
    }
    if (range) {
        if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
          pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
          if (debugPrint_) Serial.printf(" Distance: %u", pen_distance_);
        }
    }
    event_type_ = PEN;
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}

bool WacomController::decodeIntuosHT(const uint8_t *data, uint16_t len) {
  // only process report 2
  if (data[0] != 2) return false;
  // long format
  //HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  //if (debugPrint_) Serial.printf("BAMBOO PT %p, %u\n", data, len);
  uint8_t offset = 2; 
  bool touch_changed = false;
  if (len == 64) {
    uint8_t count =  data[1] & 0x7;
    touch_count_ = 0;
    if (debugPrint_) Serial.printf("INTOSH PT(64):");
    for (uint8_t i = 0; i < count; i++) {
      if(data[offset] == 0x80) {
        // button message
        buttons = data[offset+1];
        touch_changed = true;
        if (debugPrint_) Serial.printf(" BTNS: %x", buttons);
      } else if ((data[offset] >= 2) && (data[offset] <= 17)) {
        if (data[offset + 1] & 0x80) {
          touch_changed = true;
          touch_x_[touch_count_] = (data[offset + 2] << 4) | (data[offset + 4] >> 4);
          touch_y_[touch_count_] = (data[offset + 3] << 4) | (data[offset + 4] & 0x0f);
          if (debugPrint_) Serial.printf(" %u(%u):(%u, %u)", i, touch_count_, touch_x_[touch_count_], touch_y_[touch_count_]);
          touch_count_++;
        }
      }
      // Else we will ignore the slot
      offset += 8;
    }

    if (debugPrint_) Serial.println();
    if (touch_changed) {
      // is there anything to report?
      event_type_ = TOUCH;
      digitizerEvent = true;
    }	
    return true;
  } else if (len <= 16) {
    if (debugPrint_) Serial.printf("INTOSH PT( %d ): ", len);
    // the pen
    //HPID(9): 02 F0 1A 20 62 1B 00 00 0
    bool range = (data[1] & 0x80) == 0x80;
    bool prox = (data[1] & 0x40) == 0x40;
    bool rdy = (data[1] & 0x20) == 0x20;
    buttons = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (rdy) {
      buttons = data[1] & 0xf;
      pen_pressure_ = __get_unaligned_le16(&data[6]);
      if (debugPrint_) Serial.printf(" BTNS: %x Pressure: %u", buttons, pen_pressure_);
    }
    if (prox) {
        touch_x_[0] = __get_unaligned_le16(&data[2]);
        touch_y_[0] = __get_unaligned_le16(&data[4]);
        if (debugPrint_) Serial.printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
        touch_count_ = 1;
      digitizerEvent = true;  // only set true if we are close enough...
    }
    if (range) {
        if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
          pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
          if (debugPrint_) Serial.printf(" Distance: %u", pen_distance_);
        }
    }
    event_type_ = PEN;
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}

bool WacomController::decodeIntuos5(const uint8_t *data, uint16_t len)
{
  // only process reports 2 and 3
  if ((data[0] != 2) && (data[0] != 3)) return false;

  // long format
  // HPID(64): 02 07 01 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 00 00 00 00 00 00
  // HPID(64): 02 07 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 81 00 00 00 00 00 00 00 00 00 00 00 00 00
  // HPID(64): 02 80 58 82 76 01 52 82 75 01 50 00 00 00 00 11 1F 11 21 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  uint8_t offset = 2; 
  bool touch_changed = false;
  if (len == 64) {
    uint8_t count =  data[1] & 0x7;
    touch_count_ = 0;
    if (debugPrint_) Serial.printf("INTOSH PT(64):");
    for (uint8_t i = 0; i < count; i++) {
      if(data[offset] == 0x80) {
        // button message
        buttons = data[offset+1];
        touch_changed = true;
        if (debugPrint_) Serial.printf(" BTNS: %x", buttons);
      } else if ((data[offset] >= 2) && (data[offset] <= 17)) {
        if (data[offset + 1] & 0x80) {
          touch_changed = true;
          //touch_x_[touch_count_] = ((data[offset + 3] << 8) | (data[offset + 4]));
          //touch_y_[touch_count_] = ((data[offset + 5] << 8) | (data[offset + 6]));
          touch_x_[touch_count_] = (data[offset + 2] << 4) | (data[offset + 4] >> 4);
          touch_y_[touch_count_] = (data[offset + 3] << 4) | (data[offset + 4] & 0x0f);
          if (debugPrint_) Serial.printf(" %u(%u):(%u, %u)", i, touch_count_, touch_x_[touch_count_], touch_y_[touch_count_]);
          touch_count_++;
        }
      }
      // Else we will ignore the slot
      offset += 8;
    }

    if (debugPrint_) Serial.println();
    if (touch_changed) {
      // is there anything to report?
      event_type_ = TOUCH;
      digitizerEvent = true;
    }
    return true;

  } else if (len == 16) {
    if (debugPrint_) Serial.print("INTOSH PT(16): ");
    buttons = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;

    if ((data[1] & 0xfc) == 0xc0) {
      // In Prox: HPID(16): 02 C2 80 A2 38 03 29 51 00 00 00 00 00 00 00 00
      uint32_t serial = ((data[3] & 0x0f) << 28) + (data[4] << 20) + (data[5] << 12) +  
          (data[6] << 4) + (data[7] >> 4);

      uint16_t id = (data[2] << 4) | (data[3] >> 4) |
         ((data[7] & 0x0f) << 16) | ((data[8] & 0xf0) << 8);
      if (debugPrint_) Serial.printf(" Prox: Serial:%x id:%x", serial, id);
      // Do we process this one?

    } else if ((data[1] & 0xfe) == 0x20) {
      // we are in range: HPID(16): 02 20 1E 0F 00 00 00 00 00 FE 00 00 00 00 00 00
      pen_distance_ = s_tablets_info[tablet_info_index_].distance_max;
      if (debugPrint_) Serial.print(" Range");
      event_type_ = PEN;
      digitizerEvent = true;

    } else if ((data[1] & 0xfe) == 0x80) {
      
      //only process buttons and wheel here, not stylus removal
      if (data[0] == 0x03) {
     if (data[2] > 0) side_wheel_ = data[2] - 128; 
     side_wheel_button_ = data[3];
     //touch the buttons
     side_touch_buttons_ = data[5];
     
      //press the buttons
     side_press_buttons_ = data[4];
     if (debugPrint_) {
      Serial.println();
      Serial.printf("Wheel: %u ", side_wheel_);
      Serial.println();
      Serial.printf("WheelButton: %u ", side_wheel_button_);
      Serial.println();
      Serial.printf("Touch:%u Press:%u ", side_touch_buttons_, side_press_buttons_);
      Serial.println();
      
     }
      event_type_ = SIDE_CTRL;
        digitizerEvent = true;
      }
      // out of proximite 
    } else {
      // Maybe should double check tool type.
      uint8_t type = (data[1] >> 1) & 0x0f;
      if (type < 4) {
        // normal pen message.
        touch_x_[0] = __get_unaligned_be16(&data[2]) | ((data[9] >> 1) & 1);
        touch_y_[0] = __get_unaligned_be16(&data[4]) | (data[9] & 1);
        pen_distance_ = data[9] >> 2;
        pen_pressure_  = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
        pen_tilt_x_ = (((data[7] << 1) & 0x7e) | (data[8] >> 7)) - 64;
        pen_tilt_y_ = (data[8] & 0x7f) - 64;
        buttons = data[1] & 0x6;
        if (pen_pressure_ > 10) buttons |= 1;
        if (debugPrint_) Serial.printf("PEN: (%u, %u) BTNS:%x d:%u p:%u tx:%d ty:%d\n", touch_x_[0], touch_y_[0], buttons, pen_distance_, pen_pressure_, pen_tilt_x_,pen_tilt_y_);
        event_type_ = PEN;
        digitizerEvent = true;
      } else {
        if (debugPrint_) Serial.printf("Unprocess tool type: %x", type);
      }
    }
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
}


bool WacomController::decodeH640P(const uint8_t *data, uint16_t len) {
	    if (debugPrint_) Serial.println("H640P(16): ");

  if(data[0] != s_tablets_info[tablet_info_index_].report_id) return false;
  uint8_t offset = 0; 
  if (len == 64) {
    buttons = data[1] & 0xf;
    touch_count_ = 0;
    if (debugPrint_) Serial.printf("BAMBOO PT(64): BTNS: %x", buttons);
    for (uint8_t i = 0; i < 2; i++) {
      bool touch = data[offset + 3] & 0x80;
      if (touch) {
        touch_x_[touch_count_] = ((data[offset + 3] << 8) | (data[offset + 4])) & 0x7ff;
        touch_y_[touch_count_] = ((data[offset + 5] << 8) | (data[offset + 6])) & 0x7ff;
        if (debugPrint_) Serial.printf(" %u:(%u, %u)", i, touch_x_[touch_count_], touch_y_[touch_count_]);
        touch_count_++;
  		  offset += (data[1] & 0x80) ? 8 : 9;
      }
    }
    if (debugPrint_) Serial.println();
    event_type_ = TOUCH;
    digitizerEvent = true;
    return true;
  } else if (len == 16) {
    if (debugPrint_) Serial.print("H640P(16): ");
    // the pen
	//HID(d0002): 0A 00 (c1=Tip Switch, c3=lower Barrel Switch, c5 upper Barrel Switch)
	// FF 7F (x)
	// C9 39 (y)
	// 00 00 (pressure)
	// 00 00 00 00 00 00 00 00 
//    bool range = (data[1] & 0x80) == 0x80;
    bool prox = (data[1] & 0x40) == 0x40;
    bool rdy = (data[1] & 0xC0) == 0xC0;
    buttons = 0;
    touch_count_ = 0;
    pen_pressure_ = 0;
    pen_distance_ = 0;
    if (rdy) {
      buttons = data[1] & 0xf;
      pen_pressure_ = __get_unaligned_le16(&data[6]);
      if (debugPrint_) Serial.printf(" BTNS: %x Pressure: %u", buttons, pen_pressure_);
    }
    if (prox) {
        touch_x_[0] = __get_unaligned_le16(&data[2]);
        touch_y_[0] = __get_unaligned_le16(&data[4]);
        if (debugPrint_) Serial.printf(" (%u, %u)", touch_x_[0], touch_y_[0]);
        touch_count_ = 1;
      digitizerEvent = true;  // only set true if we are close enough...
    }
	/*
    if (range) {
        if (data[8] <= s_tablets_info[tablet_info_index_].distance_max) {
          pen_distance_ = s_tablets_info[tablet_info_index_].distance_max - data[8];
          if (debugPrint_) Serial.printf(" Distance: %u", pen_distance_);
        }
    }
	*/
	pen_distance_ = 0;
    event_type_ = PEN;
    if (debugPrint_) Serial.println();
    return true;
  }
  return false;
	
	
}

bool WacomController::decodeIntuos4100(const uint8_t *data, uint16_t len)
{
  // only process report 0x10(16)
  switch (data[0]) {
  case 16:
    {
      if (debugPrint_) Serial.print("4100 PEN: ");

      // I think most of this is handled in wacom_intuos_general like the pen messages of Intuous5, so will start from there.
      uint8_t type = (data[1] >> 1) & 0x0f;
      if (type < 4) {
        // normal pen message.
        touch_x_[0] = data[2] | (data[3] << 8) | (data[4] << 16);
        touch_y_[0] = data[5] | (data[6] << 8) | (data[7] << 16);
        pen_pressure_  = __get_unaligned_le16(&data[8]);
        pen_distance_ = data[6];
        buttons = data[1] & 0x7;
        if (debugPrint_) Serial.printf("PEN: (%u, %u) BTNS:%x d:%u p:%u", touch_x_[0], touch_y_[0], buttons, pen_distance_, pen_pressure_);
        event_type_ = PEN;
        digitizerEvent = true;
      } else {
        if (debugPrint_) Serial.printf("Unprocess tool type: %x", type);
      }
      if (debugPrint_) Serial.println();
      return true;
    }
  case 17:
    {
      buttons = data[1] & 0xf;
      touch_count_ = 0;
      if (debugPrint_) Serial.printf("4100 Touch: BTNS: %x\n", buttons);
      event_type_ = TOUCH;
      digitizerEvent = true;
      return true;
    }
  }
  return false;
}


void WacomController::digitizerDataClear() {
  digitizerEvent = false;
  buttons = 0;
  touch_x_[0] = 0;
  touch_y_[0] = 0;
  touch_count_ = 0;
  event_type_ = NONE;
  pen_pressure_ = 0;  
  pen_distance_ = 0;
  pen_tilt_x_ = 0;
  pen_tilt_y_ = 0;
  wheel = 0;
  wheelH = 0;
}
