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
    , _stream(capacity) {}

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

    }
    // 没有对syn和fin进行处理
    while (_next_seqno < _right_edge && !_stream.buffer_empty()) {
        // find the largest size can be send
        
        size_t write_data_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, std::min(_right_edge - _next_seqno, _stream.buffer_size()));
        
        TCPSegment segment;
        segment.parse(Buffer(_stream.read(write_data_size)));

        segment.header().seqno = wrap(_next_seqno, _isn);
        if (_next_seqno == 0) {
            segment.header().syn = true;
        }
        if (_stream.buffer_empty() && _stream.eof()) {
            segment.header().fin = true;
        }

        _segments_out.push(segment);
        _outstanding.push(segment);
        // tcp的接收窗口应该不会考虑syn与fin标志
        _next_seqno += segment.length_in_sequence_space();
        _flying_bytes += segment.length_in_sequence_space();
    }

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    _right_edge = std::max(_right_edge, abs_ackno + window_size);
    // 这里大于还是大于等于要注意
    while (!_outstanding.empty() &&
            abs_ackno > _outstanding.front().length_in_sequence_space() + unwrap(_outstanding.front().header().seqno, _isn, abs_ackno)) {
        
        _flying_bytes -= _outstanding.front().length_in_sequence_space();
        _outstanding.pop();
    }
    fill_window();
    if (_flying_bytes == 0) {
        // todo: 将定时器停止
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
