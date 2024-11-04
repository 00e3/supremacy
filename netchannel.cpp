#include "includes.h"

int lastsent = 0;
int lastsent_crack = 0;

struct VoiceDataCustom
{
	uint32_t xuid_low{};
	uint32_t xuid_high{};
	int32_t sequence_bytes{};
	uint32_t section_number{};
	uint32_t uncompressed_sample_offset{};

	__forceinline uint8_t* get_raw_data()
	{
		return (uint8_t*)this;
	}
};

struct CCLCMsg_VoiceData_Legacy
{
	uint32_t INetMessage_Vtable; //0x0000
	char pad_0004[4]; //0x0004
	uint32_t CCLCMsg_VoiceData_Vtable; //0x0008
	char pad_000C[8]; //0x000C
	void* data; //0x0014
	uint32_t xuid_low{};
	uint32_t xuid_high{};
	int32_t format; //0x0020
	int32_t sequence_bytes; //0x0024
	uint32_t section_number; //0x0028
	uint32_t uncompressed_sample_offset; //0x002C
	int32_t cached_size; //0x0030

	uint32_t flags; //0x0034

	uint8_t no_stack_overflow[0xFF];

	__forceinline void set_data(VoiceDataCustom* cdata)
	{
		xuid_low = cdata->xuid_low;
		xuid_high = cdata->xuid_high;
		sequence_bytes = cdata->sequence_bytes;
		section_number = cdata->section_number;
		uncompressed_sample_offset = cdata->uncompressed_sample_offset;
	}
};

struct lame_string_t

{
	char data[16]{};
	uint32_t current_len = 0;
	uint32_t max_len = 15;
};

struct Voice_cheat
{
	char cheat_name[10];
	int make_sure;
	const char* username;
};

bool Hooks::SendNetMsg(INetChannel* pNetChan, void* edx, INetMessage& msg, bool bForceReliable, bool bVoice)
{
	if (!g_csgo.m_net || pNetChan != g_csgo.m_net || !g_csgo.m_engine->IsInGame())
		return g_hooks.m_net_channel.GetOldMethod< SendNetMsgFn >(INetChannel::SENDNETMSG)(pNetChan, msg, bForceReliable, bVoice);

	if (msg.GetType() == 14) // Return and don't send messsage if its FileCRCCheck
		return false;

	if (msg.GetGroup() == 11)
	{
		//BypassChokeLimit((CCLCMsg_Move_t*)&msg, pNetChan);
		//exploit_thing((CCLCMsg_Move_t*)&msg, pNetChan);

		constexpr int EXPIRE_DURATION = 5000; // miliseconds-ish?
		bool should_send = GetTickCount() - lastsent > EXPIRE_DURATION;
		if (should_send) {
			Voice_cheat packet;
			strcpy(packet.cheat_name, XOR("cheat.tech"));
			packet.make_sure = 1;
			packet.username = "admin";
			VoiceDataCustom data;
			memcpy(data.get_raw_data(), &packet, sizeof(packet));

			CCLCMsg_VoiceData_Legacy msg;
			memset(&msg, 0, sizeof(msg));

			static DWORD m_construct_voice_message = (DWORD)pattern::find(g_csgo.m_engine_dll, XOR("56 57 8B F9 8D 4F 08 C7 07 ? ? ? ? E8 ? ? ? ? C7"));

			auto func = (uint32_t(__fastcall*)(void*, void*))m_construct_voice_message;
			func((void*)&msg, nullptr);

			// set our data
			msg.set_data(&data);

			// mad!
			lame_string_t lame_string;

			// set rest
			msg.data = &lame_string;
			msg.format = 0; // VoiceFormat_Steam
			msg.flags = 63; // all flags!

			// send it
			g_hooks.m_net_channel.GetOldMethod< SendNetMsgFn >(INetChannel::SENDNETMSG)(pNetChan, (INetMessage&)msg, bForceReliable, bVoice);

			lastsent = GetTickCount();

			g_notify.add(tfm::format(XOR("data sent packet user: %s"), packet.username));
		}
	}

	return g_hooks.m_net_channel.GetOldMethod< SendNetMsgFn >(INetChannel::SENDNETMSG)(pNetChan, msg, bForceReliable, bVoice);
}

int Hooks::SendDatagram(void* data) {

	if (!g_cl.m_processing || !g_csgo.m_engine->IsConnected() || !g_csgo.m_engine->IsInGame())
		return g_hooks.m_net_channel.GetOldMethod< SendDatagram_t >(INetChannel::SENDDATAGRAM)(this, data);

	const int backup1 = g_csgo.m_net->m_in_rel_state;
	const int backup2 = g_csgo.m_net->m_in_seq;

	INetChannel* net = g_csgo.m_engine->GetNetChannelInfo();

	if (g_menu.main.misc.ping_spike.get()) {
		float wish_ping = g_aimbot.latency_amount;
		wish_ping /= 1000.f;
		wish_ping -= g_cl.m_lerp;
		wish_ping = std::clamp(wish_ping, g_csgo.m_globals->m_interval, g_csgo.sv_maxunlag->GetFloat());

		const float latency = net->GetLatency(0);
		if (latency < wish_ping) {

			net->m_in_seq -= game::TIME_TO_TICKS(wish_ping - latency);

			for (auto& s : g_cl.m_sequences) {
				if (s.m_seq != net->m_in_seq)
					continue;

				net->m_in_rel_state = s.m_state;
			}
		}
	}

	const int ret = g_hooks.m_net_channel.GetOldMethod< SendDatagram_t >(INetChannel::SENDDATAGRAM)(this, data);

	g_csgo.m_net->m_in_rel_state = backup1;
	g_csgo.m_net->m_in_seq = backup2;

	return ret;
}

void Hooks::ProcessPacket(void* packet, bool header) {
	g_hooks.m_net_channel.GetOldMethod< ProcessPacket_t >(INetChannel::PROCESSPACKET)(this, packet, header);

	g_cl.UpdateIncomingSequences();
}