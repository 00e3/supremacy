#include "includes.h"

void Hooks::DoExtraBoneProcessing(int a2, int a3, int a4, int a5, int a6, int a7) {

	// the server doesn't use this so let's not use it aswell.
	return;
}

void Hooks::StandardBlendingRules(int a2, int a3, int a4, int a5, int a6)
{
	// cast thisptr to player ptr.
	Player* player = (Player*)this;

	if (!player
		|| !player->IsPlayer()
		|| !player->alive())
		return g_hooks.m_StandardBlendingRules(this, a2, a3, a4, a5, a6);

	if (player->index() == g_cl.m_local->index())
		a6 |= BONE_USED_BY_BONE_MERGE;
	else // fix arms. @ruka: DONT DO THIS ON LOCAL
		a6 = BONE_USED_BY_SERVER;

	if (player->index() != g_cl.m_local->index())
		player->m_fEffects() |= EF_NOINTERP;
	else 	player->m_fEffects() &= ~EF_NOINTERP;

	g_hooks.m_StandardBlendingRules(player, a2, a3, a4, a5, a6);

}

void Hooks::BuildTransformations(CStudioHdr* hdr, int a3, int a4, int a5, int a6, int a7) {
	// cast thisptr to player ptr.
	Player* player = (Player*)this;

	// backup bone flags.
	const auto bone_flags = hdr->m_boneFlags;

	// stop procedural animations.
	for (auto i = 0; i < hdr->m_boneFlags.Count(); i++)
		hdr->m_boneFlags.Element(i) &= ~0x04; // BONE_ALWAYS_PROCEDURAL

	// call og.
	g_hooks.m_BuildTransformations(this, hdr, a3, a4, a5, a6, a7);

	// restore bone flags.
	hdr->m_boneFlags = bone_flags;
}

void Hooks::CalcView(vec3_t& eye_origin, vec3_t& eye_angles, float& z_near, float& z_far, float& fov)
{
	// cast thisptr to player ptr.
	Player* player = (Player*)this;

	const auto use_new_anim_state = *(int*)((uintptr_t)player + 0x39E1);

	*(bool*)((uintptr_t)player + 0x39E1) = false;

	g_hooks.m_1CalcView(this, eye_origin, eye_angles, z_near, z_far, fov);

	*(bool*)((uintptr_t)player + 0x39E1) = use_new_anim_state;
}

void Hooks::UpdateClientSideAnimation() {
	Player* player = (Player*)this;

	if (!g_cl.m_processing)
		return g_hooks.m_UpdateClientSideAnimation(this);

	if (!player
		|| !player->m_bIsLocalPlayer()
		|| !player->alive()
		|| !player->IsPlayer()
		|| !player->m_iHealth()
		|| !g_cl.m_processing
		|| !g_cl.m_local)
		return g_hooks.m_UpdateClientSideAnimation(this);

	if (!g_cl.m_poses)
		return g_hooks.m_UpdateClientSideAnimation(this);

	if (!g_cl.m_layers)
		return g_hooks.m_UpdateClientSideAnimation(this);

	player->RemoveEffect(EF_NOINTERP);

	C_AnimationLayer backuplayers[13];
	ang_t old_angles = player->GetAbsAngles();
	float   m_backup_poses[24]{};

	player->GetAnimLayers(backuplayers);
	player->GetPoseParameters(m_backup_poses);

	player->SetPoseParameters(g_cl.m_poses);
	player->SetAnimLayers(g_cl.m_layers);
	g_cl.SetAngles();

	const int m_pIk = player->m_pIK();
	const int Effect = player->m_fEffects();
	const int ClientEntFlag = player->m_ClientEntEffects();
	const int Animlod = player->m_nAnimLODflags();

	player->InvalidateBoneCache();
	player->m_pIK() = 0;
	player->m_nAnimLODflags() &= ~2u;
	player->m_ClientEntEffects() |= 2u;
	player->m_fEffects() &= ~8u;

	player->SetupBones(0, -1, BONE_USED_BY_ANYTHING, g_csgo.m_globals->m_curtime);

	player->m_pIK() = m_pIk;
	player->m_fEffects() = Effect;
	player->m_ClientEntEffects() = ClientEntFlag;
	player->m_nAnimLODflags() = Animlod;

	player->SetPoseParameters(m_backup_poses);
	player->SetAnimLayers(backuplayers);
}

Weapon* Hooks::GetActiveWeapon() {
	Stack stack;

	static Address ret_1 = pattern::find(g_csgo.m_client_dll, XOR("85 C0 74 1D 8B 88 ? ? ? ? 85 C9"));

	// note - dex; stop call to CIronSightController::RenderScopeEffect inside CViewRender::RenderView.
	if (g_menu.main.visuals.removals.get(4)) {
		if (stack.ReturnAddress() == ret_1)
			return nullptr;
	}

	return g_hooks.m_GetActiveWeapon(this);
}

bool Hooks::EntityShouldInterpolate() {
	// cast thisptr to player ptr.
	Player* player = (Player*)this;

	// let us ensure we aren't disabling interpolation on our local player model.
	if (player == g_cl.m_local)
		return true;

	// lets disable interpolation on these niggas.
	if (player->IsPlayer()) {
		return false;
	}

	// call og.
	g_hooks.m_EntityShouldInterpolate(this);

	return false;
}


void CustomEntityListener::OnEntityCreated(Entity* ent) {
	if (ent) {
		// player created.
		if (ent->IsPlayer()) {
			Player* player = ent->as< Player* >();

			// access out player data stucture and reset player data.
			AimPlayer* data = &g_aimbot.m_players[player->index() - 1];
			if (data)
				data->reset();

			// get ptr to vmt instance and reset tables.
			VMT* vmt = &g_hooks.m_player[player->index() - 1];
			if (vmt) { 
				// init vtable with new ptr.
				vmt->reset();
				vmt->init(player);

				// hook this on every player.
				g_hooks.m_DoExtraBoneProcessing = vmt->add< Hooks::DoExtraBoneProcessing_t >(Player::DOEXTRABONEPROCESSING, util::force_cast(&Hooks::DoExtraBoneProcessing));
				g_hooks.m_StandardBlendingRules = vmt->add< Hooks::StandardBlendingRules_t >(Player::STANDARDBLENDINGRULES, util::force_cast(&Hooks::StandardBlendingRules));
				g_hooks.m_EntityShouldInterpolate = vmt->add< Hooks::EntityShouldInterpolate_t >(Player::ENTITYSHOULDINTERPOLATE, util::force_cast(&Hooks::EntityShouldInterpolate));
				g_hooks.m_BuildTransformations = vmt->add< Hooks::BuildTransformations_t >(Player::BUILDTRANSFORMATIONS, util::force_cast(&Hooks::BuildTransformations));

				// local gets special treatment.
				if (player->index() == g_csgo.m_engine->GetLocalPlayer()) {
					g_hooks.m_UpdateClientSideAnimation = vmt->add< Hooks::UpdateClientSideAnimation_t >(Player::UPDATECLIENTSIDEANIMATION, util::force_cast(&Hooks::UpdateClientSideAnimation));
					g_hooks.m_1CalcView = vmt->add< Hooks::CalcView_t >(270, util::force_cast(&Hooks::CalcView));
					g_hooks.m_GetActiveWeapon           = vmt->add< Hooks::GetActiveWeapon_t >( Player::GETACTIVEWEAPON, util::force_cast( &Hooks::GetActiveWeapon ) );
				}
			}
		}
	}
}

void CustomEntityListener::OnEntityDeleted(Entity* ent) {
	// note; IsPlayer doesn't work here because the ent class is CBaseEntity.
	if (ent && ent->index() >= 1 && ent->index() <= 64) {
		Player* player = ent->as< Player* >();

		// access out player data stucture and reset player data.
		AimPlayer* data = &g_aimbot.m_players[player->index() - 1];
		if (data)
			data->reset();

		// get ptr to vmt instance and reset tables.
		VMT* vmt = &g_hooks.m_player[player->index() - 1];
		if (vmt)
			vmt->reset();
	}
}