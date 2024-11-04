#include "includes.h"

void Hooks::LevelInitPreEntity( const char* map ) {
	float rate{ 1.f / g_csgo.m_globals->m_interval };

	// set rates when joining a server.
	g_csgo.cl_updaterate->SetValue( rate );
	g_csgo.cl_cmdrate->SetValue( rate );

	g_aimbot.reset( );
	g_visuals.m_hit_start = g_visuals.m_hit_end = g_visuals.m_hit_duration = 0.f;

	// invoke original method.
	g_hooks.m_client.GetOldMethod< LevelInitPreEntity_t >( CHLClient::LEVELINITPREENTITY )( this, map );
}

void Hooks::LevelInitPostEntity( ) {
	// note - maxwell; setting these because hooking them does nothing, weird.
	g_csgo.r_DrawSpecificStaticProp->SetValue(0);

	g_cl.OnMapload( );

	// invoke original method.
	g_hooks.m_client.GetOldMethod< LevelInitPostEntity_t >( CHLClient::LEVELINITPOSTENTITY )( this );
}

void Hooks::LevelShutdown( ) {
	g_aimbot.reset( );

	g_cl.m_local       = nullptr;
	g_cl.m_weapon      = nullptr;
	g_cl.m_processing  = false;
	g_cl.m_weapon_info = nullptr;
	g_cl.m_round_end   = false;

	g_cl.m_sequences.clear( );

	// invoke original method.
	g_hooks.m_client.GetOldMethod< LevelShutdown_t >( CHLClient::LEVELSHUTDOWN )( this );
}

void Hooks::FrameStageNotify( Stage_t stage ) {

	// save stage.
	if( stage != FRAME_START )
		g_cl.m_stage = stage;

	// damn son.
	g_cl.m_local = g_csgo.m_entlist->GetClientEntity< Player* >( g_csgo.m_engine->GetLocalPlayer( ) );

	// cache random values cuz valve random system cause performance issues
	if (!g_csgo.m_bInitRandomSeed) {
		for (auto i = 0; i <= 128; i++) {
			g_csgo.RandomSeed(i + 1);

			g_csgo.SpreadRandom[i].flRand1 = g_csgo.RandomFloat(0.0f, 1.0f);
			g_csgo.SpreadRandom[i].flRandPi1 = g_csgo.RandomFloat(0.0f, 6.2831855f);
			g_csgo.SpreadRandom[i].flRand2 = g_csgo.RandomFloat(0.0f, 1.0f);
			g_csgo.SpreadRandom[i].flRandPi2 = g_csgo.RandomFloat(0.0f, 6.2831855f);
		}

		g_csgo.m_bInitRandomSeed = true;
	}

	if( stage == FRAME_RENDER_START ) {	
		// apply local player animation fix.
		g_cl.SetAngles();

        // draw our custom beams.
        //g_visuals.DrawBeams( );

		// draw our bullet impacts.
		g_visuals.ImpactData();

		g_shots.Think();
	}

	// call og.
	g_hooks.m_client.GetOldMethod< FrameStageNotify_t >( CHLClient::FRAMESTAGENOTIFY )( this, stage );

	if( stage == FRAME_RENDER_START ) {
		// ...
	}

	static bool turnedon = false;
	if (g_menu.main.visuals.world.get(2)) {
		Color clr = g_menu.main.visuals.ambiencolor.get();
		turnedon = false;
		if (g_csgo.mat_ambient_light_r->GetFloat() != clr.r())
			g_csgo.mat_ambient_light_r->SetValue(clr.r() / g_menu.main.visuals.ambiencolor.get().a());

		if (g_csgo.mat_ambient_light_g->GetFloat() != clr.g())
			g_csgo.mat_ambient_light_g->SetValue(clr.g() / g_menu.main.visuals.ambiencolor.get().a());

		if (g_csgo.mat_ambient_light_b->GetFloat() != clr.b())
			g_csgo.mat_ambient_light_b->SetValue(clr.b() / g_menu.main.visuals.ambiencolor.get().a());
	}
	else if (!turnedon) {
		g_csgo.mat_ambient_light_r->SetValue(0.f);
		g_csgo.mat_ambient_light_g->SetValue(0.f);
		g_csgo.mat_ambient_light_b->SetValue(0.f);
		turnedon = true;
	}

	else if( stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START ) {
		// restore non-compressed netvars.
		// g_netdata.apply( );

		g_cl.Skybox();
		//g_cl.ClanTag();
		g_skins.think( );
	}

	else if( stage == FRAME_NET_UPDATE_POSTDATAUPDATE_END ) {
		g_visuals.NoSmoke( );
	}

	else if( stage == FRAME_NET_UPDATE_END ) {
        // restore non-compressed netvars.
		g_netdata.apply( );

		// update all players.
		for( int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i ) {
			Player* player = g_csgo.m_entlist->GetClientEntity< Player* >( i );
			if( !player || player->m_bIsLocalPlayer( ) )
				continue;

			player->SetAbsOrigin(player->m_vecOrigin());

			AimPlayer* data = &g_aimbot.m_players[ i - 1 ];
			data->OnNetUpdate( player );
		}
	}
}