#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::send_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = std::move(_sender.segments_out().front());
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _sender.segments_out().pop();
        _segments_out.push(seg);
    }
}

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _tick - _rece_tick;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // Record tick to get the segment received time
    _rece_tick = _tick;

	/* 
     *  Check does RST flag has been set or not.
	 *	If the RST flag is set, set both the inbound and outbound streams to the error state
	 *  and kills the connection permanently.
     */
    if (seg.header().rst) {
        // set inbound and outbound streams to the error state
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        
        // How to kill the connection permanently?
        _close = true;
    }

    // If receive fin before send fin -> passive close
    // How to decide does the sender have sent fin or not?
    if (seg.header().fin && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // Send the segment to the TCPReceiver, if no errors happen
    _receiver.segment_received(seg);

    // If ACK flag is set, transfer ackno and window_size from segment to TCPSender
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // At least one segment is sent to reply when the incoming segment occupied any seqeence numbers
    if (seg.length_in_sequence_space()) {
        // How to make sure it?
        size_t size = _sender.segments_out().size();
        _sender.fill_window();
        if (size == _sender.segments_out().size()) {
            _sender.send_empty_segment();
        }
    }

    // Responding to a "keep-alive" segment
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
    this->send_segment();
}

bool TCPConnection::active() const {
    return !_close;
}

size_t TCPConnection::write(const string &data) {
    return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _tick += ms_since_last_tick;
    if (time_since_last_segment_received() >= 10*_cfg.rt_timeout) {
        _linger_after_streams_finish = false;
        _close = true;
    }

    // Send the time to the sender
    _sender.tick(ms_since_last_tick);

    // Send the segments to TCPConnection if retransmission happened
    if (!_sender.segments_out().empty()) {
        this->send_segment();
    }

    // If consecutive retransmission times is more than the maximum accepted tiems, abort the connection
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _close = true;
    }

    // Any point where prerequisites #1 through #3 are satisfied, the connection is "done" if _linger_after_streams_finish is false
    if (_linger_after_streams_finish) {
        return ;
    }

    // Prereq #1: The inbound stream has been fully assembled and has ended
    if (_receiver.unassembled_bytes() && _receiver.stream_out().eof()) {
        return ;
    }

    // Prereq #2: THe outbound stream has been ended by the local application and fully sent
    if (!_sender.stream_in().eof()) {
        return ;
    }

    // Prereq #3: The outbound stream hass been fully acknowledged by the remote peer
    if (!_sender.bytes_in_flight()) {
        return ;
    }

    _close = false;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    this->send_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    this->send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
