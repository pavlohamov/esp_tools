
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <concepts>

#include "rom/crc.h"

typedef struct {
	uint16_t crc16;
	uint16_t length;
	uint32_t handle; // always increased. must not be 0
	uint8_t data[0];
} TLV_t;

template<typename B>
concept NvBackend = requires(B& b, off_t off, void* dst, const void* src, size_t size) {
    { b.read(off, dst, size) }  -> std::same_as<bool>;
    { b.write(off, src, size) } -> std::same_as<bool>;
    { b.erase(size) }           -> std::same_as<bool>;
};

template<typename Typ, size_t max_size, NvBackend Backend>
class NvStorage {
public:
	constexpr static size_t chunkSize = sizeof(TLV_t) + sizeof(Typ);
	static_assert(chunkSize <= max_size);

	struct element {
		TLV_t hdr;
		Typ data;
	};

	constexpr explicit NvStorage(Backend& be) : be_(be) {}

	bool find(Typ& val, off_t& off) {
		element el;
		if (!find(el, off))
			return false;
		val = el.data;
		return true;
	}

	bool find(Typ& val) {
		off_t off;
		return find(val, off);
	}

	bool set(const Typ &val) {
		element el;
		off_t off = 0;
		if (!find(el, off))
			memset(&el, 0, sizeof(el));
		else
			off += chunkSize;
		el.hdr.handle += 1;

		if (!el.hdr.handle) { // if handle wraps: erase and start from begining
			if (!be_.erase(max_size))
				return false;
			el.hdr.handle = 1;
			off = 0;
		}

		el.data = val;
		el.hdr.length = sizeof(element);
		el.hdr.crc16 = crc16_le(0, (uint8_t*)&el.hdr.length, sizeof(element) - sizeof(el.hdr.crc16));

		if (off + chunkSize > max_size)
			off = 0;

		return set(el, off);
	}

private:
	Backend& be_;

	bool find(element& el, off_t& off) {
		ssize_t bestAt = -1;
		uint32_t best = 0;
		for (size_t i = 0; i < max_size; i += sizeof(el)) {
			if (!be_.read(i, &el, sizeof(el)))
				continue;
			if (el.hdr.length != sizeof(element))
				continue;
			auto crc = crc16_le(0, (uint8_t*)&el.hdr.length, sizeof(element) - sizeof(el.hdr.crc16));
			if (crc != el.hdr.crc16)
				continue;
// element is valid
			if (!el.hdr.handle || best > el.hdr.handle)
				continue;
			best = el.hdr.handle;
			bestAt = i;
		}
		if (bestAt == -1)
			return false;

		if (!be_.read(bestAt, &el, sizeof(el)))
			return false;
		auto crc = crc16_le(0, (uint8_t*)&el.hdr.length, sizeof(element) - sizeof(el.hdr.crc16));
		if (crc != el.hdr.crc16)
			return false;
		if (best != el.hdr.handle)
			return false;

		off = bestAt;
		return true;
	}

	bool set(element& el, off_t off) {
		if (off + chunkSize > max_size)
			return false;

		auto crc = crc16_le(0, (uint8_t*)&el.hdr.length, sizeof(element) - sizeof(el.hdr.crc16));
		if (crc != el.hdr.crc16)
			return false;

		return be_.write(off, &el, sizeof(element));
	}
};
