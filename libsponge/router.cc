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

unsigned long long construct_binary_with_ones(int n) {
    // Construct a number with 'n' ones at the start
    unsigned long long num = ((1ULL << n) - 1) << (32 - n); // 2^n - 1 creates 'n' ones
    return num;
}

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

    // Your code here.
    _routerTable.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.
    if (dgram.header().ttl <= 1) {
        // lifetime end
        return ;
    }

    dgram.header().ttl -= 1;

    int index = -1;
    std::size_t maxPrefix = 0;
    for (std::size_t i = 0; i < _routerTable.size(); ++i) {
        const auto &t = _routerTable[i];
        uint32_t route_prefix  = std::get<0>(t);
        uint8_t prefix_length = std::get<1>(t);
        uint32_t bitMask = static_cast<uint32_t>(construct_binary_with_ones(prefix_length));

        if ((route_prefix & bitMask) != (dgram.header().dst & bitMask)) {
            continue;
        }

        if (prefix_length >= maxPrefix) {
            index = i;
            maxPrefix = prefix_length;
        }
    }

    if (index == -1) {
        return ;
    }

    const auto &t = _routerTable[index];
    Address next_hop = Address::from_ipv4_numeric(dgram.header().dst);
    if (std::get<2>(t).has_value()) {
        next_hop = std::get<2>(t).value();
        // Address next_hop = std::get<2>(t).value();
        // _interfaces[std::get<3>(t)].send_datagram(dgram, next_hop);
    }

    _interfaces[std::get<3>(t)].send_datagram(dgram, next_hop);
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
