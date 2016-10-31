#ifndef __MEASURE_H__
#define __MEASURE_H__

#include "../module.h"
#include "../utils/histogram.h"

class Measure : public Module {
 public:
  Measure()
      : Module(),
        hist_(),
        start_time_(),
        warmup_(),
        pkt_cnt_(),
        bytes_cnt_(),
        total_latency_() {}

  virtual struct snobj *Init(struct snobj *arg);
  virtual pb_error_t Init(const bess::protobuf::MeasureArg &arg);

  virtual void ProcessBatch(struct pkt_batch *batch);

  struct snobj *CommandGetSummary(struct snobj *arg);
  bess::protobuf::MeasureCommandGetSummaryResponse CommandGetSummary(
      const bess::protobuf::MeasureCommandGetSummaryArg &arg);

  static const gate_idx_t kNumIGates = 1;
  static const gate_idx_t kNumOGates = 1;

  static const Commands<Module> cmds;

 private:
  struct histogram hist_ = {0};

  uint64_t start_time_;
  int warmup_; /* second */

  uint64_t pkt_cnt_;
  uint64_t bytes_cnt_;
  uint64_t total_latency_;
};

#endif