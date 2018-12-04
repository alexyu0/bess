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


void TCPSender::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  //gate_idx_t igate = ctx->current_igate;


  int cnt = batch->cnt();

  LOG(INFO) << "batch count = " << cnt;

  for (int i = 0; i < cnt; i++) {
    bess::Packet *pkt = batch->pkts()[i];    
    

    /* Start copy from url_filter to get TCP header */
    Ethernet *eth = pkt->head_data<Ethernet *>();
    Ipv4 *ip = reinterpret_cast<Ipv4 *>(eth + 1);

    /*if (ip->protocol != Ipv4::Proto::kTcp) {
      EmitPacket(ctx, pkt, 1); // Drop
      continue;
    }*/

    int ip_bytes = ip->header_length << 2;
    Tcp *tcp = reinterpret_cast<Tcp *>(reinterpret_cast<uint8_t *>(ip) + ip_bytes);
    /* End copy from url_filter to get TCP header */



    LOG(INFO) << "Pkt seq num = " << tcp->seq_num << ", pkt len = " << ip->length << ", last seq = " << last_seen_seq;

    bess::utils::be16_t pkt_len = ip->length;
    bess::utils::be32_t next_expected_seq = last_seen_seq + bess::utils::be32_t(pkt_len.raw_value());

    // not keep-alive and (non-zero length or syn or fin) and next expected > curr seq
    if (!(tcp->flags & Tcp::Flag::kAck && pkt_len > bess::utils::be16_t(0))) {
      if (pkt_len > bess::utils::be16_t(0) || tcp->flags & Tcp::Flag::kSyn || tcp->flags & Tcp::Flag::kFin) {
        if (next_expected_seq > tcp->seq_num) {
          LOG(INFO) << "Dropping pkt with seq num " << tcp->seq_num;
          EmitPacket(ctx, pkt, 0); // Drop
          continue;
        }
      }
    }

    last_seen_seq = tcp->seq_num;

    LOG(INFO) << "Emitting pkt with seq num " << tcp->seq_num;
    EmitPacket(ctx, pkt, 0);
  }
}

ADD_MODULE(TCPSender, "tcp_sender", "sends pseudo TCP messages")
