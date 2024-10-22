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
    if (!this->is_running())
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
    if (_window_size) {
        // Count consecutive retransmission 
        if (_last_retrans_seqno.has_value() && _last_retrans_seqno.value() == segment.header().seqno) {
            _consecutive_retran++; 
        } else {
            _consecutive_retran = 1;
            _last_retrans_seqno = segment.header().seqno;
        }

        // Double RTO
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
        TCPHeader header;
        header.seqno = next_seqno();

        size_t bytes2read = _window_size - bytes_in_flight();
        if (!_stream.bytes_read() && _state == INIT) {
            header.syn = true;
            _next_seqno++; 
            bytes2read--;
            _state = SYN_SENT;
        }

        if (bytes2read >= TCPConfig::MAX_PAYLOAD_SIZE) {
            bytes2read = TCPConfig::MAX_PAYLOAD_SIZE;
        }

        Buffer payload(std::move(_stream.read(bytes2read)));

        if (_stream.eof() && _state == SYN_SENT && (header.syn == true) + payload.size() + 1 <= _window_size - bytes_in_flight()) {
            header.fin = true;
            _next_seqno++; 
            _state = FIN_SENT;
        }

        _next_seqno += payload.size();

        TCPSegment segment;
        segment.header() = header;
        segment.payload() = payload;

        if (!segment.length_in_sequence_space())
            break;
        
        _segments_out.push(segment);
        _retransmission_queue.push(segment);

        if (!timer.is_running() && segment.length_in_sequence_space() > 0) {
            timer.start(_rto);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;

    if (!_window_size) {
        _window_size = 1;
        _window_probe = true;
    } else {
        _window_probe = false;
    }

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
    TCPHeader header;

    // Set the sequence number
    header.seqno = next_seqno();

    TCPSegment segment;
    segment.header() = header;
    
    _segments_out.push(segment);
    _retransmission_queue.push(segment);
}
