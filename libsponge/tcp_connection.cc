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
    // active为false直接退出
    if (active() == false) {
        return;
    }
    // rst的优先级比是否接收syn高
    if (seg.header().rst) {
        set_error();
        return;
    }
    // 只有接收到syn才表示开始
    if (seg.header().syn) {
        _is_tcp_start = true;
    }
    if (!_is_tcp_start) {
        return;
    }
    bool f = false;
    // 更新最后接收报文时间
    _last_segment_received = _time;
    
    // 收到ack，传递给sender
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    _receiver.segment_received(seg);

    // 处理keep_alive报文
    if (_receiver.ackno().has_value()
        && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        f = true;
    }
    // 如果占用sequence number发送一个segment回应
    if (seg.length_in_sequence_space() != 0) {
        f = true;
    }

    _sender.fill_window();
    
    if (f && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    send_segment();
    
    // TCP结束部分
    check_is_end();    
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    if (active() == false) {
        return 0;
    }
    size_t _write_data_size = _sender.stream_in().write(data);
    _sender.fill_window();
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

    check_is_end();
    
    if (_end_count_time && time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        return;
    }

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_RST_segment();
        return;
    }
    // 可能存在超时重传，只有当有包才发，send_segment可能会调用fill_window，导致syn被发送
    // if (!_sender.segments_out().empty()) {
    //     send_segment();
    // }
    send_segment();
}

void TCPConnection::end_input_stream() {
    if (active() == false) {
        return;
    }
    // 发送fin 
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segment();

    check_is_end();
}

void TCPConnection::connect() { 
    if (active() == false) {
        return;
    }
    _sender.fill_window();
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
    
    set_error();
}

void TCPConnection::send_segment() {
    if (active() == false) {
        return;
    }
    // todo: 在收到一个占用seqno的报文应该发送ack即使当前发送方buffer为空
    _sender.fill_window();
    auto& _out = _sender.segments_out();
    if (_out.empty()) {
    }
    while (!_out.empty()) {
        TCPSegment seg = _out.front();
        _out.pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        if (_receiver.window_size() > numeric_limits<uint16_t>().max()) {
            seg.header().win = numeric_limits<uint16_t>().max();
        } else {
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::set_error() {
    // 设置错误状态
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
}

void TCPConnection::check_is_end() {
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() == false) {
        _linger_after_streams_finish = false;
    }
    if (_receiver.stream_out().eof() && _sender.bytes_in_flight() == 0 && _sender.stream_in().eof()) {
        if (_linger_after_streams_finish == false) {
            // active close
            _is_active = false;
        } else {
            // passive close
            _end_count_time = true;
        }
    }
}