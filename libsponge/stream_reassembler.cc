#include "stream_reassembler.hh"
#include <cstddef>
#include <cstdint>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;

        end_index = index + data.size();
    }
    
    for (size_t i = 0; i < data.size(); i++) {
        uint64_t n_index = index + i;
        if (_eof) {
            if (n_index >= end_index) {
                break;
            }
        }
        if (n_index < current_index || book.count(n_index)) {
            continue;
        }
        size_t remain_capacity = remaining_capacity();
        if (n_index == current_index) {
            size_t write_size = _output.write_single(data[i]);
            if (write_size == 1) {
                current_index++;
                book.emplace(n_index);
                if (check_is_end()) {
                    return;
                }
                continue;
            }

        }

        if (remain_capacity > 0) {
            book.emplace(n_index);
            qu.push({n_index, data[i]});
        } else {
            break;
        }
            
        
    }
    write_bytestream();

}

size_t StreamReassembler::unassembled_bytes() const {  return qu.size(); }

bool StreamReassembler::empty() const { return qu.empty(); }

void StreamReassembler::write_bytestream() {
    if (check_is_end()) {
        return;
    }
    while (!qu.empty() && qu.top().first == current_index) {
        auto [_, data] = qu.top();
        size_t write_size = _output.write_single(data);
        if (write_size == 0) {
            return;
        }
        qu.pop();
        book.erase(current_index);
        current_index++;
        if (check_is_end()) {
            break;
        }
        
    }
}

bool StreamReassembler::check_is_end() {
    if (_eof && current_index == end_index) {
        _output.end_input();
        return true;
    }
    return false;
}