// Copyright (c) 2017, The Regents of the University of California.
// Copyright (c) 2017, Vivian Fang.
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

#include "random_split.h"
#include <string>
#include <time.h>

static inline bool is_valid_gate(gate_idx_t gate) {
  return (gate < MAX_GATES || gate == DROP_GATE);
}

const Commands RandomSplitGE::cmds = {
    {"set_droprate", "RandomSplitGECommandSet_p_Arg",
     MODULE_CMD_FUNC(&RandomSplitGE::CommandSet_p), Command::THREAD_UNSAFE},
     {"set_droprate", "RandomSplitGECommandSet_r_Arg",
     MODULE_CMD_FUNC(&RandomSplitGE::CommandSet_r), Command::THREAD_UNSAFE},
     {"set_droprate", "RandomSplitGECommandSet_gs_Arg",
     MODULE_CMD_FUNC(&RandomSplitGE::CommandSet_gs), Command::THREAD_UNSAFE},
     {"set_droprate", "RandomSplitGECommandSet_bs_Arg",
     MODULE_CMD_FUNC(&RandomSplitGE::CommandSet_bs), Command::THREAD_UNSAFE},
    {"set_gates", "RandomSplitGECommandSetGatesArg",
     MODULE_CMD_FUNC(&RandomSplitGE::CommandSetGates), Command::THREAD_UNSAFE}};

CommandResponse RandomSplitGE::Init(const bess::pb::RandomSplitGEArg &arg) {
  double drop_rate = arg.drop_rate();
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

  if (arg.gates_size() > MAX_SPLIT_GATES) {
    return CommandFailure(EINVAL, "no more than %d gates", MAX_SPLIT_GATES);
  }

  for (int i = 0; i < arg.gates_size(); i++) {
    if (!is_valid_gate(arg.gates(i))) {
      return CommandFailure(EINVAL, "Invalid gate %d", gates_[i]);
    }
  }

  ngates_ = arg.gates_size();
  for (int i = 0; i < ngates_; i++) {
    gates_[i] = arg.gates(i);
  }

  return CommandSuccess();
}

CommandResponse RandomSplitGE::CommandSet_p(
    const bess::pb::RandomSplitGECommandSet_p_Arg &arg) {
  double p = arg.p();
  if (p < 0 || p > 1) {
    return CommandFailure(EINVAL, "p needs to be between [0, 1]");
  }
  p_ = p;

  return CommandSuccess();
}
CommandResponse RandomSplitGE::CommandSet_r(
    const bess::pb::RandomSplitGECommandSet_r_Arg &arg) {
  double r = arg.r();
  if (r < 0 || r > 1) {
    return CommandFailure(EINVAL, "r needs to be between [0, 1]");
  }
  r_ = r;

  return CommandSuccess();
}
CommandResponse RandomSplitGE::CommandSet_gs(
    const bess::pb::RandomSplitGECommandSet_gs_Arg &arg) {
  double g_s = arg.gs();
  if (g_s < 0 || g_s > 1) {
    return CommandFailure(EINVAL, "p needs to be between [0, 1]");
  }
  g_s_ = g_s;

  return CommandSuccess();
}
CommandResponse RandomSplitGE::CommandSet_bs(
    const bess::pb::RandomSplitGECommandSet_bs_Arg &arg) {
  double b_s = arg.bs();
  if (b_s < 0 || b_s > 1) {
    return CommandFailure(EINVAL, "p needs to be between [0, 1]");
  }
  b_s_ = b_s;

  return CommandSuccess();
}

CommandResponse RandomSplitGE::CommandSetGates(
    const bess::pb::RandomSplitGECommandSetGatesArg &arg) {
  if (arg.gates_size() > MAX_SPLIT_GATES) {
    return CommandFailure(EINVAL, "no more than %d gates", MAX_SPLIT_GATES);
  }

  for (int i = 0; i < arg.gates_size(); i++) {
    if (!is_valid_gate(arg.gates(i))) {
      return CommandFailure(EINVAL, "Invalid gate %d", gates_[i]);
    }
  }

  ngates_ = arg.gates_size();
  for (int i = 0; i < ngates_; i++) {
    gates_[i] = arg.gates(i);
  }

  return CommandSuccess();
}

void RandomSplitGE::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  if (ngates_ <= 0) {
    bess::Packet::Free(batch);
    return;
  }

  int cnt = batch->cnt();
  for (int i = 0; i < cnt; i++) {
    bess::Packet *pkt = batch->pkts()[i];
    if (rng_.GetReal() <= p_ && state_ == 1) {
      state_ = 0;
    } else if (rng_.GetReal() <= r_ && state_ == 0) {
      state_ = 1;
    }

    double drop_prob = rng_.GetReal();
    if ((state_ == 1 && drop_prob <= (1 - g_s_)) ||
        (state_ == 0 && drop_prob <= (1 - b_s_))) {
      DropPacket(ctx, pkt);
    } else {
      EmitPacket(ctx, pkt, gates_[rng_.GetRange(ngates_)]);
    }
  }
}

ADD_MODULE(RandomSplitGE, "random_split_ge", "randomly splits/drops packets")
