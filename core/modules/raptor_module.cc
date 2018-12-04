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

#include "raptor.h"
#include "raptor_module.h"
#include <vector>
#include <unistd.h>

CommandResponse RaptorAndLoss::Init(const bess::pb::RaptorAndLossArg &arg) {
  // Setup GE model
  double p = arg.p();
  double r = arg.r();
  double g_s = arg.g_s();
  double b_s = arg.b_s();
  if (p < 0 || p > 1) {
    return CommandFailure(EINVAL, "p needs to be between [0, 1]");
  } else if (r < 0 || r > 1) {
    return CommandFailure(EINVAL, "r needs to be between [0, 1]");
  } else if (g_s < 0 || g_s > 1) {
    return CommandFailure(EINVAL, "g_s needs to be between [0, 1]");
  } else if (b_s < 0 || b_s > 1) {
    return CommandFailure(EINVAL, "b_s needs to be between [0, 1]");
  }
  p_ = p;
  r_ = r;
  g_s_ = g_s;
  b_s_ = b_s;
  ge_ge_state_ = 1;

  // Setup Raptor code params
  T_ = arg.T();
  K_min_ = arg.k_min(); x                
  // GoInt source_block_size = 13; // TODO set
  // GoInt alignment_size = 1; // TODO set

  // // Create Raptor codec/encoder
  // new_raptor_codec_ = NewRaptorCodec(source_blocks, alignment_size);
  
  return CommandSuccess();
}

void RaptorAndLoss::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  // setup raptor stuff
  // treat each packet as 1 source block, 1 16 byte symbol per packet FOR NOW
  // TODO try different values
  // TODO add encoding and decoding latencies
  int cnt = batch->cnt();
  for (int i = 0; i < cnt; i++) {
    bess::Packet *pkt = batch->pkts()[i];
    int num_symbols = pkt->data_len()/T_;
    int num_packets = num_symbols + (int)(num_symbols/0.9); // add overhead rate
    usleep(17236);

    int received_packets = 0;
    for (int j = 0; j < num_packets; j++) {
      if (rng_.GetReal() <= p_ && ge_state_ == 1) {
        ge_state_ = 0;
      } else if (rng_.GetReal() <= r_ && ge_state_ == 0) {
        ge_state_ = 1;
      }

      double drop_prob = rng_.GetReal();
      if (!((ge_state_ == 1 && drop_prob <= (1 - g_s_)) ||
            (ge_state_ == 0 && drop_prob <= (1 - b_s_)))) {
        received_packets++;
      }
    }

    while (received_packets < num_symbols + 3) {
      if (rng_.GetReal() <= p_ && ge_state_ == 1) {
        ge_state_ = 0;
      } else if (rng_.GetReal() <= r_ && ge_state_ == 0) {
        ge_state_ = 1;
      }

      double drop_prob = rng_.GetReal();
      if (!((ge_state_ == 1 && drop_prob <= (1 - g_s_)) ||
            (ge_state_ == 0 && drop_prob <= (1 - b_s_)))) {
        received_packets++;
      }
    }
    usleep(17073);

    EmitPacket(pkt, 0);
  }
}

ADD_MODULE(RaptorAndLoss, 
  "raptor_encode", 
  "encoder using Raptor codes")
// ADD_MODULE(RaptorAndLoss, 
//   "raptor_encode", 
//   "encoder using Raptor codes")
// ADD_MODULE(RaptorDecoder, 
//   "raptor_decode", 
//   "decoder for received Raptor encoded packets")
// ADD_MODULE(RepairForwarder, 
//   "repair_forwarder", 
//   "forwards repair requests from decoder to encoder")

