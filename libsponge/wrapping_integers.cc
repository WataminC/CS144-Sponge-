#include "wrapping_integers.hh"
#include <cmath>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // DUMMY_CODE(n, isn);
    uint64_t max_number = 1ULL << 32;
    n = n % max_number;
    return isn + n;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // DUMMY_CODE(n, isn, checkpoint);
    uint64_t max_number = 1ULL << 32;
    uint64_t times = checkpoint / max_number;

    uint64_t A = static_cast<uint64_t>(WrappingInt32(n - isn).raw_value());
    uint64_t B = checkpoint % max_number;

    // uint64_t ans1 = times * max_number + (n - isn);
    // if (((A <= B && A >= B - max_number/2) || (B == 0 && A <= max_number / 2)) || (A > B && A <= B + max_number / 2)) {
    if ((A <= B && (B <= max_number / 2 || A >= B - max_number/2)) || (A > B && (B >= max_number / 2|| A <= B + max_number / 2))) {
        return times * max_number + A;
    }
    else if (A <= B) {
        return (times + 1) * max_number + A;
    }
    else if (times < 1) { // A > B but B smaller than the max_number
        return times * max_number + A;
    } else {
        return (times - 1) * max_number + A;
    }

    return -1;
}
