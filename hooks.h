#pragma once

class bf_read {
public:
	const char* m_pDebugName;
	bool m_bOverflow;
	int m_nDataBits;
	unsigned int m_nDataBytes;
	unsigned int m_nInBufWord;
	int m_nBitsAvail;
	const unsigned int* m_pDataIn;
	const unsigned int* m_pBufferEnd;
	const unsigned int* m_pData;

	bf_read() = default;

	bf_read(const void* pData, int nBytes, int nBits = -1) {
		StartReading(pData, nBytes, 0, nBits);
	}

	void StartReading(const void* pData, int nBytes, int iStartBit, int nBits) {
		// Make sure it's dword aligned and padded.
		m_pData = (uint32_t*)pData;
		m_pDataIn = m_pData;
		m_nDataBytes = nBytes;

		if (nBits == -1) {
			m_nDataBits = nBytes << 3;
		}
		else {
			m_nDataBits = nBits;
		}
		m_bOverflow = false;
		m_pBufferEnd = reinterpret_cast<uint32_t const*>(reinterpret_cast<uint8_t const*>(m_pData) + nBytes);
		if (m_pData)
			Seek(iStartBit);
	}

	bool Seek(int nPosition) {
		bool bSucc = true;
		if (nPosition < 0 || nPosition > m_nDataBits) {
			m_bOverflow = true;
			bSucc = false;
			nPosition = m_nDataBits;
		}
		int nHead = m_nDataBytes & 3; // non-multiple-of-4 bytes at head of buffer. We put the "round off"
		// at the head to make reading and detecting the end efficient.

		int nByteOfs = nPosition / 8;
		if ((m_nDataBytes < 4) || (nHead && (nByteOfs < nHead))) {
			// partial first dword
			uint8_t const* pPartial = (uint8_t const*)m_pData;
			if (m_pData) {
				m_nInBufWord = *(pPartial++);
				if (nHead > 1)
					m_nInBufWord |= (*pPartial++) << 8;
				if (nHead > 2)
					m_nInBufWord |= (*pPartial++) << 16;
			}
			m_pDataIn = (uint32_t const*)pPartial;
			m_nInBufWord >>= (nPosition & 31);
			m_nBitsAvail = (nHead << 3) - (nPosition & 31);
		}
		else {
			int nAdjPosition = nPosition - (nHead << 3);
			m_pDataIn = reinterpret_cast<uint32_t const*>(
				reinterpret_cast<uint8_t const*>(m_pData) + ((nAdjPosition / 32) << 2) + nHead);
			if (m_pData) {
				m_nBitsAvail = 32;
				GrabNextDWord();
			}
			else {
				m_nInBufWord = 0;
				m_nBitsAvail = 1;
			}
			m_nInBufWord >>= (nAdjPosition & 31);
			m_nBitsAvail = std::min(m_nBitsAvail, 32 - (nAdjPosition & 31)); // in case grabnextdword overflowed
		}
		return bSucc;
	}

	FORCEINLINE void GrabNextDWord(bool bOverFlowImmediately = false) {
		if (m_pDataIn == m_pBufferEnd) {
			m_nBitsAvail = 1; // so that next read will run out of words
			m_nInBufWord = 0;
			m_pDataIn++; // so seek count increments like old
			if (bOverFlowImmediately)
				m_bOverflow = true;
		}
		else if (m_pDataIn > m_pBufferEnd) {
			m_bOverflow = true;
			m_nInBufWord = 0;
		}
		else {
			m_nInBufWord = LittleDWord(*(m_pDataIn++));
		}
	}
};

class bf_write {
public:
	unsigned char* m_pData;
	int m_nDataBytes;
	int m_nDataBits;
	int m_iCurBit;
	bool m_bOverflow;
	bool m_bAssertOnOverflow;
	const char* m_pDebugName;

	void StartWriting(void* pData, int nBytes, int iStartBit = 0, int nBits = -1) {
		// Make sure it's dword aligned and padded.
		// The writing code will overrun the end of the buffer if it isn't dword aligned, so truncate to force alignment
		nBytes &= ~3;

		m_pData = (unsigned char*)pData;
		m_nDataBytes = nBytes;

		if (nBits == -1) {
			m_nDataBits = nBytes << 3;
		}
		else {
			m_nDataBits = nBits;
		}

		m_iCurBit = iStartBit;
		m_bOverflow = false;
	}

	bf_write() {
		m_pData = NULL;
		m_nDataBytes = 0;
		m_nDataBits = -1; // set to -1 so we generate overflow on any operation
		m_iCurBit = 0;
		m_bOverflow = false;
		m_bAssertOnOverflow = true;
		m_pDebugName = NULL;
	}

	// nMaxBits can be used as the number of bits in the buffer.
	// It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
	bf_write(void* pData, int nBytes, int nBits = -1) {
		m_bAssertOnOverflow = true;
		m_pDebugName = NULL;
		StartWriting(pData, nBytes, 0, nBits);
	}

	bf_write(const char* pDebugName, void* pData, int nBytes, int nBits = -1) {
		m_bAssertOnOverflow = true;
		m_pDebugName = pDebugName;
		StartWriting(pData, nBytes, 0, nBits);
	}
};

class INetMessage {
public:
	virtual ~INetMessage() {};

	// Use these to setup who can hear whose voice.
	// Pass in client indices (which are their ent indices - 1).

	virtual void SetNetChannel(INetChannel* netchan) = 0; // netchannel this message is from/for
	virtual void SetReliable(bool state) = 0;             // set to true if it's a reliable message

	virtual bool Process(void) = 0; // calles the recently set handler to process this message

	virtual bool ReadFromBuffer(bf_read& buffer) = 0; // returns true if parsing was OK
	virtual bool WriteToBuffer(bf_write& buffer) = 0; // returns true if writing was OK

	virtual bool IsReliable(void) const = 0; // true, if message needs reliable handling

	virtual int GetType(void) const = 0;         // returns module specific header tag eg svc_serverinfo
	virtual int GetGroup(void) const = 0;        // returns net message group of this message
	virtual const char* GetName(void) const = 0; // returns network message name, eg "svc_serverinfo"
	virtual INetChannel* GetNetChannel(void) const = 0;
	virtual const char* ToString(void) const = 0; // returns a human readable string about message content
};

class Hooks {
public:
	void init();

public:
	// forward declarations
	class IRecipientFilter;

	// prototypes.
	using PaintTraverse_t = void(__thiscall*)(void*, VPANEL, bool, bool);
	using DoPostScreenSpaceEffects_t = bool(__thiscall*)(void*, CViewSetup*);
	using CreateMove_t = bool(__thiscall*)(void*, float, CUserCmd*);
	using LevelInitPostEntity_t = void(__thiscall*)(void*);
	using LevelShutdown_t = void(__thiscall*)(void*);
	using LevelInitPreEntity_t = void(__thiscall*)(void*, const char*);
	using IN_KeyEvent_t = int(__thiscall*)(void*, int, int, const char*);
	using FrameStageNotify_t = void(__thiscall*)(void*, Stage_t);
	using UpdateClientSideAnimation_t = void(__thiscall*)(void*);
	using GetActiveWeapon_t = Weapon * (__thiscall*)(void*);
	using DoExtraBoneProcessing_t = void(__thiscall*)(void*, int, int, int, int, int, int);
	using BuildTransformations_t = void(__thiscall*)(void*, CStudioHdr*, int, int, int, int, int);
	using CalcViewModelView_t = void(__thiscall*)(void*, vec3_t&, ang_t&);
	using InPrediction_t = bool(__thiscall*)(void*);
	using OverrideView_t = void(__thiscall*)(void*, CViewSetup*);
	using LockCursor_t = void(__thiscall*)(void*);
	using RunCommand_t = void(__thiscall*)(void*, Entity*, CUserCmd*, IMoveHelper*);
	using ProcessPacket_t = void(__thiscall*)(void*, void*, bool);
	using EntityShouldInterpolate_t = bool(__thiscall*)(void*);
	using SendNetMsgFn = bool(__thiscall*)(INetChannel* pNetChan, INetMessage& msg, bool bForceReliable, bool bVoice);
	using SendDatagram_t = int(__thiscall*)(void*, void*);
	// using CanPacket_t                = bool( __thiscall* )( void* );
	using PlaySound_t = void(__thiscall*)(void*, const char*);
	using GetScreenSize_t = void(__thiscall*)(void*, int&, int&);
	using Push3DView_t = void(__thiscall*)(void*, CViewSetup&, int, void*, void*);
	using SceneEnd_t = void(__thiscall*)(void*);
	using DrawModelExecute_t = void(__thiscall*)(void*, uintptr_t, const DrawModelState_t&, const ModelRenderInfo_t&, matrix3x4_t*);
	using ComputeShadowDepthTextures_t = void(__thiscall*)(void*, const CViewSetup&, bool);
	using GetInt_t = int(__thiscall*)(void*);
	using GetBool_t = bool(__thiscall*)(void*);
	using IsConnected_t = bool(__thiscall*)(void*);
	using IsHLTV_t = bool(__thiscall*)(void*);
	using OnEntityCreated_t = void(__thiscall*)(void*, Entity*);
	using OnEntityDeleted_t = void(__thiscall*)(void*, Entity*);
	using RenderSmokeOverlay_t = void(__thiscall*)(void*, bool);
	using ShouldDrawFog_t = bool(__thiscall*)(void*);
	using ShouldDrawParticles_t = bool(__thiscall*)(void*);
	using Render2DEffectsPostHUD_t = void(__thiscall*)(void*, const CViewSetup&);
	using OnRenderStart_t = void(__thiscall*)(void*);
	using RenderView_t = void(__thiscall*)(void*, const CViewSetup&, const CViewSetup&, int, int);
	using GetMatchSession_t = CMatchSessionOnlineHost * (__thiscall*)(void*);
	using OnScreenSizeChanged_t = void(__thiscall*)(void*, int, int);
	using IsPaused_t = bool(__thiscall*)(void*);
	using CalcView_t = void(__thiscall*)(void*, vec3_t&, vec3_t&, float&, float&, float&);
	using OverrideConfig_t = bool(__thiscall*)(void*, MaterialSystem_Config_t*, bool);
	using PostDataUpdate_t = void(__thiscall*)(void*, DataUpdateType_t);
	using TempEntities_t = bool(__thiscall*)(void*, void*);
	using EmitSound_t = void(__thiscall*)(void*, IRecipientFilter&, int, int, const char*, unsigned int, const char*, float, float, int, int, int, const vec3_t*, const vec3_t*, void*, bool, float, int);
	// using PreDataUpdate_t            = void( __thiscall* )( void*, DataUpdateType_t );
	using StandardBlendingRules_t = void(__thiscall*)(void*, int, int, int, int, int);
	using PacketStart_t = void(__thiscall*)(void*, int, int);
public:
	void                     PacketStart(int incoming_sequence, int outgoing_acknowledged);
	void                     CalcView(vec3_t& eye_origin, vec3_t& eye_angles, float& z_near, float& z_far, float& fov);
	bool                     TempEntities(void* msg);
	void                     PaintTraverse(VPANEL panel, bool repaint, bool force);
	bool                     DoPostScreenSpaceEffects(CViewSetup* setup);
	bool                     CreateMove(float input_sample_time, CUserCmd* cmd);
	void                     LevelInitPostEntity();
	void                     LevelShutdown();
	//int                      IN_KeyEvent( int event, int key, const char* bind );
	void                     LevelInitPreEntity(const char* map);
	void                     FrameStageNotify(Stage_t stage);
	void                     UpdateClientSideAnimation();
	Weapon* GetActiveWeapon();
	bool                     EntityShouldInterpolate();
	bool                     InPrediction();
	bool                     ShouldDrawParticles();
	bool                     ShouldDrawFog();
	void                     OverrideView(CViewSetup* view);
	bool                     IsPaused();
	void                     LockCursor();
	void                     PlaySound(const char* name);
	void                     OnScreenSizeChanged(int oldwidth, int oldheight);
	void                     RunCommand(Entity* ent, CUserCmd* cmd, IMoveHelper* movehelper);
	bool                     SendNetMsg(INetChannel* pNetChan, void* edx, INetMessage& msg, bool bForceReliable, bool bVoice);
	int                      SendDatagram(void* data);
	void                     ProcessPacket(void* packet, bool header);
	//void                     GetScreenSize( int& w, int& h );
	void                     SceneEnd();
	void                     DrawModelExecute(uintptr_t ctx, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone);
	void                     ComputeShadowDepthTextures(const CViewSetup& view, bool unk);
	int                      DebugSpreadGetInt();
	bool                     NetShowFragmentsGetBool();
	void                     DoExtraBoneProcessing(int a2, int a3, int a4, int a5, int a6, int a7);
	void                     BuildTransformations(CStudioHdr* hdr, int a3, int a4, int a5, int a6, int a7);
	bool                     IsConnected();
	bool                     IsHLTV();
	void                     EmitSound(IRecipientFilter& filter, int iEntIndex, int iChannel, const char* pSoundEntry, unsigned int nSoundEntryHash, const char* pSample, float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, const vec3_t* pOrigin, const vec3_t* pDirection, void* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity);
	void                     RenderSmokeOverlay(bool unk);
	void                     OnRenderStart();
	void                     RenderView(const CViewSetup& view, const CViewSetup& hud_view, int clear_flags, int what_to_draw);
	void                     Render2DEffectsPostHUD(const CViewSetup& setup);
	CMatchSessionOnlineHost* GetMatchSession();
	bool                     OverrideConfig(MaterialSystem_Config_t* config, bool update);
	void                     PostDataUpdate(DataUpdateType_t type);
	void                     StandardBlendingRules(int a2, int a3, int a4, int a5, int a6);
	static LRESULT WINAPI WndProc(HWND wnd, uint32_t msg, WPARAM wp, LPARAM lp);

public:
	// vmts.
	VMT m_panel;
	VMT m_client_mode;
	VMT m_client;
	VMT m_client_state;
	VMT m_engine;
	VMT m_engine_sound;
	VMT m_prediction;
	VMT m_surface;
	VMT m_render;
	VMT m_net_channel;
	VMT m_render_view;
	VMT m_model_render;
	VMT m_shadow_mgr;
	VMT m_view_render;
	VMT m_match_framework;
	VMT m_material_system;
	VMT m_fire_bullets;
	VMT m_net_show_fragments;

	// player shit.
	std::array< VMT, 64 > m_player;

	// cvars
	VMT m_debug_spread;

	// wndproc old ptr.
	WNDPROC m_old_wndproc;

	// old player create fn.
	DoExtraBoneProcessing_t     m_DoExtraBoneProcessing;
	EntityShouldInterpolate_t   m_EntityShouldInterpolate;
	UpdateClientSideAnimation_t m_UpdateClientSideAnimation;
	GetActiveWeapon_t           m_GetActiveWeapon;
	BuildTransformations_t      m_BuildTransformations;
	CalcView_t                  m_1CalcView;
	StandardBlendingRules_t     m_StandardBlendingRules;

	// netvar proxies.
	RecvVarProxy_t m_Pitch_original;
	RecvVarProxy_t m_Body_original;
	RecvVarProxy_t m_Force_original;
	RecvVarProxy_t m_AbsYaw_original;
};

// note - dex; these are defined in player.cpp.
class CustomEntityListener : public IEntityListener {
public:
	virtual void OnEntityCreated(Entity* ent);
	virtual void OnEntityDeleted(Entity* ent);

	__forceinline void init() {
		g_csgo.AddListenerEntity(this);
	}
};

extern Hooks                g_hooks;
extern CustomEntityListener g_custom_entity_listener;