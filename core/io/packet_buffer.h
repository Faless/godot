
#include "core/ring_buffer.h"

template<class T>
class PacketBuffer {

private:
	RingBuffer<T> _info;
	RingBuffer<uint8_t> _payload;

public:

	Error write_packet_info(const T *p_info) {
		ERR_FAIL_COND_V(_info.space_left() < 1, ERR_OUT_OF_MEMORY);
		_info.write(p_info, 1);
		return OK;
	}

	Error write_packet_payload(const void *p_payload, int p_bytes) {
		ERR_FAIL_COND_V(_payload.space_left() < p_bytes, ERR_OUT_OF_MEMORY);
		_payload.write((const uint8_t *)p_payload, p_bytes);
		return OK;
	}

	Error read_packet_info(T *r_info) {
		ERR_FAIL_COND_V(_info.data_left() < 1, ERR_UNAVAILABLE);
		_info.read(r_info, 1);
		return OK;
	}

	Error read_packet_payload(uint8_t *r_payload, int p_bytes) {
		ERR_FAIL_COND_V(_payload.data_left() < p_bytes, ERR_UNAVAILABLE);
		_payload.read(r_payload, p_bytes);
		return OK;
	}

	void set_payload_size(int p_shift) {
		_payload.resize(p_shift);
	}

	void set_max_packets(int p_shift) {
		_info.resize(p_shift);
	}

	int packets_left() const {
		return _info.data_left();
	}

	int packets_space() const {
		return _info.space_left();
	}

	int payload_left() const {
		return _payload.data_left();
	}

	int payload_space() const {
		return _payload.space_left();
	}

	void clear() {
		_info.resize(0);
		_payload.resize(0);
	}

	PacketBuffer() {
		clear();
	}

	~PacketBuffer() {
		clear();
	}
};
