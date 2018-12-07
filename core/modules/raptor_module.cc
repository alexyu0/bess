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

#include "raptor_module.h"
#include <vector>
#include <unistd.h>
#include <string>
#include <numeric>

/********************************** ENCODER ***********************************/
CommandResponse RaptorEncoder::Init(const bess::pb::RaptorEncoderArg &arg) {
  // Setup Raptor code params
  T_ = arg.t();
  K_min_ = arg.k_min();
  block_id_ = 0;

  return CommandSuccess();
}

void RaptorEncoder::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  // encode each packet as its own source block
  int cnt = batch->cnt();
  for (int i = 0; i < cnt; i++) {
    bess::Packet *pkt = batch->pkts()[i];
    int num_symbols = pkt->data_len()/T_;
    int num_enc_symbols = (int)(num_symbols/0.9); // add overhead rate

    // re-packetize into MTU sized packets
    std::string dumped = pkt->Dump();
    dumped.resize(T_);
    unsigned char source_blk_char[4];
    source_blk_char[0] = (block_id_ >> 24) & 0xFF;
    source_blk_char[1] = (block_id_ >> 16) & 0xFF;
    source_blk_char[2] = (block_id_ >> 8) & 0xFF;
    source_blk_char[3] = block_id_ & 0xFF;
    unsigned char source_blk_size[4];
    source_blk_size[0] = (num_symbols >> 24) & 0xFF;
    source_blk_size[1] = (num_symbols >> 16) & 0xFF;
    source_blk_size[2] = (num_symbols >> 8) & 0xFF;
    source_blk_size[3] = num_symbols & 0xFF;
    unsigned char pkt_data[T_ + 12];
    strncpy((char *)pkt_data, (char *)source_blk_char, 4);
    strncpy((char *)pkt_data + 8, (char *)source_blk_size, 4);
    strncpy((char *)pkt_data + 12, dumped.c_str(), T_);
    int symbol_start = 0;

    // send packet per symbol
    LOG(INFO) << "symbols for encoder: " << num_symbols << ", " << num_enc_symbols;
    for (int j = 0; j < num_symbols + num_enc_symbols; j++) {
      unsigned char s_s_char[4];
      s_s_char[0] = (symbol_start >> 24) & 0xFF;
      s_s_char[1] = (symbol_start >> 16) & 0xFF;
      s_s_char[2] = (symbol_start >> 8) & 0xFF;
      s_s_char[3] = symbol_start & 0xFF;
      strncpy((char *)pkt_data + 8, (char *)s_s_char, 4);
      symbol_start++;

      bess::Packet *pkt_copy = bess::Packet::copy(pkt);
      pkt_copy->set_total_len(T_ + 12);
      pkt_copy->set_data_len(T_ + 12);
      LOG(INFO) << "hello " << pkt_copy->buffer<char *>();
      char *ptr = pkt_copy->buffer<char *>() + pkt_copy->data_off();
      bess::utils::CopyInlined(ptr, pkt_data, T_ + 12, true);
      LOG(INFO) << "hello2 " << pkt_copy->buffer<char *>();
      EmitPacket(ctx, pkt_copy, 0);
    }

    block_id_++;
  }
}

ADD_MODULE(RaptorEncoder, 
  "raptor_encoder", 
  "encoder using Raptor codes")
/************************************ END *************************************/

/********************************** DECODER ***********************************/
CommandResponse RaptorDecoder::Init(const bess::pb::RaptorDecoderArg &arg) {
  // Setup Raptor code params
  T_ = arg.t();
  K_min_ = arg.k_min();
  block_sizes_ = std::map<int, int>();
  symbols_recv_block_ = std::map<int, std::vector<int>>();

  // retreive source block ID and source block size from payload
  
  return CommandSuccess();
}

void RaptorDecoder::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  int cnt = batch->cnt();
  int src_blk_id;
  int symbol_id;
  int src_blk_size;
  bess::Packet *pkt = batch->pkts()[0];
  for (int i = 0; i < cnt; i++) {
    pkt = batch->pkts()[i];
    
    // dissect packet to get source block ID, symbol ID, source block size
    std::string buffer = pkt->buffer<char *>() + pkt->data_off();
    src_blk_id = int((unsigned char)(buffer.c_str()[0]) << 24 |
            (unsigned char)(buffer.c_str()[1]) << 16 |
            (unsigned char)(buffer.c_str()[2]) << 8 |
            (unsigned char)(buffer.c_str()[3]));
    symbol_id = int((unsigned char)(buffer.c_str()[4]) << 24 |
            (unsigned char)(buffer.c_str()[5]) << 16 |
            (unsigned char)(buffer.c_str()[6]) << 8 |
            (unsigned char)(buffer.c_str()[7]));
    src_blk_size = int((unsigned char)(buffer.c_str()[8]) << 24 |
            (unsigned char)(buffer.c_str()[9]) << 16 |
            (unsigned char)(buffer.c_str()[10]) << 8 |
            (unsigned char)(buffer.c_str()[11]));

    block_sizes_[src_blk_id] = src_blk_size;
    symbols_recv_block_[src_blk_id].push_back(symbol_id);
  }

  // loop through map to emit packets that have enough and can now be decoded
  for (auto const &kv : symbols_recv_block_) {
    int block_id = kv.first;
    std::vector<int> sym_ids = kv.second;
    bess::Packet *pkt_copy;
    int target_size = (int)(symbols_recv_block_[block_id].size());
    if ((int)(sym_ids.size()) >= target_size + 3) {
      // can decode, first check if all source symbols are present
      std::map<int, bool> source_sym_ids;
      for (auto const &id : sym_ids) {
        source_sym_ids[id] = true;
      }
      bool source_exists = true;
      for (int j = 0; j < target_size; j++) {
        if (source_sym_ids.find(j) == source_sym_ids.end()) {
          source_exists = false;
        }
      }

      pkt_copy = bess::Packet::copy(pkt);
      if (source_exists) {
        pkt_copy->set_total_len(target_size * 8);
        pkt_copy->set_data_len(target_size * 8);
      } else {
        usleep(17073); // replicate decode time
      }
      
      EmitPacket(ctx, pkt_copy, 0);
    }
  }
}

ADD_MODULE(RaptorDecoder, 
  "raptor_decoder", 
  "decoder for received Raptor encoded packets")
/************************************ END *************************************/

/********************************** GE LOSS ***********************************/
CommandResponse GELoss::Init(const bess::pb::GELossArg &arg) {
  // Setup GE model
  // TODO try different values
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
  ge_state_ = 1;
  
  return CommandSuccess();
}

void GELoss::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  int cnt = batch->cnt();
  for (int i = 0; i < cnt; i++) {
    bess::Packet *pkt = batch->pkts()[i];
    if (rng_.GetReal() <= p_ && ge_state_ == 1) {
      ge_state_ = 0;
    } else if (rng_.GetReal() <= r_ && ge_state_ == 0) {
      ge_state_ = 1;
    }

    double drop_prob = rng_.GetReal();
    if ((ge_state_ == 1 && drop_prob <= (1 - g_s_)) ||
          (ge_state_ == 0 && drop_prob <= (1 - b_s_))) {
      EmitPacket(ctx, pkt, 1);
    } else {
      EmitPacket(ctx, pkt, 0);
    }
  }
}

ADD_MODULE(GELoss, 
  "ge_loss", 
  "loss model based on GE model")
/************************************ END *************************************/
