#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void RetransmissionTimer::start(const unsigned int timeout) {
    rto = std::chrono::milliseconds(timeout);
    running.store(true);
}

bool RetransmissionTimer::is_running() {
    return running.load();
}

void RetransmissionTimer::stop() {
    running.store(false);
}

void RetransmissionTimer::time_passed(const size_t ms_since_last_tick, std::function<void()> callback) {
    if (!is_running)
        return ;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds(ms_since_last_tick);

    if (rto > elapsed) {
        rto -= elapsed;
        return ;
    }

    callback();
}

void TCPSender::retransmit() {
    // Send the earliest segment but don't remove it from the queue
    TCPSegment segment = _retransmission_queue.front(); 
    _segments_out.push(segment);

    // If window is empty, don't retransmit.
    if (!_window_size) {
        return ;
    }

    // Count consecutive retransmission 
    if (_last_retrans_seqno.has_value() && _last_retrans_seqno.value() == segment.header().seqno) {
        _consecutive_retran++; 
    } else {
        _consecutive_retran = 0;
        _last_retrans_seqno = segment.header().seqno;
    }

    // Double RTO
    _rto *= 2;

    // Reset and start timer
    timer.start(_rto);
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity), _outstanding_bytes(0), _consecutive_retran(0), _rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _outstanding_bytes;
}

void TCPSender::fill_window() {
    while (!_stream.buffer_empty()) {
        if (!_window_size) {
            break;
        }

        TCPHeader header;
        size_t length = 0;

        if (!_stream.bytes_read()) {
            header.syn = true;
            length++;
        }

        if (_stream.eof()) {
            header.fin = true;
            length++;
        }

        size_t bytes2read = _window_size - length;
        if (bytes2read >= TCPConfig::MAX_PAYLOAD_SIZE) {
            bytes2read = TCPConfig::MAX_PAYLOAD_SIZE;
        }

        Buffer payload(std::move(_stream.read(bytes2read)));

        TCPSegment segment;
        segment.header() = header;
        segment.payload() = payload;
        
        _segments_out.push(segment);
        _retransmission_queue.push(segment);

        if (!timer.is_running() && segment.length_in_sequence_space() - length > 0) {
            timer.start(_rto);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _next_seqno = unwrap(ackno, _isn, 0);
    _window_size = window_size;

    // Remove the segment from the retransmission queue

    // Stop timer
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer.time_passed(ms_since_last_tick, [this]() { this->retransmit(); });
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retran;
}

void TCPSender::send_empty_segment() {
    TCPHeader header;

    if (!_stream.bytes_read()) {
        header.syn = true;
    }

    if (_stream.eof()) {
        header.fin = true;
    }

    TCPSegment segment;
    segment.header() = header;
    
    _segments_out.push(segment);
    _retransmission_queue.push(segment);
}
