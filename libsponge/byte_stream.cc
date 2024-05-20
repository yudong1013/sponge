#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buffer(capacity + 1), _capacity(capacity), _bytes_written(0),
                                                _bytes_read(0), _head(0), _tail(_capacity) {}
 
size_t ByteStream::write(const string &data) {
    auto n = min(data.size(), remaining_capacity());
    for (size_t i = 0; i < n; ++i) {
        _tail = (_tail + 1) % (_capacity + 1); // increase the buffer size by shifting the tail
        _buffer[_tail] = data[i];
    }
    _bytes_written += n;
    return n;
}
 
//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string output;
    auto n = min(len, buffer_size());
    for (size_t i = 0; i < n; ++i) {
        output.push_back(_buffer[(_head + i) % (_capacity + 1)]);
    }
    return output;
}
 
//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto n = min(len, buffer_size());
    _head = (_head + n) % (_capacity + 1); // fake pop in circular buffer
    _bytes_read += n;
}
 
//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    auto str_read = peek_output(len);
    pop_output(len);
    return str_read;
}
 
void ByteStream::end_input() { _closed = true; }
 
bool ByteStream::input_ended() const { return _closed; }
 
size_t ByteStream::buffer_size() const {
     return (_tail - _head + 1 + _capacity + 1) % (_capacity + 1) ; // the size of the buffer; handles the wrap-around case 
}
 
bool ByteStream::buffer_empty() const { return buffer_size() == 0; }
 
bool ByteStream::eof() const { return _closed && buffer_empty(); }
 
size_t ByteStream::bytes_written() const { return _bytes_written; }
 
size_t ByteStream::bytes_read() const { return _bytes_read; }
 
size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
