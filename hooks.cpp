#include "includes.h"

Hooks                g_hooks{ };;
CustomEntityListener g_custom_entity_listener{ };;

void Pitch_proxy( CRecvProxyData *data, Address ptr, Address out ) {
	// normalize this fucker.
	math::NormalizeAngle( data->m_Value.m_Float );

	// clamp to remove retardedness.
	math::clamp( data->m_Value.m_Float, -90.f, 90.f );

	// call original netvar proxy.
	if ( g_hooks.m_Pitch_original )
		g_hooks.m_Pitch_original( data, ptr, out );
}

void Body_proxy( CRecvProxyData *data, Address ptr, Address out ) {
	Stack stack;

	static Address RecvTable_Decode{ pattern::find( g_csgo.m_engine_dll, XOR( "EB 0D FF 77 10" ) ) };

	// call from entity going into pvs.
	if ( stack.next( ).next( ).ReturnAddress( ) != RecvTable_Decode ) {
		// convert to player.
		Player *player = ptr.as< Player * >( );

		// store data about the update.
		g_resolver.OnBodyUpdate( player, data->m_Value.m_Float );
	}

	// call original proxy.
	if ( g_hooks.m_Body_original )
		g_hooks.m_Body_original( data, ptr, out );
}

void AbsYaw_proxy( CRecvProxyData *data, Address ptr, Address out ) {
	// convert to ragdoll.
	//Ragdoll* ragdoll = ptr.as< Ragdoll* >( );

	// get ragdoll owner.
	//Player* player = ragdoll->GetPlayer( );

	// get data for this player.
	/*AimPlayer* aim = &g_aimbot.m_players[ player->index( ) - 1 ];

	if( player && aim ) {
	if( !aim->m_records.empty( ) ) {
	LagRecord* match{ nullptr };

	// iterate records.
	for( const auto &it : aim->m_records ) {
	// find record that matches with simulation time.
	if( it->m_sim_time == player->m_flSimulationTime( ) ) {
	match = it.get( );
	break;
	}
	}

	// we have a match.
	// and it is standing
	// TODO; add air?
	if( match /*&& match->m_mode == Resolver::Modes::RESOLVE_STAND*/// ) {
	/*	RagdollRecord record;
	record.m_record   = match;
	record.m_rotation = math::NormalizedAngle( data->m_Value.m_Float );
	record.m_delta    = math::NormalizedAngle( record.m_rotation - match->m_lbyt );

	float death = math::NormalizedAngle( ragdoll->m_flDeathYaw( ) );

	// store.
	//aim->m_ragdoll.push_front( record );

	//g_cl.print( tfm::format( XOR( "rot %f death %f delta %f\n" ), record.m_rotation, death, record.m_delta ).data( ) );
	}
	}*/
	//}

	// call original netvar proxy.
	if ( g_hooks.m_AbsYaw_original )
		g_hooks.m_AbsYaw_original( data, ptr, out );
}

void Force_proxy( CRecvProxyData *data, Address ptr, Address out ) {
	// convert to ragdoll.
	Ragdoll *ragdoll = ptr.as< Ragdoll * >( );

	// get ragdoll owner.
	Player *player = ragdoll->GetPlayer( );

	// we only want this happening to noobs we kill.
	//if ( g_menu.main.misc.ragdoll_force.get( ) && g_cl.m_local && player && player->enemy( g_cl.m_local ) ) {
	//	// get m_vecForce.
	//	vec3_t vel = { data->m_Value.m_Vector[ 0 ], data->m_Value.m_Vector[ 1 ], data->m_Value.m_Vector[ 2 ] };

	//	// give some speed to all directions.
	//	vel *= 1000.f;

	//	// boost z up a bit.
	//	if ( vel.z <= 1.f )
	//		vel.z = 2.f;

	//	vel.z *= 2.f;

	//	// don't want crazy values for this... probably unlikely though?
	//	math::clamp( vel.x, std::numeric_limits< float >::lowest( ), std::numeric_limits< float >::max( ) );
	//	math::clamp( vel.y, std::numeric_limits< float >::lowest( ), std::numeric_limits< float >::max( ) );
	//	math::clamp( vel.z, std::numeric_limits< float >::lowest( ), std::numeric_limits< float >::max( ) );

	//	// set new velocity.
	//	data->m_Value.m_Vector[ 0 ] = vel.x;
	//	data->m_Value.m_Vector[ 1 ] = vel.y;
	//	data->m_Value.m_Vector[ 2 ] = vel.z;
	//}

	if ( g_hooks.m_Force_original )
		g_hooks.m_Force_original( data, ptr, out );
}

using FnShouldSkipAnimationFrame = bool(__fastcall*)(void*, uint32_t*);
FnShouldSkipAnimationFrame oShouldSkipAnimationFrame;
bool __fastcall hkShouldSkipAnimationFrame(void* ecx, uint32_t* wow) {
	oShouldSkipAnimationFrame(ecx, wow);
	return false;
}

using FnGetExposureRange = void(__fastcall*)(float*, float*);
FnGetExposureRange oGetExposureRange;
void __fastcall hkGetExposureRange(float* pflAutoExposureMin, float* pflAutoExposureMax) {
	oGetExposureRange(pflAutoExposureMin, pflAutoExposureMax);
	*pflAutoExposureMin = 1.f;
	*pflAutoExposureMax = 1.f;
}

using FnListInLeavesBox = int(__fastcall*)(const std::uintptr_t, const std::uintptr_t, const vec3_t&, const vec3_t&, const uint16_t* const, const int);
FnListInLeavesBox oListInLeavesBox;


int __fastcall list_leaves_in_box(const std::uintptr_t ecx, const std::uintptr_t edx, const vec3_t& mins, const vec3_t& maxs, const uint16_t* const list, const int max) {

	if (!g_cl.m_local)
		return oListInLeavesBox(ecx, edx, mins, maxs, list, max);

	if (*(uint32_t*)_ReturnAddress() != 0x8B087D8B)
		return oListInLeavesBox(ecx, edx, mins, maxs, list, max);

	struct renderable_info_t {
		IClientRenderable* m_renderable{};
		std::uintptr_t	m_alpha_property{};
		int				m_enum_count{};
		int				m_render_frame{};
		std::uint16_t	m_first_shadow{};
		std::uint16_t	m_leaf_list{};
		short			m_area{};
		std::uint16_t	m_flags0{};
		std::uint16_t	m_flags1{};
		vec3_t			m_bloated_abs_min{};
		vec3_t			m_bloated_abs_max{};
		vec3_t			m_abs_min{};
		vec3_t			m_abs_max{};
		char			pad0[4u]{};
	};

	const auto info = *reinterpret_cast<renderable_info_t**>(
		reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress()) + 0x14u
		);
	if (!info
		|| !info->m_renderable)
		return oListInLeavesBox(ecx, edx, mins, maxs, list, max);

	const auto entity = info->m_renderable->GetIClientUnknown()->GetBaseEntity();
	if (!entity
		|| !entity->IsPlayer())
		return oListInLeavesBox(ecx, edx, mins, maxs, list, max);

	info->m_flags0 &= ~0x100;
	info->m_flags1 |= 0xC0;

	return oListInLeavesBox(ecx, edx, { -16384.f, -16384.f, -16384.f }, { 16384.f, 16384.f, 16384.f }, list, max);
}

bool __fastcall hkUpdateClientSideAnimation(void* player) {
	bool is_local_player = *(bool*)((uintptr_t)player + 0x35F8);

	if (is_local_player)
		return g_cache.m_functions.UpdateClientSideAnimation(player);

	//// m_flGuardianTooFarDistFrac - 0x38C0
	//ang_t* eye_angles = (ang_t*)((uintptr_t)player + 0xB23C);
	//float networked_yaw = *(float*)((uintptr_t)player + 0x38C0);

	//eye_angles->y = networked_yaw;

	return g_cache.m_functions.UpdateClientSideAnimation(player);
}

void Hooks::init() {
	MH_Initialize();

	const auto dwGetExposureRange = pattern::find(g_csgo.m_client_dll, XOR("55 8B EC 51 80 3D ? ? ? ? ? 0F 57")).as<LPVOID>();
	MH_CreateHook(dwGetExposureRange, hkGetExposureRange, reinterpret_cast<void**>(&oGetExposureRange));

	const auto dwShouldSkipAnimationFrame = (DWORD)pattern::find(g_csgo.m_client_dll, XOR("E8 ? ? ? ? 88 44 24 0B")).rel32(1);
	MH_CreateHook((byte*)dwShouldSkipAnimationFrame, hkShouldSkipAnimationFrame, reinterpret_cast<void**>(&oShouldSkipAnimationFrame));

	MH_EnableHook(MH_ALL_HOOKS);

	// hook wndproc.
	m_old_wndproc = (WNDPROC)g_winapi.SetWindowLongA(g_csgo.m_game->m_hWindow, GWL_WNDPROC, util::force_cast<LONG>(Hooks::WndProc));

	// setup normal VMT hooks.
	m_panel.init(g_csgo.m_panel);
	m_panel.add(IPanel::PAINTTRAVERSE, util::force_cast(&Hooks::PaintTraverse));

	m_client.init(g_csgo.m_client);
	m_client.add(CHLClient::LEVELINITPREENTITY, util::force_cast(&Hooks::LevelInitPreEntity));
	m_client.add(CHLClient::LEVELINITPOSTENTITY, util::force_cast(&Hooks::LevelInitPostEntity));
	m_client.add(CHLClient::LEVELSHUTDOWN, util::force_cast(&Hooks::LevelShutdown));
	m_client.add(CHLClient::FRAMESTAGENOTIFY, util::force_cast(&Hooks::FrameStageNotify));

	m_engine.init(g_csgo.m_engine);
	m_engine.add(IVEngineClient::ISCONNECTED, util::force_cast(&Hooks::IsConnected));
	m_engine.add(IVEngineClient::ISHLTV, util::force_cast(&Hooks::IsHLTV));
	m_engine.add(IVEngineClient::ISPAUSED, util::force_cast(&Hooks::IsPaused));

	m_prediction.init(g_csgo.m_prediction);
	m_prediction.add(CPrediction::INPREDICTION, util::force_cast(&Hooks::InPrediction));
	m_prediction.add(CPrediction::RUNCOMMAND, util::force_cast(&Hooks::RunCommand));

	m_client_mode.init(g_csgo.m_client_mode);
	m_client_mode.add(IClientMode::SHOULDDRAWPARTICLES, util::force_cast(&Hooks::ShouldDrawParticles));
	m_client_mode.add(IClientMode::SHOULDDRAWFOG, util::force_cast(&Hooks::ShouldDrawFog));
	m_client_mode.add(IClientMode::OVERRIDEVIEW, util::force_cast(&Hooks::OverrideView));
	m_client_mode.add(IClientMode::CREATEMOVE, util::force_cast(&Hooks::CreateMove));
	m_client_mode.add(IClientMode::DOPOSTSPACESCREENEFFECTS, util::force_cast(&Hooks::DoPostScreenSpaceEffects));

	m_surface.init(g_csgo.m_surface);
	m_surface.add(ISurface::LOCKCURSOR, util::force_cast(&Hooks::LockCursor));
	m_surface.add(ISurface::PLAYSOUND, util::force_cast(&Hooks::PlaySound));
	m_surface.add(ISurface::ONSCREENSIZECHANGED, util::force_cast(&Hooks::OnScreenSizeChanged));

	m_model_render.init(g_csgo.m_model_render);
	m_model_render.add(IVModelRender::DRAWMODELEXECUTE, util::force_cast(&Hooks::DrawModelExecute));

	m_render_view.init(g_csgo.m_render_view);
	m_render_view.add(IVRenderView::SCENEEND, util::force_cast(&Hooks::SceneEnd));

	m_shadow_mgr.init(g_csgo.m_shadow_mgr);
	m_shadow_mgr.add(IClientShadowMgr::COMPUTESHADOWDEPTHTEXTURES, util::force_cast(&Hooks::ComputeShadowDepthTextures));

	m_view_render.init(g_csgo.m_view_render);
	m_view_render.add(CViewRender::ONRENDERSTART, util::force_cast(&Hooks::OnRenderStart));
	m_view_render.add(CViewRender::RENDERVIEW, util::force_cast(&Hooks::RenderView));
	m_view_render.add(CViewRender::RENDER2DEFFECTSPOSTHUD, util::force_cast(&Hooks::Render2DEffectsPostHUD));
	m_view_render.add(CViewRender::RENDERSMOKEOVERLAY, util::force_cast(&Hooks::RenderSmokeOverlay));

	m_match_framework.init(g_csgo.m_match_framework);
	m_match_framework.add(CMatchFramework::GETMATCHSESSION, util::force_cast(&Hooks::GetMatchSession));

	m_material_system.init(g_csgo.m_material_system);
	m_material_system.add(IMaterialSystem::OVERRIDECONFIG, util::force_cast(&Hooks::OverrideConfig));

	m_fire_bullets.init(g_csgo.TEFireBullets);
	m_fire_bullets.add(7, util::force_cast(&Hooks::PostDataUpdate));

	m_client_state.init(g_csgo.m_hookable_cl);
	m_client_state.add(CClientState::TEMPENTITIES, util::force_cast(&Hooks::TempEntities));
	//m_client_state.add(CClientState::PACKETSTART, util::force_cast(&Hooks::PacketStart));

	g_custom_entity_listener.init();

	// cvar hooks.
	m_debug_spread.init(g_csgo.weapon_debug_spread_show);
	m_debug_spread.add(ConVar::GETINT, util::force_cast(&Hooks::DebugSpreadGetInt));

	m_net_show_fragments.init(g_csgo.net_showfragments);
	m_net_show_fragments.add(ConVar::GETBOOL, util::force_cast(&Hooks::NetShowFragmentsGetBool));

	// set netvar proxies.
	g_netvars.SetProxy(HASH("DT_CSPlayer"), HASH("m_angEyeAngles[0]"), Pitch_proxy, m_Pitch_original);
	g_netvars.SetProxy(HASH("DT_CSPlayer"), HASH("m_flLowerBodyYawTarget"), Body_proxy, m_Body_original);
	g_netvars.SetProxy(HASH("DT_CSRagdoll"), HASH("m_vecForce"), Force_proxy, m_Force_original);
	g_netvars.SetProxy(HASH("DT_CSRagdoll"), HASH("m_flAbsYaw"), AbsYaw_proxy, m_AbsYaw_original);

	// minhook.
	MH_Initialize();
	auto dwListLeavesInBox = (void*)util::GetVFunc(g_csgo.m_engine->GetBSPTreeQuery(), 6);
	MH_CreateHook(dwListLeavesInBox, list_leaves_in_box, (void**)(&oListInLeavesBox));
	MH_CreateHook(g_cache.m_functions.UpdateClientSideAnimation, hkUpdateClientSideAnimation, (LPVOID*)(&g_cache.m_functions.UpdateClientSideAnimation));
	MH_EnableHook(MH_ALL_HOOKS);
}