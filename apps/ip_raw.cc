#include "socket.hh"

using namespace std;

int main()
{
  // construct an Internet or user datagram here, and send using a RawSocket
  string d;
  d += 0b0100'0101;    // version and IHL
  d += string( 7, 0 ); // rest of first two lines

  d += 64;             // TTL
  d += 5;              // proto
  d += string( 6, 0 ); // rest of next two lines

  d += 192; // destination address
  d += 168u;
  d += 31u;
  d += 165u;

  d += "Hello my steamdeck!";

  RawSocket {}.send( d, Address { "1" } ); // the "1" needs to be chosen a little carefully -- the datagram needs to leave our computer on the right interface

  return 0;
}
