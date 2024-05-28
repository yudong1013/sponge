#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // save the route for later use
    _routing_table.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // match the destination IP address of the datagram to a route in the routing table
    auto ip = dgram.header().dst;
    auto best_match = _routing_table.begin();
    // find the longest prefix match
    for (auto it = _routing_table.begin(); it != _routing_table.end(); it = std::next(it)) {
        // in this compiler, right shift 32 bits is not equal to right shift 0 bits, so we need to handle them separately
        // XOR makes equal bits 0
        if (it -> prefix_length == 0 ||  (it -> route_prefix ^ ip) >> (32 - it -> prefix_length) == 0) { // match the prefix
            if (it -> prefix_length > best_match -> prefix_length) { // longest prefix among all matched
                best_match = it;
            }
        }
    }
    // update the TTL and send the datagram to the next hop or the destination
    if (best_match != _routing_table.end() && dgram.header().ttl > 1) {
        --dgram.header().ttl; // decrement the TTL of IP datagram to prevent infinite routing loops
        auto &next_interface = interface(best_match -> interface_num); // find the correct interface to send the datagram
        // send to the next hop if it exists, otherwise send to the destination directly
        if (best_match -> next_hop.has_value()) {
            next_interface.send_datagram(dgram, best_match -> next_hop.value());
        } else {
            next_interface.send_datagram(dgram, Address::from_ipv4_numeric(ip));
        }
    }
    // discard the datagram if no route matches or TTL is 0
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}