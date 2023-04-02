#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // flag 表示是否为第一个syn
    bool flag = false;
    if (seg.header().syn == true) {
        if (is_start == false) {
            is_start = true;
            isn = seg.header().seqno;
            flag = true;
        }
    }
    // 未开始，直接退出
    if (is_start == false) {
        return;
    }
    // 计算absolute seqno
    uint64_t absolute_seqno = unwrap(seg.header().seqno, isn, _reassembler.get_current_index());
    std::cout << "absolute_seqno: " << absolute_seqno << std::endl;
    std::cout << "curent_index: " << _reassembler.get_current_index() << std::endl;
    if (seg.header().fin == true) {
        if (is_end == false) {
            is_end = true;

            end_index = absolute_seqno + seg.length_in_sequence_space() - 2;  // 由于fin和syn同时存在
        }
    }
    // 如果是第一个报文，由于第一个报文的stream_index和absolute_seqno是相等的，所以不用减一，而其他因为有syn在所以要减一
    if (absolute_seqno == 0) {
        if (flag) {
            write_to_assembler(seg.payload().copy(), absolute_seqno);
        } else {
            write_to_assembler("", absolute_seqno);
        }
        return;
    } else {            
        write_to_assembler(seg.payload().copy(), absolute_seqno - 1);
    }
    


}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (is_start == false) {
        return {};
    }
    // 因为有syn存在，且此处计算的是绝对index，要对标志位作处理
    uint64_t absolute_seqno = _reassembler.get_current_index() + 1ul;

    // 到达最后，还要加上fin，所以加一
    if (is_end && _reassembler.get_current_index() >= end_index) {
        absolute_seqno++;
    }
    return wrap(absolute_seqno, isn);
}

// 定义 容量减去在byte_stream中的字节数
size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

void TCPReceiver::write_to_assembler(const std::string& data, uint64_t index) {
    std::cout << "window size: " << window_size() << std::endl;
    if (index >= _reassembler.get_current_index() + window_size()) {
        return;
    }
    if (is_end) {
        if (index + data.size() < end_index) {
                _reassembler.push_substring(data, index, is_end);
        } else {
            size_t write_size = end_index - index;
            _reassembler.push_substring(data.substr(0, write_size), index, is_end);
        }
    } else {
        std::cout << "data: " << data << " index: " << index << std::endl;
        _reassembler.push_substring(data, index, is_end);
    }
    
}