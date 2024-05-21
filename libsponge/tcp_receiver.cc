#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// Find the correct first_index of the segment and write the segment to the byte stream
void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();
    
    if (!_isn.has_value()) { // no SYN received before
        if (!header.syn) return; // if the current segment is not SYN, discard it
        _isn = header.seqno; // if the current segment is SYN, the seqno is isn
    }
    
    uint64_t checkpoint = _reassembler.stream_out().bytes_written(); // index of the last reassmebled byte
    uint64_t abs_seqno = unwrap(header.seqno, _isn.value(), checkpoint);
    uint64_t stream_index = abs_seqno - 1 + (header.syn ? 1: 0); // the same only if the current segment is SYN, otherwise increase by 1 for the SYN processed before
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin); // FIN signals eof
}

// Return the expected ackno and check if SYN has been received
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value()) return nullopt; // no ACK if no SYN received before
    
    // Ackno is the first unassmebled byte, indicating where the next segment should be placed
    uint64_t abs_seq = _reassembler.stream_out().bytes_written() + 1 + // +1 for SYN; byte_written is equal to the index of the next byte expected
                    (_reassembler.stream_out().input_ended() ? 1 : 0); // if input ended, add 1 for the FIN
    
    return wrap(abs_seq, _isn.value());
}

size_t TCPReceiver::window_size() const {
    // window size is the distance between the first unassmebled byte (ackno) and the last byte that can be accepted (eof)
    // ackno + window size defines the range of indices that TCP sender can send
    return _capacity - _reassembler.stream_out().buffer_size();
}