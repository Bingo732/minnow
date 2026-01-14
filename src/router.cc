#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  // 直接插入路由表
  std::optional<uint32_t> next_hop_optional;
  next_hop_optional = next_hop.has_value() ? std::optional<uint32_t>(next_hop->ipv4_numeric()) : std::nullopt;
  routing_table_.insert( { make_pair( route_prefix, prefix_length ) ,
                           make_pair( next_hop_optional, interface_num ) } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  debug( "unimplemented route() called" );
  for ( const auto& interface : interfaces_ ) {
    queue<InternetDatagram> &datagrams = interface->datagrams_received();
    // debug( "interface {} have {} datagrams", interface->name(), datagrams.size() );
    while ( !datagrams.empty() ) {
      InternetDatagram datagram = datagrams.front();
      datagrams.pop();
      if ( datagram.header.ttl <= 1 ) continue;
      datagram.header.ttl -= 1;
      datagram.header.compute_checksum();
      uint32_t next_hop;
      size_t interface_num;
      if ( longest_prefix_matching( datagram.header.dst, next_hop, interface_num ) ) {
        debug( "find correct route !" );
        interfaces_[ interface_num ]->send_datagram( datagram, Address::from_ipv4_numeric( next_hop ) );
      }
    }
  }
}

bool Router::route_matching( uint32_t ipv4_addr, uint32_t route_prefix, uint8_t prefix_length ) const {
  debug( "match dst {} with route : prefix {} and length {}", Address::from_ipv4_numeric( ipv4_addr ).ip(), Address::from_ipv4_numeric( route_prefix ).ip(), prefix_length );
  // 位运算后比较值，运算 32 位的情况要单独处理
  if ( prefix_length == 0 ) {
    debug( "32 bits" );
    return true;
  } else {
    return ( ipv4_addr >> ( 32 - prefix_length ) ) == ( route_prefix >> ( 32 - prefix_length ) );
  }
}

bool Router::longest_prefix_matching( uint32_t ipv4_addr, uint32_t &next_hop, size_t &interface_num ) const {
  for ( const auto& it : routing_table_ ) {
    if ( route_matching( ipv4_addr, it.first.first, it.first.second ) ) {
      next_hop = it.second.first.has_value() ? *it.second.first : ipv4_addr;
      interface_num = it.second.second;
      return true;
    }
  }
  return false;
}