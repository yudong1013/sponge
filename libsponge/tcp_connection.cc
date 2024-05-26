#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;

    const auto& header =  seg.header();

    // Close the connection if RST is received
    if (header.rst) {
        _set_rst_state(false); // no need to send RST back
        return;
    }

    // give the segment to the receiver to extract data
    // while receiving FIN, end the input stream in reassembler
    _receiver.segment_received(seg);

    // must be acked when the segment has payload or SYN or FIN
    bool need_empty_ack = seg.length_in_sequence_space() > 0;

    // if the segment takes an ACK
    if (header.ack) {
        // ack SYN, update ackno and window size, and fill the window 
        // also handle ACK in the third handshake, with payload filled here and ACK added below
        _sender.ack_received(header.ackno, header.win);
        // no need to send empty ack if we can send ack with segments (piggybacking)
        if (need_empty_ack && !_segments_out.empty())
            need_empty_ack = false;
    }

    // while receiving SYN at LISTEN state，the connection changes its state to SYN RECEIVED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // send back SYN ACK
        connect();
        return;
    }

    // clean shutdown (Passive close)
    // check if the passive CLOSE is initiated
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && // FIN received; inbound stream ended (#1)
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) { // stream ongoing (outbound not reaching eof)
        _linger_after_streams_finish = false;
    }

    // if everything for passive CLOSE is satisfied, close the connection
    if (!_linger_after_streams_finish && 
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && // input ended (#1)
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) { // outbound stream ended and acked (#2 #3)
        _is_active = false;
        _linger_after_streams_finish = false;
        return;
    }
    
    // Keep-alive mechanism
    if (_receiver.ackno().has_value() && // SYN received
         (seg.length_in_sequence_space() == 0) && // no payload
         seg.header().seqno == _receiver.ackno().value() - 1) { // seqno is the last acked byte
        need_empty_ack = true;
    }

    // send empty ACK when 1. can't send ACK with the segment, 2. keep-alive mechanism
    if (need_empty_ack) {
        _sender.send_empty_segment();
    }

    // ack and send all available segments in this round
    _add_ackno_and_window_and_send();
}

bool TCPConnection::active() const { return _is_active; }

// applications write data to the outbound byte stream and send it over TCP
size_t TCPConnection::write(const string &data) {
    auto n = _sender.stream_in().write(data);
    _sender.fill_window();
    _add_ackno_and_window_and_send();
    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;

    // call sender's tick to handle retransmission and backoff logic
    _sender.tick(ms_since_last_tick);

    // RST if too many retransmissions
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // clear up everything and close the connection
        while (!_sender.segments_out().empty()) _sender.segments_out().pop();
        _set_rst_state(true);
        return;
    }

    // retranmission
    _add_ackno_and_window_and_send();

    // Clean shutdown (Active close)
    if (_linger_after_streams_finish && // not passive close case
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && // inbound stream has ended (#1)
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && // outbound stream ended and acked (#2 #3)
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
    }
}

// close the outbound byte stream; send FIN
void TCPConnection::end_input_stream() { 
    // stop getting more data from the application
    _sender.stream_in().end_input();
    // finish up sending the remaining data as segments and send FIN
    _sender.fill_window();
    _add_ackno_and_window_and_send();
}

// initiate a connection; handles SYN and SYN ACK
void TCPConnection::connect() {
    // SYN is sent in fill_window(), with no payload as the initial window size is 1
    _sender.fill_window();
    // ACK is added
    _add_ackno_and_window_and_send();
}

// the object destructs itself by sending a RST segment and closing the connection
TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_set_rst_state(const bool send_rst) {
    // tell the server to close by sending RST segment
    if (send_rst) {
        TCPSegment seg;
        seg.header().seqno = _sender.next_seqno();
        seg.header().rst = true;
        // no ack needed
        _segments_out.emplace(std::move(seg));
    }
    // close itself (unclean shutdown)
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}

void TCPConnection::_add_ackno_and_window_and_send() {
    // for all seg in sender's queue, take it out, set ackno and window size, and put it back
    while (!_sender.segments_out().empty()) {
        auto seg = std::move(_sender.segments_out().front());
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) { // read the ackno from the receiver; no need to ack if no ackno (the first SYN)
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        // read the window size from the receiver, with the maximum value of uint16_t
        seg.header().win = min(static_cast<size_t>(numeric_limits<uint16_t>::max()), _receiver.window_size());
        _segments_out.emplace(std::move(seg));
    }
}