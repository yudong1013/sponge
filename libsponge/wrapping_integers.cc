#include "wrapping_integers.hh"

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
    // the least significant 32 bits of n = the wrapped-around 32-bit representation of n
    return WrappingInt32{static_cast<uint32_t>(n) + isn.raw_value()}; // unsigned integers automatically wrap around when they overflow
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
    auto low32bit = static_cast<uint32_t>(n - isn); // exactly the absolute seqno reduced to 32 bits
    // checkpoints with erased lower 32 bits; the first multiple of 2^32 less than checkpoint
    // all three possible ranges where the closest seqno falls into
    auto high32bit_upper = (checkpoint + (1 << 31)) & 0xFFFFFFFF00000000;
    auto high32bit_lower = (checkpoint - (1 << 31)) & 0xFFFFFFFF00000000;
    auto high32bit_same = checkpoint & 0xFFFFFFFF00000000; // NOT TESTED; NO CHECKPTS AROUND 2^16

    auto res_upper = low32bit | high32bit_upper;
    auto res_lower = low32bit | high32bit_lower;
    auto res_same = low32bit | high32bit_same;
    
    auto dist_upper = max(res_upper, checkpoint) - min(res_upper, checkpoint);
    auto dist_lower = max(res_lower, checkpoint) - min(res_lower, checkpoint);
    auto dist_same = max(res_same, checkpoint) - min(res_same, checkpoint);

    if (dist_same <= dist_upper && dist_same <= dist_lower)
        return res_same;
    if (dist_upper <= dist_lower)
        return res_upper;
    return res_lower;
}
