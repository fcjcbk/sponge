#include "tcp_sender.hh"

#include "buffer.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <ios>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _flying_bytes; }

void TCPSender::fill_window() {
    if (_is_fin) {
        return;
    }
    // 注意：syn和fin各占一个字节
    if (_stream.buffer_empty() && (next_seqno_absolute() == 0 || _stream.eof()) && next_seqno_absolute() < _right_edge) {
        TCPSegment _segment;
        _segment.header().seqno = wrap(next_seqno_absolute(), _isn);
        if (next_seqno_absolute() == 0) {
            _segment.header().syn = true;
        }
        if (_stream.eof() && _is_fin == false) {
            _segment.header().fin = true;
            _is_fin = true;
        }
        _segments_out.push(_segment);
        _outstanding.push(_segment);
        // tcp的接收窗口会考虑syn与fin标志
        _next_seqno += _segment.length_in_sequence_space();
        _flying_bytes += _segment.length_in_sequence_space();
        if (!_is_time_counter_on) {
            _is_time_counter_on = true;
            _time_count = 0;
        }
    }
    while (next_seqno_absolute() < _right_edge && !_stream.buffer_empty()) {
        // find the largest size can be send
        
        TCPSegment _segment;
        _segment.header().seqno = wrap(next_seqno_absolute(), _isn);
        
        size_t write_data_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, std::min(_right_edge - next_seqno_absolute(), _stream.buffer_size()));
        if (next_seqno_absolute() == 0) {
            _segment.header().syn = true;
            write_data_size--;
        }
        if (write_data_size > 0) {
            _segment.payload() = Buffer(_stream.read(write_data_size));
        }
        
        if (_stream.eof() && _is_fin == false) {
            if (next_seqno_absolute() + _segment.length_in_sequence_space() < _right_edge) {
                _segment.header().fin = true;
                _is_fin = true;
            }
        }
        _segments_out.push(_segment);
        _outstanding.push(_segment);
        // tcp的接收窗口要考虑syn与fin标志大小
        _next_seqno += _segment.length_in_sequence_space();
        _flying_bytes += _segment.length_in_sequence_space();
        if (!_is_time_counter_on) {
            _is_time_counter_on = true;
            _time_count = 0;
        }
    }

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    // 有新的数据到达 必须要将rto值恢复正常
    uint64_t abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    _right_edge = abs_ackno + window_size;
    if (window_size == 0) {
        _right_edge++;
    }
    // 这里是大于等于，同时如果ackno大于next_seqno要拒绝
    while (!_outstanding.empty() 
            && abs_ackno >= _outstanding.front().length_in_sequence_space() + unwrap(_outstanding.front().header().seqno, _isn, abs_ackno)
            && abs_ackno <= next_seqno_absolute()) {
        
        _flying_bytes -= _outstanding.front().length_in_sequence_space();
        _outstanding.pop();

        _retransmission_timeout = _initial_retransmission_timeout;
        _time_count = 0;

        if (_retransmission_consecutive_times > 0) {
            _retransmission_consecutive_times = 0;    
        }
    }
    fill_window();
    if (_flying_bytes == 0) {
        // 没有数据未确认停止定时器
        _is_time_counter_on = false;
        _time_count = 0;
    }
    
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if  (!_is_time_counter_on) {
        return;
    }
    _time_count += ms_since_last_tick;
    if (_time_count >= _retransmission_timeout) {
        // 重传
        _segments_out.push(_outstanding.front());
        // 按照窗口大于0处理
        if (_window_size > 0) {
            _retransmission_consecutive_times++;
            _retransmission_timeout *= 2;
        }
        _time_count = 0;
        
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_consecutive_times; }

void TCPSender::send_empty_segment() {
    TCPSegment _segment;
    _segment.header().seqno = wrap(next_seqno_absolute(), _isn);
    _segments_out.push(_segment);
}
