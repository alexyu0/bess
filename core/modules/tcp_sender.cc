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

#include "tcp_sender.h"

#include "../utils/ether.h"
#include "../utils/format.h"
#include "../utils/http_parser.h"
#include "../utils/ip.h"

using bess::utils::Ethernet;
using bess::utils::Ipv4;
using bess::utils::Tcp;
using bess::utils::be16_t;

bess::utils::be32_t last_seen_seq = bess::utils::be32_t(0);


CommandResponse TCPSender::Init(const bess::pb::EmptyArg &) {
  /*task_id_t tid;

  tid = RegisterTask(nullptr);
  if (tid == INVALID_TASK_ID)
    return CommandFailure(ENOMEM, "Context creation failed");
  */
  return CommandSuccess();
}

/*struct task_result TCPSender::RunTask(Context *, bess::PacketBatch *, void *) {
  return {.block = false, .packets = 0, .bits = 0};
}
*/

void TCPSender::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  //gate_idx_t igate = ctx->current_igate;

  //LOG(INFO) << "hello from process batch";

  int cnt = batch->cnt();

  LOG(INFO) << "batch count = " << cnt;

  for (int i = 0; i < cnt; i++) {
    bess::Packet *pkt = batch->pkts()[i];    
    
    LOG(INFO) << "Got pkt from batch...";

    /* Start copy from url_filter to get TCP header */
    Ethernet *eth = pkt->head_data<Ethernet *>();
    Ipv4 *ip = reinterpret_cast<Ipv4 *>(eth + 1);

    if (ip->protocol != Ipv4::Proto::kTcp) {
      EmitPacket(ctx, pkt, 1); // Drop
      continue;
    }

    int ip_bytes = ip->header_length << 2;
    Tcp *tcp = reinterpret_cast<Tcp *>(reinterpret_cast<uint8_t *>(ip) + ip_bytes);
    /* End copy from url_filter to get TCP header */

    LOG(INFO) << "Processing pkt as TCP...";



    if (tcp->seq_num > last_seen_seq) {
      last_seen_seq = tcp->seq_num;
    } else {
      LOG(INFO) << "Dropping pkt with seq num " << tcp->seq_num;
      EmitPacket(ctx, pkt, 1); // Drop
      continue;
    }


    EmitPacket(ctx, pkt, 0);
  }

}

ADD_MODULE(TCPSender, "tcp_sender", "sends pseudo TCP messages")
