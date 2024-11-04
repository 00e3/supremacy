#pragma once

enum HitscanMode : int {
	NORMAL = 0,
	LETHAL = 1,
	LETHAL2 = 3,
	PREFER = 4
};

struct AnimationBackup_t {
	vec3_t           m_origin, m_abs_origin;
	vec3_t           m_velocity, m_abs_velocity;
	int              m_flags, m_eflags;
	float            m_duck, m_body;
	C_AnimationLayer m_layers[13];
};

struct HitscanData_t {
	float  m_damage;
	int    m_hitbox, m_hitgroup;
	vec3_t m_pos;

	__forceinline HitscanData_t() : m_damage{ 0.f }, m_pos{}, m_hitbox{}, m_hitgroup{} {}
};

struct HitscanBox_t {
	int         m_index;
	HitscanMode m_mode;

	__forceinline bool operator==(const HitscanBox_t& c) const {
		return m_index == c.m_index && m_mode == c.m_mode;
	}
};

class AimPlayer {
public:
	using records_t = std::deque< std::shared_ptr< LagRecord > >;
	using hitboxcan_t = stdpp::unique_vector< HitscanBox_t >;

public:
	// essential data.
	Player* m_player;
	float	  m_spawn;
	records_t m_records;
	float	  m_freestand[2];

	// aimbot data.
	hitboxcan_t m_hitboxes;

	// resolve data.
	bool      freestand_data;
	float     m_anti_fs_angle;
	int       m_shots;
	int       m_missed_shots;
	LagRecord m_walk_record;

	float     m_body_update;
	bool      m_moved;


	int m_edge_index;
	int m_logic_index;
	int m_reversefs_index;
	int m_fam_reverse_index;
	bool m_should_correct;
	int m_testfs_index;
	int m_spin_index;
	float m_last_angle;
	int m_reversefs_at_index;
	int m_stand3_reversefs;
	int m_stand3_back;
	int m_airlast_index;
	int m_air_index;
	int m_airlby_index;
	float     m_body_delta;
	int m_airback_index;
	int m_back_index;
	int m_back_at_index;
	int m_lastmove_index;
	int m_sidelast_index;
	int m_lby_index;
	float update_body;
	int m_lowlby_index;
	int m_move_index;
	int m_stand_int[7];
	bool valid_flick;
	int m_lbyticks;
	LagRecord m_walk_stuff;
	bool m_broke_lby;
	bool m_has_body_updated;
	bool ever_flicked;
	int m_stand_index1;
	int m_stand_index2;
	float m_fakeflick_body;
	int m_stand_index3;
	int m_test_index;
	int m_stand_index4;
	int m_body_index;
	int m_fakewalk_index;
	int m_moving_index;
	bool m_override{ false };
	float m_override_angle;

	// data about the LBY proxy.
	float m_body;
	float m_old_body;
	bool is_last_moving_lby_valid;
	bool is_air_previous_valid;
	float m_valid_pitch;

	float m_stored_body;
	float m_stored_body_old;

	float m_body_proxy;
	float m_body_proxy_old;
	bool  m_body_proxy_updated;

public:
	void CorrectVelocity(Player* player, LagRecord* record, LagRecord* previous_record);

	void UpdateAnimations(LagRecord* record);
	void OnNetUpdate(Player* player);
	void OnRoundStart(Player* player);
	void SetupHitboxes(LagRecord* record);
	//void SetupHitboxes(LagRecord* record, bool history);
	bool SetupHitboxPoints(LagRecord* record, BoneArray* bones, int index, std::vector< vec3_t >& points);
	bool GetBestAimPosition(vec3_t& aim, float& damage, LagRecord* record, int& hitbox, int& hitgroup);

public:
	void reset() {
		m_player = nullptr;
		m_spawn = 0.f;
		m_walk_record = LagRecord{};
		m_shots = 0;
		m_body_delta = 0;
		m_missed_shots = 0;

		m_records.clear();
		m_hitboxes.clear();
	}
};

class Aimbot {
private:
	struct target_t {
		Player* m_player;
		AimPlayer* m_data;
	};

	struct knife_target_t {
		target_t  m_target;
		LagRecord m_record;
	};

	struct table_t {
		uint8_t swing[2][2][2]; // [ first ][ armor ][ back ]
		uint8_t stab[2][2];		  // [ armor ][ back ]
	};

	const table_t m_knife_dmg{ { { { 25, 90 }, { 21, 76 } }, { { 40, 90 }, { 34, 76 } } }, { { 65, 180 }, { 55, 153 } } };

	std::array< ang_t, 12 > m_knife_ang{
		ang_t{ 0.f, 0.f, 0.f }, ang_t{ 0.f, -90.f, 0.f }, ang_t{ 0.f, 90.f, 0.f }, ang_t{ 0.f, 180.f, 0.f },
		ang_t{ -80.f, 0.f, 0.f }, ang_t{ -80.f, -90.f, 0.f }, ang_t{ -80.f, 90.f, 0.f }, ang_t{ -80.f, 180.f, 0.f },
		ang_t{ 80.f, 0.f, 0.f }, ang_t{ 80.f, -90.f, 0.f }, ang_t{ 80.f, 90.f, 0.f }, ang_t{ 80.f, 180.f, 0.f }
	};

public:
	std::array< AimPlayer, 64 > m_players;
	std::vector< AimPlayer* >   m_targets;

	BackupRecord m_backup[64];

	// target selection stuff.
	float m_best_dist;
	float m_best_fov;
	float m_best_damage;
	int   m_best_hp;
	float m_best_lag;
	float m_best_height;

	// found target stuff.
	Player* m_target;
	ang_t      m_angle;
	vec3_t     m_aim;
	float      m_damage;
	bool       m_damage_toggle;
	int        m_hitbox, m_hitgroup;
	LagRecord* m_record;
	float      m_hit_chance, m_hit_chance_wall;



	// fake latency stuff.
	bool       m_fake_latency, m_fake_latency2;
	float latency_amount;
	bool can_hit;

	bool m_stop;

public:

	void handle_fakelatency(int tick_count, bool secondary, float secondary_val, float primary_val) {
		if (tick_count % 2 != 0)
			return;

		if (secondary) {
			if (latency_amount > secondary_val) {
				latency_amount = secondary_val;
				return;
			}

			if (latency_amount < secondary_val)
				latency_amount += 10.f;
		}
		else
			latency_amount = primary_val;
	}

	__forceinline void reset() {
		// reset aimbot data.
		init();

		// reset all players data.
		for (auto& p : m_players)
			p.reset();
	}

	__forceinline bool IsValidTarget(Player* player) {
		if (!player)
			return false;

		if (!player->IsPlayer())
			return false;

		if (!player->alive())
			return false;

		if (player->m_bIsLocalPlayer())
			return false;

		if (!player->enemy(g_cl.m_local))
			return false;

		return true;
	}

public:
	int main();
	// aimbot.
	void init();
	void update_shoot_pos();
	void StripAttack();
	void think();
	void find();
	bool CanHitPlayer(LagRecord* pRecord, const vec3_t& vecEyePos, const vec3_t& vecEnd, int iHitboxIndex);
	bool CheckHitchance(Player* player, const ang_t& angle);
	int CalcHitchance(Player* target, const ang_t& angle);
	bool SelectTarget(LagRecord* record, const vec3_t& aim, float damage);
	void apply();
	void NoSpread();

	// knifebot.
	void knife();
	bool CanKnife(LagRecord* record, ang_t angle, bool& stab);
	bool KnifeTrace(vec3_t dir, bool stab, CGameTrace* trace);
	bool KnifeIsBehind(LagRecord* record);

	int to_hitgroup(int hitbox) {

		switch (hitbox) {
		case HITBOX_HEAD:
		case HITBOX_NECK:
		case HITBOX_LOWER_NECK:
			return HITGROUP_HEAD;
		case HITBOX_UPPER_CHEST:
		case HITBOX_CHEST:
		case HITBOX_THORAX:
			return HITGROUP_CHEST;
		case HITBOX_PELVIS:
		case HITBOX_BODY:
			return HITGROUP_STOMACH;
		case HITBOX_L_THIGH:
		case HITBOX_L_CALF:
		case HITBOX_L_FOOT:
			return HITGROUP_LEFTLEG;
		case HITBOX_R_CALF:
		case HITBOX_R_FOOT:
		case HITBOX_R_THIGH:
			return HITGROUP_RIGHTLEG;
		case HITBOX_L_FOREARM:
		case HITBOX_L_HAND:
		case HITBOX_L_UPPER_ARM:
		case HITBOX_R_UPPER_ARM:
		case HITBOX_R_FOREARM:
		case HITBOX_R_HAND:
			return HITGROUP_LEFTARM; // lol ghetto
		default:
			return HITGROUP_GENERIC;
		}
	}
};

extern Aimbot g_aimbot;