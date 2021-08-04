extern "C" {
	#include <CoreMIDI/CoreMIDI.h>
	#include <CoreFoundation/CoreFoundation.h>
}
#include <unordered_map>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <cstddef>

// Uncomment this line if you want that all non-realtime status bytes abort the current SysEx, as per the MIDI specification.
// #define RESPECT_SYSEX_ABORT 1

static uint8_t sysex_id_byte = 0x70;

class syxlog_parser {
public:
	void process(uint8_t byte) {
		if(byte == 0xF0) {
			if(state == state_reading && size) {
				buffer[size] = '\0';
				printf("%s [aborted]\n", buffer);
				size = 0;
			}
			state = state_waiting_id;
			return;
		}

		switch(state) {
			case state_idle:
				break;
			case state_waiting_id:
				if(byte == sysex_id_byte) {
					state = state_reading;
					size = 0;
				} else {
					state = state_idle;
				}
				break;
			case state_reading:
				if(byte == 0xF7) {
					state = state_idle;
					if(!size) return;
					buffer[size] = '\0';
					printf("%s\n", buffer);
					size = 0;
				} else if(byte < 0x80) {
					if(size == capacity) {
						// Output the current text and continue with more later
						buffer[size] = '\0';
						printf("%s [...]\n", buffer);
						size = 0;
					}
					buffer[size] = byte;
					++size;
				}
#if defined(RESPECT_SYSEX_ABORT)
				else if(byte < 0xF8) {
					// Non-realtime status bytes should abort the SysEx
					buffer[size] = '\0';
					printf("%s [aborted]\n", buffer);
					size = 0;
					state = state_idle;
				}
#endif
				break;
		}
	}
private:
	static constexpr size_t capacity = 256;
	enum state_t {
		state_idle,
		state_waiting_id,
		state_reading
	};
	char buffer[capacity];
	size_t size = 0;
	state_t state = state_idle;
};

static MIDIClientRef client = 0;
static MIDIPortRef input_port = 0;
static std::unordered_map<MIDIUniqueID, std::unique_ptr<syxlog_parser>> inputs;

static void read_proc_external(const MIDIPacketList* list, void*, void* refcon) {
	MIDIUniqueID uid = reinterpret_cast<uintptr_t>(refcon);
	const auto in = inputs[uid].get();
	if(!in) return;
	const MIDIPacket* packet = list->packet;
	for(size_t i=0; i<list->numPackets; ++i) {
		for(size_t j=0; j<packet->length; ++j) {
			const auto byte = packet->data[j];
			in->process(byte);
		}
		packet = MIDIPacketNext(packet);
	}
}

static void notify_proc(const MIDINotification* message, void*) {
	const auto id = message->messageID;
	const auto added = id == kMIDIMsgObjectAdded;
	const auto removed = id == kMIDIMsgObjectRemoved;
	if(!added && !removed) return;


	const auto info = reinterpret_cast<const MIDIObjectAddRemoveNotification*>(message);
	if(info->childType != kMIDIObjectType_Source) return;
	
	MIDIEndpointRef source = info->child;
	MIDIUniqueID uid = 0;
	MIDIObjectGetIntegerProperty(source, kMIDIPropertyUniqueID, &uid);
	if(uid == 0) return;

	if(added) {
		inputs[uid] = std::make_unique<syxlog_parser>();
		MIDIPortConnectSource(input_port, source, reinterpret_cast<void*>(uid));
	} else {
		const auto it = inputs.find(uid);
		if(it == inputs.end()) return;

		inputs.erase(uid);
		MIDIPortDisconnectSource(input_port, source);
	}
}

static void midi_init() {
	MIDIClientCreate(CFSTR("syxlog"), notify_proc, nullptr, &client);
	MIDIInputPortCreate(client, CFSTR("syxlog"), read_proc_external, nullptr, &input_port);

	// Connect to everything already there
	const auto size = MIDIGetNumberOfSources();
	for(size_t i=0; i<size; ++i) {
		MIDIEndpointRef source = MIDIGetSource(i);
		MIDIUniqueID uid = 0;
		MIDIObjectGetIntegerProperty(source, kMIDIPropertyUniqueID, &uid);
		if(!uid) continue;
		inputs[uid] = std::make_unique<syxlog_parser>();
		MIDIPortConnectSource(input_port, source, reinterpret_cast<void*>(uid));
	}
}

int main(int argc, char const *argv[]) {
	midi_init();

	CFRunLoopRef runloop = CFRunLoopGetCurrent();
	CFRunLoopRun();

	return 0;
}
