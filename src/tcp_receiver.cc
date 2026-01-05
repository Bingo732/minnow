#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;
// TCPSenderMessage 的字段 seqno, SYN, payload, FIN, RST
void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // RST 信号被设置后连接应该被中断
  if ( message.RST ) {
    const_cast<Writer&>(reassembler_.writer()).set_error();
    const_cast<Writer&>(reassembler_.writer()).close();
    return ;
  }
  uint64_t stream_index_;
  // SYN 信号被设置时初始化
  if ( message.SYN && !is_sync_ ) {
    zero_point_ = message.seqno;
    is_sync_ = true;
    stream_index_ = 0;
  }
  if ( !message.SYN ) {
    // 这里不 -1 会有问题，因为建立连接时是不传数据的
    stream_index_ = message.seqno.unwrap(zero_point_, reassembler_.writer().bytes_pushed()) - 1;
  }
  reassembler_.insert(stream_index_, message.payload, message.FIN);
  debug( "unimplemented receive() called" );
}

// TCPReceiverMessage 的字段 ackno, window_size, RST
TCPReceiverMessage TCPReceiver::send() const
{
  debug( "unimplemented send() called" );
  // Your code here.
  uint64_t abs = reassembler_.writer().bytes_pushed() + 1 + (reassembler_.writer().is_closed() ? 1 : 0);
  Wrap32 ackno = zero_point_ + abs;
  uint16_t window_size = min(reassembler_.writer().available_capacity(), static_cast<uint64_t>(UINT16_MAX));
  bool RST = reassembler_.writer().has_error();
  return {
    .ackno = is_sync_ ? ackno : std::optional<Wrap32>{},
    .window_size = window_size,
    .RST = RST
  };
}
