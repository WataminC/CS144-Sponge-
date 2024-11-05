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
        _segments_out.push(std::move(seg));
    }
}

bool TCPConnection::check_segment_in_window(const TCPSegment &seg) {
    // Check does sender and receiver has initialized.
    if (!_isn_receiver.has_value() || !_receiver.ackno().has_value()) {
        return true;
    }

    uint64_t seg_start = unwrap(seg.header().seqno, _isn_receiver.value(), 0);
    uint64_t seg_end = seg_start + (seg.length_in_sequence_space() == 0 ? 0 : seg.length_in_sequence_space() - 1);
    uint64_t window_start = unwrap(_receiver.ackno().value(), _isn_receiver.value(), 0);
    uint64_t window_end = window_start + _receiver.window_size();

    // If segment less than the window
    if (seg_end < window_start) {
        return false;
    }

    // If segment bigger than the window
    if (seg_start >= window_end) {
        return false;
    }
    
    return true;
}

void TCPConnection::send_rst(void) {
    // Send an empty segment to ensure at least one segment in the queue
    _sender.send_empty_segment();

    // Get the segment from the the queue and set RST flag
    TCPSegment seg = std::move(_sender.segments_out().front());
    seg.header().rst = true;
    while (!_segments_out.empty()) {
        _segments_out.pop();
    }
    _segments_out.push(std::move(seg));

    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    _close = true;
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

    // Record the receiver's isn
    if (seg.header().syn) {
        _isn_receiver = seg.header().seqno;
    }

    // If receive fin before send fin -> passive close
    if (seg.header().fin && 
    (this->state().state_summary(_sender) != TCPSenderStateSummary::FIN_SENT && 
    this->state().state_summary(_sender) != TCPSenderStateSummary::FIN_ACKED)) {
        _linger_after_streams_finish = false;
    }

    // Send the segment to the TCPReceiver, if no errors happen
    if (this->check_segment_in_window(seg)) {
        _receiver.segment_received(seg);
    }

    // If ACK flag is set, transfer ackno and window_size from segment to TCPSender
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // At least one segment is sent to reply when the incoming segment occupied any seqeence numbers
    if (seg.length_in_sequence_space()) {
        // How to make sure it?
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
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
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    this->send_segment();
    return size; 
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _tick += ms_since_last_tick;
    // Fill the window every times call the tick
    if (!_sender.stream_in().buffer_empty() || _sender.stream_in().eof()) {
        _sender.fill_window();
        this->send_segment();
    }
    
    // Send the time to the sender
    _sender.tick(ms_since_last_tick);

    // Send the segments to TCPConnection if retransmission happened
    if (!_sender.segments_out().empty()) {
        this->send_segment();
    }

    // If consecutive retransmission times is more than the maximum accepted tiems, abort the connection
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        this->send_rst();
    }

    // Prereq #1: The inbound stream has been fully assembled and has ended
    if (_receiver.unassembled_bytes() || !_receiver.stream_out().eof()) {
        return ;
    }

    // Prereq #2: THe outbound stream has been ended by the local application and fully sent
    if (!_sender.stream_in().eof()) {
        return ;
    }

    // Prereq #3: The outbound stream hass been fully acknowledged by the remote peer
    if (_sender.bytes_in_flight()) {
        return ;
    }

    // Every time prerequisites #1 throught #3 are satisfied, it should wait at least 10 times the initial retransmission timeout
    if (time_since_last_segment_received() >= 10*_cfg.rt_timeout) {
        _linger_after_streams_finish = false;
        _close = true;
    }

    // Any point where prerequisites #1 through #3 are satisfied, the connection is "done" if _linger_after_streams_finish is false
    if (_linger_after_streams_finish) {
        return ;
    }

    _close = true;
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
            this->send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
