#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 处理截止长度
  if ( is_last_substring ) {
    end_index_ = first_index + data.size();
  }
  uint64_t cur_capacity = output_.writer().available_capacity();
  uint64_t data_pos;   // data 中对应 next_index_ 下标的位置
  uint64_t vector_pos; // 循环中遍历 vector
  // 1. next_index_ 在区间内
  if ( ( first_index <= next_index_ ) && ( first_index + data.size() > next_index_ ) ) {
    // 根据当前剩余空间将信息写入 reassembler 的缓冲区
    debug( "in case 1 get data {}", data );
    data_pos = next_index_ - first_index;
    for ( uint64_t i = data_pos; ( i < data.size() ) && ( i - data_pos < cur_capacity ); i++ ) {
      vector_pos = ( r_m_itr_ + i - data_pos ) % r_capacity_;
      if ( reassembler_memory_flag_[vector_pos] == false ) {
        reassembler_memory_flag_[vector_pos] = true;
        reassembler_memory_[vector_pos] = data[i];
        r_buffer_pending_++;
      }
    } // 2. next_index_ 在区间左外边
  } else if ( ( first_index >= next_index_ ) && ( first_index < next_index_ + cur_capacity ) ) {
    debug( "in case 2 get data {}", data );
    data_pos = 0;
    for ( uint64_t i = 0; ( i < data.size() ) && ( first_index + i < next_index_ + cur_capacity ); i++ ) {
      vector_pos = ( r_m_itr_ + ( first_index - next_index_ ) + i ) % r_capacity_;
      if ( reassembler_memory_flag_[vector_pos] == false ) {
        reassembler_memory_flag_[vector_pos] = true;
        reassembler_memory_[vector_pos] = data[i];
        r_buffer_pending_++;
      }
    }
  }
  // 推数据
  std::string push_data = "";
  while ( reassembler_memory_flag_[r_m_itr_] ) {
    reassembler_memory_flag_[r_m_itr_] = false;
    push_data.push_back( reassembler_memory_[r_m_itr_] );
    r_buffer_pending_ -= 1;
    r_m_itr_ = ( r_m_itr_ + 1 ) % r_capacity_;
    next_index_ += 1;
  }
  debug( "now push data {}", push_data );
  if ( push_data.size() > 0 )
    output_.writer().push( push_data );
  if ( next_index_ >= end_index_ )
    output_.writer().close();
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return r_buffer_pending_;
  debug( "unimplemented count_bytes_pending() called" );
}