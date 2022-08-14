#ifndef __HIDFEATUREREPORT_H_
#define __HIDFEATUREREPORT_H_
#include <Arduino.h>
#include <USBHost_t36.h>

class HIDFeatureReports {
public:
  HIDFeatureReports() : phidp_(nullptr) {}
  void setHIDParser(USBHIDParser *phidp) {phidp_ = phidp;}
  void parseHIDReportDescriptor(bool print_verbose = false); // parse the descriptor and return count of Features found
  uint8_t countFeatures() {
    if (cnt_feature_reports_ == 0xff) parseHIDReportDescriptor();
    return cnt_feature_reports_;
  }

  void printFeatureReportList();

  typedef struct {
    uint32_t top_usage;   // the top usage this was under
    uint32_t usage;       // usage
    uint32_t report_id;   // The id probably only 8 bits
    uint32_t report_size;
    uint32_t report_count;
    uint32_t logical_min;
    uint32_t logical_max;
    uint8_t  feature_type;  
  } FEATURE_REPORTS_t;
  
  enum {MAX_FEATURE_REPORTS=32};
  FEATURE_REPORTS_t feature_reports[MAX_FEATURE_REPORTS];

private:

  void printUsageInfo(uint8_t usage_page, uint16_t usage);
  void print_input_output_feature_bits(uint8_t val);

  USBHIDParser *phidp_;

  uint32_t fixed_usage_;
  uint32_t usage_ = 0;
  // Track changing fields. 
   const static int MAX_CHANGE_TRACKED = 512;
  uint32_t usages_[MAX_CHANGE_TRACKED];
  int32_t values_[MAX_CHANGE_TRACKED];
  int count_usages_ = 0;
  int index_usages_ = 0;
  // experiment to see if we can receive data from Feature reports.
  uint8_t cnt_feature_reports_ = 0xff;
};
#endif // __HIDFEATUREREPORT_H_
