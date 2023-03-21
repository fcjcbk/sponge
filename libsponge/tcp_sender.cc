#include "tcp_sender.hh"

#include "buffer.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cstdint>
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
    //todo: 发送报文段时要检查定时器是否启动
    if (_window_size == 0) {
        size_t write_data_size = 1;
        TCPSegment segment;
        segment.parse(Buffer(_stream.read(write_data_size)));
        if (_stream.buffer_empty() && _stream.eof()) {
            segment.header().fin = true;
        }
        _segments_out.push(segment);
        _outstanding.push(segment);
        _next_seqno += segment.length_in_sequence_space();
        _flying_bytes += segment.length_in_sequence_space();
        if (!_is_time_counter_on) {
            _is_time_counter_on = true;
            _time_count = 0;
        }
        return;
    }
    // 没有对syn和fin进行处理
    while (_next_seqno < _right_edge) {
        // find the largest size can be send
        
        TCPSegment _segment;
        _segment.header().seqno = wrap(_next_seqno, _isn);
        if (_stream.buffer_empty()) {
            if (_next_seqno == 0) {
                _segment.header().syn = true;
            }
            if (_stream.eof()) {
                _segment.header().fin = true;
            }
        } else {
            size_t write_data_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, std::min(_right_edge - _next_seqno, _stream.buffer_size()));
            _segment.parse(Buffer(_stream.read(write_data_size)));

            if (_next_seqno == 0) {
                _segment.header().syn = true;
            }
            if (_stream.buffer_empty() && _stream.eof()) {
                _segment.header().fin = true;
            }
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

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    // todo: 如果是确认重传必须要将rto值恢复正常
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    _right_edge = std::max(_right_edge, abs_ackno + window_size);
    // 这里大于还是大于等于要注意
    while (!_outstanding.empty() 
            && abs_ackno > _outstanding.front().length_in_sequence_space() + unwrap(_outstanding.front().header().seqno, _isn, abs_ackno)) {
        
        _flying_bytes -= _outstanding.front().length_in_sequence_space();
        _outstanding.pop();

        if (_retransmission_consecutive_times > 0) {
            _retransmission_consecutive_times = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
            _time_count = 0;
        }
    }
    fill_window();
    if (_flying_bytes == 0) {
        // todo: 将定时器停止
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
        _segments_out.push(_outstanding.front());
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
    _segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(_segment);
}
