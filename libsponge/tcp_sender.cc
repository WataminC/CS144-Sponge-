#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void RetransmissionTimer::start(const size_t timeout) {
    rto = timeout;
    running = true;
}

bool RetransmissionTimer::is_running() {
    return running;
}

void RetransmissionTimer::stop() {
    running = false;
}

void RetransmissionTimer::time_passed(const size_t ms_since_last_tick, std::function<void()> callback) {
    if (!this->is_running())
        return ;

    if (rto > ms_since_last_tick) {
        rto -= ms_since_last_tick;
        return ;
    }

    callback();
}

void TCPSender::retransmit() {
    // Send the earliest segment but don't remove it from the queue
    TCPSegment segment = _retransmission_queue.front(); 
    _segments_out.push(segment);

    // If window is empty, don't retransmit.
    if (_window_size) {
        // Count consecutive retransmission 
        if (_last_retrans_seqno.has_value() && _last_retrans_seqno.value() == segment.header().seqno) {
            _consecutive_retran++; 
        } else {
            // Reset consecutive retransmission times
            _consecutive_retran = 1;
            _last_retrans_seqno = segment.header().seqno;
        }

        // Double RTO when it is not the "probe" state
        if (!_window_probe) {
            _rto *= 2;
        }
    }
    
    // Reset and start timer
    timer.start(_rto);
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity), _outstanding_bytes(0), _consecutive_retran(0), _rto(retx_timeout)
    , _window_size{1}, _ack(_isn), _window_probe(false) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _next_seqno - unwrap(_ack, _isn, 0);
}

void TCPSender::fill_window() {
    while (_window_size - bytes_in_flight() && _state != FIN_SENT) {
        TCPSegment segment;
        segment.header().seqno = next_seqno();

        size_t bytes2read = _window_size - bytes_in_flight();
        if (!_stream.bytes_read() && _state == INIT) {
            segment.header().syn = true;
            bytes2read--;
            _state = SYN_SENT;
        }

        if (bytes2read >= TCPConfig::MAX_PAYLOAD_SIZE) {
            bytes2read = TCPConfig::MAX_PAYLOAD_SIZE;
        }

        segment.payload() = std::move(_stream.read(bytes2read));

        // If stream is end and there is some places that can hold the fin 
        if (_stream.eof() && _state == SYN_SENT && segment.length_in_sequence_space() + 1 <= _window_size - bytes_in_flight()) {
            segment.header().fin = true;
            _state = FIN_SENT;
        }

        // Break when there are no bytes to read to avoid infinite loop
        if (!segment.length_in_sequence_space())
            break;

        _next_seqno += segment.length_in_sequence_space();
        
        // Send the segment and record it in the retransmission queue
        _segments_out.push(segment);
        _retransmission_queue.push(segment);

        // If the segment do occupy some sequence space(Not Empty!), start timer!
        if (!timer.is_running() && segment.length_in_sequence_space() > 0) {
            timer.start(_rto);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    _window_probe = false;

    // If window size is 0, treat it like it is 1 but enter "probe" state
    if (!_window_size) {
        _window_size = 1;
        _window_probe = true;
    }

    // Do nothing when ackno bigger the last sent bytes seqno or ackno smaller than the original _ack
    if (unwrap(ackno, _isn, 0) > next_seqno_absolute() || unwrap(ackno, _isn, 0) <= unwrap(_ack, _isn, 0)) {
        return ;
    }
    _ack = ackno;

    // Reset rto
    _rto = _initial_retransmission_timeout;

    TCPSegment element;
    // Remove the segment from the retransmission queue
    while (_retransmission_queue.size()) {
        element = _retransmission_queue.front();
        if (unwrap(element.header().seqno, _isn, 0) + element.length_in_sequence_space() > unwrap(ackno, _isn, 0))
            break;
        _retransmission_queue.pop();
    }

    // If the retransmission queue is not empty, start the timer
    if (_retransmission_queue.size()) {
        timer.start(_rto);
    } else {
        timer.stop();
    }

    //  Reset consecutive retransmission times
    _consecutive_retran = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer.time_passed(ms_since_last_tick, [this]() { this->retransmit(); });
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retran;
}

// Send the segment has zero length in sequence space and with the sequence number set correctly
void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    
    _segments_out.push(segment);
}
