#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

// Maybe need to install libpcap-dev library to start lab2
// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    TCPHeader header = seg.header(); 
    Buffer payload = seg.payload();
    bool end_flag = false;

    if(header.syn) {
        isn = header.seqno;
    }

    if (header.fin) {
        end_flag = true;
    }

    _reassembler.push_substring(payload.copy(), unwrap(header.seqno, isn, 0), end_flag);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!isn.raw_value()) 
        return std::nullopt; 
    return wrap(_reassembler.stream_out().bytes_written(), isn);
}

size_t TCPReceiver::window_size() const { 
    return _reassembler.stream_out().remaining_capacity();
}
