#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

#include <iostream>

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // debug( "TCPSender::sequence_numbers_in_flight() get {}", outstanding_data_.size() );
  return current_buffering_;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  // debug( "unimplemented consecutive_retransmissions() called" );
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t avail_window = current_window_ == 0 ? 1 : current_window_;
  debug( "TCPSender::push called with current_buffering_:{}, avail_window:{}", current_buffering_, avail_window );
  while( current_buffering_ < avail_window ) {
    TCPSenderMessage current_message = make_empty_message();
    // 处理错误
    if ( current_message.RST ) {
      if ( !writer().is_closed() ) writer().close();
      // 发出终止数据包
      debug("error occur");
      before_transmit(current_message);
      transmit(current_message);
      return;
    }
    // 建立连接 即当前为首次发出 SYN 信号
    if ( !is_SYN_sent_ ) {
      is_SYN_sent_ = true;
      current_message.SYN = true;
    }
    // 装载 payload
    uint64_t current_payload_length = avail_window - current_buffering_ - current_message.sequence_length();
    current_payload_length = min(TCPConfig::MAX_PAYLOAD_SIZE, current_payload_length);
    debug("before read have {} in buffer", reader().bytes_buffered());
    read(reader(), current_payload_length, current_message.payload);
    debug("current_payload_length:{}, avail_window:{}, current_buffering_:{}, current_message.sequence_length: {}",
           current_payload_length,    avail_window,    current_buffering_,    current_message.sequence_length());
    debug("still have {} in buffer", reader().bytes_buffered());
    // 写入 FIN 信号
    if ( reader().is_finished() && !is_FIN_sent_ && avail_window - current_buffering_ - current_message.sequence_length() > 0) {
      current_message.FIN = true;
      is_FIN_sent_ = true;
    }
    // 不发出数据包
    if ( current_message.sequence_length() == 0 ) {
      debug("not sent");
      return;
    }
    // 发出数据包
    debug("now have payload length {} with value {}, SYN: {}, FIN: {}", current_message.sequence_length(), current_message.payload, current_message.SYN, current_message.FIN);
    before_transmit(current_message);
    transmit(current_message);
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  debug( "empty_message() called" );
  return {
    .seqno = Wrap32::wrap(abs_seqno_, isn_),
    .SYN = false,
    .payload = "",
    .FIN = false,
    .RST = reader().has_error()
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  debug( "--TCPSender::receive window_size {}, and current size:{} ", msg.window_size, current_window_ );
  // 处理错误
  if ( msg.RST ) {
    if ( !writer().has_error() ) writer().set_error();
    if ( !writer().is_closed() ) writer().close();
    return;
  }
  // 修改标志位信息 current_window_ consecutive_retransmissions_ curren_RTO_ms_ current_buffering_
  if ( !outstanding_data_.empty() && msg.ackno.has_value() ) {
    uint64_t receive_absno = msg.ackno.value().unwrap(isn_, abs_seqno_);
    // 越界 ack 处理
    if ( receive_absno > outstanding_data_.back().seqno.unwrap(isn_, abs_seqno_) + outstanding_data_.back().sequence_length() ) {
      debug("impossible ack with val:{}, and window_size:{}", receive_absno, current_window_);
      return;
    }
    // 非越界 ack 处理 1. 队列中没有能够被确认的 2. 队列中有能够被确认的
    if ( receive_absno < outstanding_data_.front().seqno.unwrap(isn_, abs_seqno_) + outstanding_data_.front().sequence_length() ) {
      consecutive_retransmissions_ += 1;
    } else {
      consecutive_retransmissions_ = 0;
      curren_RTO_ms_ = initial_RTO_ms_;
      timer_ms_ = 0;
    }
    // 处理确认收到的数据
    while( !outstanding_data_.empty() && msg.ackno.has_value() && receive_absno >= outstanding_data_.front().seqno.unwrap(isn_, abs_seqno_) + outstanding_data_.front().sequence_length() ) {
      // debug("- - - pop length {} with value {}, SYN: {}, FIN: {}", outstanding_data_.front().sequence_length(), outstanding_data_.front().payload, outstanding_data_.front().SYN, outstanding_data_.front().FIN);
      current_buffering_ -= outstanding_data_.front().sequence_length();
      outstanding_data_.pop_front();
    }
  }
  // 这玩意得最后再改
  current_window_ = msg.window_size;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "tick called curren_RTO_ms_:{}, timer_ms_:{}, ms_since_last_tick:{}",curren_RTO_ms_, timer_ms_, ms_since_last_tick );
  if ( outstanding_data_.empty() ) {
    timer_ms_ = 0;
    return ;
  }
  timer_ms_ += ms_since_last_tick;
  if ( timer_ms_ >= curren_RTO_ms_ ) {
    // debug("time elapse in tick(), veriables are outstanding_data_: {}, curren_RTO_ms_: {}, timer_ms_: {}", outstanding_data_.size(), curren_RTO_ms_, timer_ms_);
    // 超时后重传 outstanding_data_ 中第一个数据
    // curren_RTO_ms_ 翻倍 - 仅发生在窗口不为 0 时
    debug(" timeout with RTO:{}", curren_RTO_ms_);
    if(current_window_ != 0) curren_RTO_ms_ *= 2;
    consecutive_retransmissions_ += 1;
    transmit(outstanding_data_.front());
    timer_ms_ = 0;
  }
}

void TCPSender::before_transmit( const TCPSenderMessage& message )
{
  abs_seqno_ += message.sequence_length();
  outstanding_data_.push_back(message);
  current_buffering_ += message.sequence_length();
}