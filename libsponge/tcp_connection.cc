#include "tcp_connection.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time - _last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if (active() == false) {
        return;
    }
    std::cout << "received" << std::endl;
    bool f = false;
    // 更新最后接收报文时间
    _last_segment_received = _time;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _is_active = false;
        return;
    }
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    _receiver.segment_received(seg);
    if (_receiver.ackno().has_value()
        && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        f = true;
    }
    // todo: 如果占用sequence number发送一个segment回应
    if (seg.length_in_sequence_space() != 0) {
        std::cout << "true" << std::endl;
        f = true;
    }
    if (f) {
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            std::cout << "generate a empty segmet" <<std::endl;
            _sender.send_empty_segment();
        }
        send_segment();
    }
    // TCP结束部分
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() == false) {
        _linger_after_streams_finish = false;
    }
    if (_receiver.stream_out().eof() && _sender.bytes_in_flight() == 0 && _sender.stream_in().eof()) {
        if (_linger_after_streams_finish == false) {
            _is_active = false;
        } else {
            _end_count_time = true;
        }
    }
    
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    if (active() == false) {
        return 0;
    }
    size_t _write_data_size = _sender.stream_in().write(data);
    send_segment();
    return _write_data_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if (active() == false) {
        return;
    }
    _time += ms_since_last_tick; 
    _sender.tick(ms_since_last_tick);
    // 可能存在超时重传
    send_segment();
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_RST_segment();
    }
    if (_end_count_time && time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
        _is_active = false;
    }
}

void TCPConnection::end_input_stream() {
    // if (active() == false) {
    //     return;
    // }
    // 发送fin 
    std::cout << "end_input_stream is_eof: " << _sender.stream_in().eof() << std::endl;
    _sender.stream_in().end_input();
    std::cout << "end_input_stream is_eof: " << _sender.stream_in().eof() << std::endl;
    send_segment();
}

void TCPConnection::connect() { 
    if (active() == false) {
        return;
    }
    send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_RST_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_RST_segment() {
    _sender.send_empty_segment();
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    seg.header().rst = true;
    _segments_out.push(seg);
}

void TCPConnection::send_segment() {
    // todo: 在收到一个占用seqno的报文应该发送ack即使当前发送方buffer为空
    _sender.fill_window();
    auto& _out = _sender.segments_out();
    if (_out.empty()) {
        std::cout << "empty!!!" << std::endl;
    }
    while (!_out.empty()) {
        TCPSegment seg = _out.front();
        _out.pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        std::cout << "send segment  ";
        _segments_out.push(seg);
        if (seg.header().syn == true) {
            std::cout << "syn " << _out.size();
        }
        if (seg.header().ack == true) {
            std::cout << " ack" << std::endl;
        } else {
            std::cout << std::endl;
        }
    }
}