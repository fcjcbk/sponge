#include "byte_stream.hh"
#include <cstddef>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity1) : capacity(capacity1) { DUMMY_CODE(capacity); }

size_t ByteStream::write(const string &data) {
    if (input_ended() == true || remaining_capacity() == 0) {
        return 0;
    }

    size_t write_size = std::min(remaining_capacity(), data.size());

    for (size_t i = 0; i < write_size; i++) {
        buffer.push_back(data[i]);
    }   
    total_bytes_written += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = std::min(len, buffer_size());
    string peek_str;
    for (size_t i = 0; i < peek_len; i++) {
        peek_str.push_back(buffer[i]);
    }
    return peek_str;
}

//! \param[in] len bytes will be removed from the output side of the buffer the total_read_size also would be added
void ByteStream::pop_output(const size_t len) {
    if (len > buffer_size()) {
        set_error();
        return;
    }
    for (size_t i = 0; i < len; i++) {
        buffer.pop_front();
    }
    total_bytes_read += len;
    if (input_ended() == true && buffer_empty()) {
        is_eof = true;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (is_eof) {
        return "";
    }
    if (len > buffer_size()) {
        set_error();
        return "";
    }
    string res = peek_output(len);
    pop_output(len);
    return res;
}

// should examine buffer is empty and mark eof
void ByteStream::end_input() { 
    is_input_ended = true;
    if (buffer_size() == 0) {
        is_eof = true;
    }
}

size_t ByteStream::write_single(const char& data) {
    if (input_ended() == true || remaining_capacity() == 0) {
        return 0;
    }
    total_bytes_written++;
    buffer.push_back(data);
    return 1;
}

bool ByteStream::input_ended() const { return is_input_ended; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return is_eof; }

size_t ByteStream::bytes_written() const { return total_bytes_written; }

size_t ByteStream::bytes_read() const { return total_bytes_read; }

size_t ByteStream::remaining_capacity() const { return capacity - buffer_size(); }

