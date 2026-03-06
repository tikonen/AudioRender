#pragma once

#pragma pack(push)
#pragma pack(1)

struct FTSample {
	uint16_t x : 14;
	uint16_t : 1; // reserved
	uint16_t resetx : 1; // reset x integrator
	uint16_t y : 14;
	uint16_t : 1; // reserved
	uint16_t resety : 1; // reset y integrator
	uint16_t wait : 10; // microseconds	
	uint16_t : 5; // reserved
	uint16_t nodac : 1; // Do not update dac
};

// packet types
#define FT_P_TYPE_CONTROL 0x00
#define FT_P_TYPE_FRAME 0x01
#define FT_P_TYPE_SYNC 0x02

#define FT_MIN_PACKET_SIZE 64
#define FT_MAX_PACKET_SIZE 1024
#define FT_MAX_PACKET_SAMPLES ((FT_MAX_PACKET_SIZE - offsetof(FTPacket, frame.samples)) / sizeof(FTSample))

#define FT_SAMPLE_MAX_WAIT_US ((1 << 10) - 1)
#define FT_SAMPLE_MAX_FPS ((1 << 4) - 1)
#define FT_FRAME_FPS_MUL_MS 5

struct FTPacket {
	uint16_t size; // packet total length
	uint16_t type : 4;  // 0: control, 1: frame
	uint16_t : 12; // reserved
	union {
		struct {
			uint8_t sof : 1; // start of frame
			uint8_t eof : 1; // end of frame
			uint8_t fps : 4; // x 5ms frame
			uint8_t : 2; // reserved
			uint8_t reserved[3]; // reserved
			uint16_t count; // number of samples in the packet
			FTSample samples[1];
		} frame;
		struct {
			uint16_t : 16; // reserved
			// TBD
		} control;
		struct {
			uint32_t ts; // timestamp
		} sync;

		uint8_t bytes[FT_MIN_PACKET_SIZE - 4];
	};
};

#pragma pack(pop)
