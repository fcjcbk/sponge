#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity1) : capacity(capacity1) { DUMMY_CODE(capacity); }

size_t ByteStream::write(const string &data) {
    DUMMY_CODE(data);
    if (input_ended() == true || remaining_capacity() == 0) {
        return 0;
    }

    int write_size = 0;

    if (data.size() + buffer_size() <= capacity) {
        write_size = data.size();
        buffer += data;
    } else {
        write_size = capacity - buffer.size();
        buffer += data.substr(0, write_size);
    }
    
    total_bytes_written += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    DUMMY_CODE(len);
    int peek_len = 0;
    if (len >= buffer_size()) {
        peek_len = buffer_size();
    } else {
        peek_len = 0;
    }
    return buffer.substr(0, peek_len);
}

//! \param[in] len bytes will be removed from the output side of the buffer the total_read_size also would be added
void ByteStream::pop_output(const size_t len) {
    DUMMY_CODE(len);
    if (len >= buffer_size()) {
        total_bytes_read +=  buffer_size();
        buffer = "";
    } else {
        total_bytes_read += len;
        buffer = buffer.substr(len);
    }
    if (input_ended() == true && buffer_empty()) {
        is_eof = true;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    DUMMY_CODE(len);
    if (is_eof) {
        return "";
    }
    int read_size = 0;
    if (len >= buffer_size()) {
        read_size = buffer_size();
    } else {
        read_size = len;
    }
    string res = buffer.substr(0, read_size);
    pop_output(read_size);
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
