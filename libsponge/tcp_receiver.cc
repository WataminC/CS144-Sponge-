#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

// Maybe need to install libpcap-dev library to start lab2

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header(); 
    Buffer payload = seg.payload();

    if(header.syn) {
        isn = header.seqno;
    }

    bool end_flag = false;
    if (header.fin && isn.has_value()) {
        end_flag = true;
    }


    if (isn.has_value()) {
        size_t str_index = 0;
        if (!header.syn) {
            str_index = unwrap(header.seqno, isn.value(), 0) - 1;
        }
        _reassembler.push_substring(payload.copy(), str_index, end_flag);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!isn.has_value()) {
        return std::nullopt;
    }
    bool end = _reassembler.stream_out().input_ended();
    return wrap(_reassembler.stream_out().bytes_written() + 1 + end, isn.value());
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
