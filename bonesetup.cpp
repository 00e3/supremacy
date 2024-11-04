#include "includes.h"

Bones g_bones{};;

bool Bones::setup(Player* player, BoneArray* out, LagRecord* record) {
	// if the record isnt setup yet.
	if (!record->m_setup) {
		// run setupbones rebuilt.
		if (!BuildBones(player, 0x7FF00, record->m_bones, record))
			return false;

		// we have setup this record bones.
		record->m_setup = true;
	}

	// record is setup.
	if (out && record->m_setup)
		std::memcpy(out, record->m_bones, sizeof(BoneArray) * 128);

	return true;
}

bool Bones::BuildBones(Player* target, int mask, BoneArray* out, LagRecord* record) {
	vec3_t		     pos[128];
	quaternion_t     q[128];
	vec3_t           backup_origin;
	ang_t            backup_angles;
	float            backup_poses[24];
	C_AnimationLayer backup_layers[13];

	// get hdr.
	CStudioHdr* hdr = target->GetModelPtr();
	if (!hdr)
		return false;

	// get ptr to bone accessor.
	CBoneAccessor* accessor = &target->m_BoneAccessor();
	if (!accessor)
		return false;

	// store origial output matrix.
	// likely cachedbonedata.
	BoneArray* backup_matrix = accessor->m_pBones;
	if (!backup_matrix)
		return false;

	// prevent the game from calling ShouldSkipAnimationFrame.
	auto bSkipAnimationFrame = *reinterpret_cast<int*>(uintptr_t(target) + 0x260);
	*reinterpret_cast<int*>(uintptr_t(target) + 0x260) = NULL;

	// backup original.
	backup_origin = target->GetAbsOrigin();
	backup_angles = target->GetAbsAngles();
	target->GetPoseParameters(backup_poses);
	target->GetAnimLayers(backup_layers);

	// compute transform from raw data.
	matrix3x4_t transform;
	math::AngleMatrix(record->m_abs_ang, record->m_pred_origin, transform);

	// set non interpolated data
	target->AddEffect(EF_NOINTERP);
	target->SetAbsOrigin(record->m_pred_origin);
	target->SetAbsAngles(record->m_abs_ang);
	target->SetPoseParameters(record->m_poses);
	target->SetAnimLayers(record->m_layers);

	// force game to call AccumulateLayers - pvs fix.
	m_running = true;

	// set bone array for write.
	accessor->m_pBones = out;

	// compute and build bones.
	target->StandardBlendingRules(hdr, pos, q, record->m_pred_time, mask);

	uint8_t computed[0x100];
	std::memset(computed, 0, 0x100);
	target->BuildTransformations(hdr, pos, q, transform, mask, computed);

	// restore old matrix.
	accessor->m_pBones = backup_matrix;

	// restore original interpolated entity data.
	target->SetAbsOrigin(backup_origin);
	target->SetAbsAngles(backup_angles);
	target->SetPoseParameters(backup_poses);
	target->SetAnimLayers(backup_layers);

	// revert to old game behavior.
	m_running = false;

	// allow the game to call ShouldSkipAnimationFrame.
	*reinterpret_cast<int*>(uintptr_t(target) + 0x260) = bSkipAnimationFrame;

	return true;
}

bool Bones::SetupBones(Player* player, matrix3x4_t* world, int max, int mask, float curtime, LagRecord* record)
{
	m_running = true;
	C_AnimationLayer backup[13];
	player->GetAnimLayers(backup);

	player->InvalidateBoneCache();
	vec3_t origin_backup = player->GetAbsOrigin();

	const auto m_pIk = player->m_pIK();
	const auto client_ent_flags = player->m_ClientEntEffects();
	const auto effects = player->m_fEffects();
	const auto animlod = player->m_nAnimLODflags();
	const auto jigglebones = player->m_bIsJiggleBonesEnabled();

	const auto curtime_backup = g_csgo.m_globals->m_curtime;
	const auto frametime = g_csgo.m_globals->m_frametime;

	g_csgo.m_globals->m_curtime = curtime;
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;

	if (record) {
		player->SetAnimLayers(record->m_layers);
		player->SetAbsOrigin(record->m_origin);
	}

	player->m_pIK() = 0;
	player->m_BoneAccessor().m_ReadableBones = 0;
	player->m_iMostRecentModelBoneCounter() = 0;
	player->m_flLastBoneSetupTime() = FLT_MAX;
	player->m_nAnimLODflags() &= ~2u; //flag: ANIMLODFLAG_OUTSIDEVIEWFRUSTUM
	player->m_nCustomBlendingRuleMask() = -1;
	player->m_ClientEntEffects() |= 2u; //flag: NO_IK
	player->m_fEffects() |= EF_NOINTERP;
	player->m_bIsJiggleBonesEnabled() = false;

	auto& boneSnapshot1 = *(float*)(uintptr_t(player) + 0x39F0 + 4);
	auto& boneSnapshot2 = *(float*)(uintptr_t(player) + 0x6E40 + 4);

	auto bk_snapshot1 = &boneSnapshot1;
	auto bk_snapshot2 = &boneSnapshot2;

	boneSnapshot1 = 0.0f;
	boneSnapshot2 = 0.0f;

	//g_hooks.m_updating_bones[player->index()] = true;
	auto bone_result = player->SetupBones(world, max, mask, curtime);
	//g_hooks.m_updating_bones[player->index()] = false;

	boneSnapshot1 = *bk_snapshot1;
	boneSnapshot2 = *bk_snapshot2;

	player->m_pIK() = m_pIk;
	player->m_fEffects() = effects;
	player->m_ClientEntEffects() = client_ent_flags;
	player->m_nAnimLODflags() = animlod;

	if (record) {
		player->SetAnimLayers(backup);
		player->SetAbsOrigin(origin_backup);
	}

	g_csgo.m_globals->m_curtime = curtime_backup;
	g_csgo.m_globals->m_frametime = frametime;
	m_running = false;

	return bone_result;
}

bool Bones::BuildLocalBones(Player* player, int mask, matrix3x4_t* out, float time)
{
	player->m_fEffects() &= ~8u;
	const auto m_pIk = player->m_pIK();
	const auto client_ent_flags = player->m_ClientEntEffects();
	const auto effects = player->m_fEffects();
	const auto animlod = player->m_nAnimLODflags();

	player->m_pIK() = 0;
	player->m_BoneAccessor().m_ReadableBones = 0;
	player->m_iMostRecentModelBoneCounter() = 0;
	player->m_nLastNonSkippedFrame() = 0;
	player->m_nAnimLODflags() &= ~2u; //flag: ANIMLODFLAG_OUTSIDEVIEWFRUSTUM
	player->m_nCustomBlendingRuleMask() = -1;
	player->m_ClientEntEffects() |= 2u; //flag: NO_IK
	player->m_fEffects() |= 8u;

	float prev_layer = -1;

	if (player->m_AnimOverlayCount() >= 12) {
		prev_layer = player->m_AnimOverlay()[12].m_weight;
		player->m_AnimOverlay()[12].m_weight = 0;
	}

	const auto pastedframe = g_csgo.m_globals->m_frame;

	g_csgo.m_globals->m_frame = -999;

	auto boneSnapshot1 = (float*)(uintptr_t(this) + (0x3AD0 + 4));
	auto boneSnapshot2 = (float*)(uintptr_t(this) + (0x6F20 + 4));

	auto bk_snapshot1 = *boneSnapshot1;
	auto bk_snapshot2 = *boneSnapshot2;

	*boneSnapshot1 = 0.0f;
	*boneSnapshot2 = 0.0f;

	auto bones = player->SetupBones(out, 128, mask, time);

	g_csgo.m_globals->m_frame = pastedframe;
	*boneSnapshot1 = bk_snapshot1;
	*boneSnapshot2 = bk_snapshot2;

	if (player->m_AnimOverlayCount() >= 12 && prev_layer >= 0.f) {
		player->m_AnimOverlay()[12].m_weight = prev_layer;
	}

	player->m_pIK() = m_pIk;
	player->m_fEffects() = effects;
	player->m_ClientEntEffects() = client_ent_flags;
	player->m_nAnimLODflags() = animlod;

	return bones;
}

bool Bones::SetupBonesRebuild(Player* entity, matrix3x4_t* pBoneMatrix, int nBoneCount, int boneMask, float time, int flags) {

	if (entity->m_nSequence() == -1)
		return false;
}
