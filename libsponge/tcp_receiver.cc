#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    // 此处需要判断是否为fin，同时为fin要进行下一步操作
    bool flag = false;
    if (seg.header().syn == true) {
        if (is_start == false) {
            is_start = true;
            isn = seg.header().seqno;
            flag = true;
        }
    }
    if (is_start == false) {
        return;
    }
    uint64_t absolute_index = unwrap(seg.header().seqno, isn, _reassembler.get_current_index());

    // bool flag = false; // 在参数传递时判断是否为eof
    if (seg.header().fin == true) {
        if (is_end == false) {
            is_end = true;
            if (seg.header().syn == true) {
                end_index = absolute_index + seg.length_in_sequence_space() - 2; // 由于fin和syn同时存在
            } else {
                end_index = absolute_index + seg.length_in_sequence_space() - 1; // 由于fin和syn存在
            }
        }
    }
    if (absolute_index == 0) {
        if (flag) {
            write_to_assembler(seg.payload().copy(), absolute_index);
        } else {
            write_to_assembler("", absolute_index);
        }
        return;
    } else {            
        write_to_assembler(seg.payload().copy(), absolute_index - 1);
    }
        // write_to_assembler(seg.payload().copy(), absolute_index - 1);
    


}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (is_start == false) {
        return {};
    }
    // 要对标志位作处理
    uint64_t absolute_index = _reassembler.get_current_index() + 1ul;
    if (is_end && _reassembler.get_current_index() == end_index) {
        absolute_index++;
    }
    return wrap(absolute_index, isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

void TCPReceiver::write_to_assembler(const std::string& data, uint64_t index) {
    if (is_end) {
        if (index + data.size() < end_index) {
                _reassembler.push_substring(data, index, is_end);
        } else {
            size_t write_size = end_index - index;
            _reassembler.push_substring(data.substr(0, write_size), index, is_end);
        }
    } else {
        _reassembler.push_substring(data, index, is_end);
    }
    
}