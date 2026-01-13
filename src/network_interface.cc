#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;


//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
  debug( "unimplemented send_datagram called" );
  const uint32_t ip_addr = next_hop.ipv4_numeric();
  if ( availbale_in_table( ip_addr ) ) {
    debug( "send ipv4" );
    EthernetFrame eth_frame = wrap( dgram, ip_addr );
    transmit( eth_frame );
  } else {
    debug("no mapping exist");
    internet_datagram_queue_.insert( { ip_addr, { dgram, 0 } } );
    if ( availbale_in_requesting_queue( ip_addr ) ) {
      debug( "arp still no reply" );
      if( mapping_requesting_[ ip_addr ] > 5000 ) {
        debug( "resend arp request" );
        mapping_requesting_[ ip_addr ] = 0;
        ARPMessage arp_msg = ARP_generate( ip_addr );
        EthernetFrame eth_frame = wrap( arp_msg, ETHERNET_BROADCAST );
        transmit( eth_frame );
      }
    } else {
      debug( "should send request arp" );
      ARPMessage arp_msg = ARP_generate( ip_addr );
      debug( "now have arp request: {}", arp_msg.to_string() );
      EthernetFrame eth_frame = wrap( arp_msg, ETHERNET_BROADCAST );
      debug( "now have eth_frame: header - {}", eth_frame.header.to_string() );
      transmit( eth_frame );
      mapping_requesting_.insert( { ip_addr, 0 } );
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) return;
  // 收到发给自己的信息
  debug( "receive frame with header {}", frame.header.to_string() );

  Parser parser( frame.payload );
  // 验证出错
  if ( parser.has_error() ) return;

  // 收到 ARP 信息
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    arp_msg.parse( parser );
    debug( "receive arp msg: {}", arp_msg.to_string() );
    // 存储 IP - MAC 映射
    mapping_table_[ arp_msg.sender_ip_address ] = { arp_msg.sender_ethernet_address, 0 };
    // 清除排队信息
    mapping_requesting_.erase( arp_msg.sender_ip_address );
    // 发送等待信息
    debug( "have {} msg to transmit", internet_datagram_queue_.count( arp_msg.sender_ip_address ) );
    auto it = internet_datagram_queue_.find( arp_msg.sender_ip_address );
    while( it != internet_datagram_queue_.end() ) {
      send_datagram( it->second.first, Address::from_ipv4_numeric( arp_msg.sender_ip_address ) );
      ++it;
    }
    internet_datagram_queue_.erase( arp_msg.sender_ip_address );
    // 广播判定
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      debug( "should send reply ARP" );

      ARPMessage arp_reply = ARP_generate( arp_msg );
      debug( "now have arp reply: {}", arp_reply.to_string() );
      EthernetFrame eth_frame = wrap( arp_reply, arp_msg.sender_ethernet_address );
      debug( "now have eth_frame: header - {}", eth_frame.header.to_string() );
      transmit( eth_frame );

    }
  // 收到 IPV4 信息 
  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ipv4_msg;
    ipv4_msg.parse( parser );
    debug( "receive ipv4 msg header: {}", ipv4_msg.header.to_string() );
    datagrams_received_.push(ipv4_msg);
  // 收到其他信息
  } else {
    return;
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "unimplemented tick({}) called", ms_since_last_tick );
  for ( auto it = mapping_table_.begin(); it != mapping_table_.end(); ) {
    auto &current_pair = *it;
    current_pair.second.second += ms_since_last_tick;
    if ( current_pair.second.second >= 30000 ) {
      debug( "RTT for ip - mac mapping {}", Address::from_ipv4_numeric( current_pair.first ).to_string() );
      it = mapping_table_.erase( it );
    } else {
      ++it;
    }
  }
  for ( auto &pair : mapping_requesting_ ) pair.second += ms_since_last_tick;
  for ( auto it = internet_datagram_queue_.begin(); it != internet_datagram_queue_.end(); ) {
    auto &current_pair = *it;
    current_pair.second.second += ms_since_last_tick;
    if ( current_pair.second.second >= 5000 ) {
      debug( "RTT for pending msg header: {}", current_pair.second.first.header.to_string() );
      it = internet_datagram_queue_.erase( it );
    } else {
      ++it;
    }
  }
}

ARPMessage NetworkInterface::ARP_generate( const uint32_t& next_hop_ip ) const {
  ARPMessage arp_msg;
  arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
  arp_msg.sender_ethernet_address = ethernet_address_;
  arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
  arp_msg.target_ethernet_address = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  arp_msg.target_ip_address = next_hop_ip;
  return arp_msg;
}

ARPMessage NetworkInterface::ARP_generate( const ARPMessage &arp_msg ) const {
  ARPMessage arp_reply;
  arp_reply.opcode = ARPMessage::OPCODE_REPLY;
  arp_reply.sender_ethernet_address = ethernet_address_;
  arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
  arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
  arp_reply.target_ip_address = arp_msg.sender_ip_address;
  return arp_reply;
}

EthernetFrame NetworkInterface::wrap( const ARPMessage &arp_msg, const EthernetAddress &next_hop_mac  ) const {
  EthernetFrame eth_frame;
  eth_frame.header.src = ethernet_address_;
  eth_frame.header.dst = next_hop_mac;
  eth_frame.header.type = EthernetHeader::TYPE_ARP;
  Serializer ser;
  arp_msg.serialize(ser);
  eth_frame.payload = ser.finish();
  return eth_frame;
}

EthernetFrame NetworkInterface::wrap( const InternetDatagram &ipv4_msg, const uint32_t& next_hop ) const {
  EthernetFrame eth_frame;
  eth_frame.header.src = ethernet_address_;
  auto it = mapping_table_.find(next_hop);
  eth_frame.header.dst = it->second.first;
  eth_frame.header.type = EthernetHeader::TYPE_IPv4;
  Serializer ser;
  ipv4_msg.serialize(ser);
  eth_frame.payload = ser.finish();
  return eth_frame;
}

