// Copyright (c) 2014-2016, The Regents of the University of California.
// Copyright (c) 2016-2017, Nefeli Networks, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the names of the copyright holders nor the names of their
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef BESS_MODULES_RAPTORMODULE_H_
#define BESS_MODULES_RAPTORMODULE_H_

#include "../module.h"
#include "../pb/module_msg.pb.h"
#include <vector>
#include <map>

class RaptorEncoder final : public Module {

 public:
  CommandResponse Init(const bess::pb::RaptorEncoderArg &arg);
  static const size_t kMaxTemplateSize = 1512;

  static const gate_idx_t kNumIGates = 1;
  static const gate_idx_t kNumOGates = 1;

 private:
  int T_; // symbol size
  int K_min_; // minimum source block size
  int block_id_; // current block ID
};

class RaptorDecoder final : public Module {

 public:
  CommandResponse Init(const bess::pb::RaptorDecoderArg &arg);

  static const gate_idx_t kNumIGates = 1;
  static const gate_idx_t kNumOGates = 1;

 private:
  int T_; // symbol size
  int K_min_; // minimum source block size
  std::map<int, std::vector<int>> block_sizes_; // size of blocks being decoded
  std::map<int, std::vector<int>> symbols_recv_block_; // block to vector of symbols received
};

class GELoss final : public Module {

 public:
  CommandResponse Init(const bess::pb::GELossArg &arg);

  static const gate_idx_t kNumIGates = 1;
  static const gate_idx_t kNumOGates = 2; // 1 is sink

 private:
  Random rng_;  // Random number generator
  double p_;
  double r_;
  double g_s_;
  double b_s_;
  int ge_state_; // 0 = bad, 1 = good
};

#endif  // BESS_MODULES_RAPTORMODULE_H_
