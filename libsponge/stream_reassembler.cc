#include "stream_reassembler.hh"
#include <string>
#include <limits>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), 
                                                              _stream(capacity), _exp_index(0),
                                                              _eof_index(std::numeric_limits<size_t>::max()), 
                                                              _num_bytes_unassembled(0) {}
 
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) _eof_index = min(_eof_index, index + data.size());

    auto left = max(index, _exp_index); // the left bound of the segment
    auto right = min(index + data.size(), min(_exp_index - _output.buffer_size() + _capacity, _eof_index)); // the right bound of the segment
    
    for (size_t i = left, j = left - index; i < right; ++i, ++j) { // j is the first non-overlapped index of the substring
        auto &t = _stream[i % _capacity];
        if (t.second == true) { // already occupied
            if (t.first != data[j]) return; // discard if the segment is inconsistent 
            // do nothing if overlapped
        } else {
            t = make_pair(data[j], true);
            ++_num_bytes_unassembled;
        }
    }
    
    string str;
    while (_exp_index < _eof_index && _stream[_exp_index % _capacity].second == true) { // pop out as long as it's contiguous
        str.push_back(_stream[_exp_index % _capacity].first); 
        _stream[_exp_index % _capacity] = { 0, false }; // clear the room for future use
        --_num_bytes_unassembled, ++_exp_index;
    }
    _output.write(str);

    if (_exp_index >= _eof_index) _output.end_input();
}
 
size_t StreamReassembler::unassembled_bytes() const { return _num_bytes_unassembled; }
 
bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
