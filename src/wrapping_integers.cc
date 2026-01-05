#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // debug("original data {}", n);
  return Wrap32 { static_cast<uint32_t>( n ) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // debug("get val {}, checkpoint {}", raw_value_, checkpoint);
  const uint64_t n_2_32 = 1ULL << 32; // 4294967296
  const uint32_t abs_seqno = raw_value_ - zero_point.raw_value_;
  const uint64_t offset = checkpoint & ~( n_2_32 - 1 ); // 高 32 位
  const uint64_t remain = checkpoint & ( n_2_32 - 1 );  // 低 32 位
  // debug("have abs_seqno: {}, offset: {}, reamin: {}", abs_seqno, offset, remain);
  if ( remain > abs_seqno + ( n_2_32 / 2 ) ) {
    // debug("1-get result {}", abs_seqno + offset + n_2_32);
    return abs_seqno + offset + n_2_32;
  } else if ( ( abs_seqno > remain + ( n_2_32 / 2 ) ) && ( abs_seqno + offset >= n_2_32 ) ) {
    // debug("2-get result {}", abs_seqno + offset - n_2_32);
    return abs_seqno + offset - n_2_32;
  }
  // debug("3-get result {}", abs_seqno + offset);
  return abs_seqno + offset;
}
