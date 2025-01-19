#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // Translate the ip address to the mac address
    // If the destination mac address is known
    auto macAddr = _cacheMap.find(next_hop_ip);
    if (macAddr != _cacheMap.end()) {
        // Set ethernet header and send the datagram
        EthernetFrame ef;
        ef.header().src = _ethernet_address;
        ef.header().dst = macAddr->second.first;
        ef.header().type = EthernetHeader::TYPE_IPv4;
        ef.payload() = dgram.serialize();
        _frames_out.push(ef);

        return ;
    }

    // If the destination mac address is unknown
    // If the arp request haven't been sent in last five seconds
    auto arpRq = _arpRequests.find(next_hop_ip);
    if (arpRq == _arpRequests.end()) {
        // Broadcast an ARP request

        // Set ARP message
        ARPMessage am;
        am.opcode = ARPMessage::OPCODE_REQUEST; 
        am.sender_ethernet_address = this->_ethernet_address;
        am.sender_ip_address = this->_ip_address.ipv4_numeric();
        am.target_ip_address = next_hop_ip;

        // Set ethernet header and send the datagram
        EthernetFrame ef;
        ef.header().src = this->_ethernet_address;
        ef.header().dst = ETHERNET_BROADCAST;
        ef.header().type = EthernetHeader::TYPE_ARP;
        ef.payload() = am.serialize();
        _frames_out.push(ef);

        // Set arp request lifetime
        _arpRequests[next_hop_ip] = 5 * 1000;
    }

    // Queue the IP datagram
    _datagramQueue[next_hop_ip].emplace(dgram);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return std::nullopt;
    }

    // If the inbound frame is IPv4
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        std::string payloadStr = frame.payload().concatenate();
        InternetDatagram ipDatagram;
        ParseResult result = ipDatagram.parse(std::move(payloadStr));

        // If parse successfully
        if (result != ParseResult::NoError) {
            return std::nullopt;
        }

        return ipDatagram;
    }

    // If the inbound frame is ARP
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        std::string payloadStr = frame.payload().concatenate();
        ARPMessage arpContent;
        ParseResult result = arpContent.parse(std::move(payloadStr));

        // If parse successfully
        if (result != ParseResult::NoError) {
            return std::nullopt;
        }

        _cacheMap[arpContent.sender_ip_address] = std::make_pair(arpContent.sender_ethernet_address, 30*1000);

        // Send the queuing ip datagram if needed
        auto it = _datagramQueue.find(arpContent.sender_ip_address);
        if (it != _datagramQueue.end()) {
            while (!it->second.empty()) {
                auto &data = it->second.front();
                // Set ethernet header and send the datagram
                EthernetFrame ef;
                ef.header().src = _ethernet_address;
                ef.header().dst = arpContent.sender_ethernet_address;
                ef.header().type = EthernetHeader::TYPE_IPv4;
                ef.payload() = data.serialize();
                _frames_out.push(ef);

                it->second.pop();
            }
        }

        // If the arp message is a arp request, send the corresponding respond
        if (arpContent.opcode == ARPMessage::OPCODE_REQUEST && arpContent.target_ip_address == _ip_address.ipv4_numeric()) {
            arpContent.opcode = ARPMessage::OPCODE_REPLY;

            arpContent.target_ethernet_address = arpContent.sender_ethernet_address;
            arpContent.target_ip_address = arpContent.sender_ip_address;
            arpContent.sender_ethernet_address = _ethernet_address;
            arpContent.sender_ip_address = _ip_address.ipv4_numeric();

            // Set ethernet header and send the datagram
            EthernetFrame ef;
            ef.header().src = this->_ethernet_address;
            ef.header().dst = arpContent.target_ethernet_address;
            ef.header().type = EthernetHeader::TYPE_ARP;
            ef.payload() = arpContent.serialize();
            _frames_out.push(ef);
        }
    }

    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _arpRequests.begin(); it != _arpRequests.end(); )  {
        if (it->second > ms_since_last_tick) {
            it->second -= ms_since_last_tick;
            it++;
        } else {
            it = _arpRequests.erase(it);
        }
    }

    for (auto it = _cacheMap.begin(); it != _cacheMap.end(); ) {
        if (it->second.second > ms_since_last_tick) {
            it->second.second -= ms_since_last_tick;
            it++;
        } else {
            it = _cacheMap.erase(it);
        }
    }
}
