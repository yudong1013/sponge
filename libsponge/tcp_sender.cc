#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

size_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint16_t window_size = max(_window_size, static_cast<uint16_t>(1)); // act like a stop-and-wait protocol if window size is 0
    
    while (_bytes_in_flight < window_size) {
        TCPSegment seg;
        
        if (!_syn_flag) { // send SYN if not sent
            seg.header().syn = true;
            _syn_flag = true;
        }

        // payload size is constrained by the segment size, buffer size, and the window size
        auto payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, 
                            min(window_size - _bytes_in_flight - (seg.header().syn ? 1 : 0) - (seg.header().fin ? 1 : 0), // available window size
                             _stream.buffer_size()));
        auto payload = _stream.read(payload_size); // read from the outbound byte stream
        seg.payload() = Buffer(move(payload));

        // stop sending by setting the FIN flag if the stream is empty
        if (!_fin_flag && _stream.eof() && _bytes_in_flight + seg.length_in_sequence_space() < window_size) {
            _fin_flag = true;
            seg.header().fin = true;
        }

        uint64_t seg_length = seg.length_in_sequence_space();
        if (seg_length == 0) break; // no more data to send

        seg.header().seqno = next_seqno(); // set the seqno of the segment and send it
        _segments_out.push(seg);

        if (!_timer.is_running()) _timer.restart(); // start the timer

        _outstanding_seg.emplace(_next_seqno, std::move(seg)); // add to the outstanding segment queue
        
        _next_seqno += seg_length; // _next_seqno is absolute seqno, accumulated from 0
        _bytes_in_flight += seg_length;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    if (abs_ackno > next_seqno_absolute()) return; // window start after the next seqno
    
    // clear all acked outstanding segments
    bool is_successful = false;
    while (!_outstanding_seg.empty()) { 
        auto &[abs_seqno, seg] = _outstanding_seg.front(); // no need to check elements other than the first one as ackno range is continuous
        if (abs_seqno + seg.length_in_sequence_space() - 1 < abs_ackno) { // skip if already acked
            is_successful = true;
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outstanding_seg.pop();
        } else {
            break;
        } 
    }

    // reset the timer and the retransmission cnt if the segment is acked
    if (is_successful) { // the oldest outstanding segment(s) is acked
        _consecutive_retransmission_cnt = 0;
        _timer.set_rto(_initial_retransmission_timeout);
        _timer.restart();
    }

    if (_bytes_in_flight == 0) {
        _timer.stop();
    }

    _window_size = window_size;
    fill_window(); // continue sending on receiving ACK
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
// called every few milliseconds; track the passage of time by adding ms to the timer; retransmit if timeout
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick); // accumulate over time
    
    if (_timer.is_expired() && !_outstanding_seg.empty()) {
        _segments_out.push(_outstanding_seg.front().second); // retransmit if timeout
        
        // exponential backoff and increment cnt, as long as the ACK is not received
        ++_consecutive_retransmission_cnt;
        _timer.set_rto(_timer.get_rto() * 2); 
        _timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission_cnt; }

void TCPSender::send_empty_segment() {
    // Empty segment just for ACK
    TCPSegment seg;
    seg.header().seqno = next_seqno(); // ackno is the seqno of the next byte expected
    _segments_out.emplace(std::move(seg));
}