#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <optional>

// 加的
#include <map>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

  // 加的

  // 单条路由的匹配
  bool route_matching( uint32_t, uint32_t, uint8_t ) const;
  // 最长前缀路由匹配
  bool longest_prefix_matching( uint32_t, uint32_t&, size_t& ) const;
private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};

  // 加的

  // 路由排序, 优先考虑最大长度
  struct RouteCompare {
    bool operator() ( const std::pair<uint32_t, uint8_t> &l, const std::pair<uint32_t, uint8_t> &r ) const {
      return l.second > r.second ? l.second > r.second : l.first > r.first;
    }
  };
  // 存储的路由表
  std::map<std::pair<uint32_t, uint8_t>, std::pair<std::optional<uint32_t>, size_t>, RouteCompare> routing_table_;
};
