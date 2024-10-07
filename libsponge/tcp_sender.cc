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

void RetransmissionTimer::time_passed(const size_t ms_since_last_tick, std::function<void> callback) {
    if (!is_running)
        return ;

    std::chrono::milliseconds elapsed = std::chrono::milliseconds(ms_since_last_tick);

    if (rto > elapsed) {
        rto -= elapsed;
        return ;
    }

    callback();
}


//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity), outstanding_bytes(0), consecutive_retran(0), rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return outstanding_bytes;
}

void TCPSender::fill_window() {}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { DUMMY_CODE(ackno, window_size); }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const {
    return consecutive_retran;
}

void TCPSender::send_empty_segment() {}
