//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Antlion - nasty bug
//
//=============================================================================//

#include "cbase.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_moveprobe.h"
#include "ai_route.h"
#include "npcevent.h"
#include "gib.h"
#include "entitylist.h"
#include "ndebugoverlay.h"
#include "antlion_dust.h"
#include "engine/IEngineSound.h"
#include "globalstate.h"
#include "movevars_shared.h"
#include "te_effect_dispatch.h"
#include "vehicle_base.h"
#include "mapentities.h"
#include "npc_android.h"
#include "decals.h"
#include "hl2_shareddefs.h"
#include "explode.h"
#include "weapon_physcannon.h"
#include "baseparticleentity.h"
#include "props.h"
#include "particle_parse.h"
#include "ai_tacticalservices.h"

#ifdef HL2_EPISODIC
#include "grenade_spit.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int AE_ANDROID_BALL;
int AE_ANDROID_UNBALL;

//Attack range definitions
#define	ANDROID_LASER_RANGE_MIN		1000.0f
#define	ANDROID_LASER_RANGE_MAX		2000.0f
#define	ANDROID_NAIL_RANGE_MIN		400.0f
#define	ANDROID_NAIL_RANGE_MAX		1200.0f

ConVar sk_android_health("sk_android_health", "125");

int g_interactionAndroidFoundTarget = 0;
int g_interactionAndroidFiredAtTarget = 0;
float g_AndroidBeamThinkTime = 0.0;

#define	ANDROID_MODEL			"models/antlion.mdl"
#define	ANDROID_RIGHT_ARM			2
#define	ANDROID_LEFT_ARM			1

//==================================================
// Antlion Activities
//==================================================

int ACT_ANDROID_ZAP;
int ACT_ANDROID_BALL;
int ACT_ANDROID_UNBALL;

CNPC_Android::CNPC_Android(void)
{
	m_flIdleDelay = 0.0f;
	m_flIgnoreSoundTime = 0.0f;
}

LINK_ENTITY_TO_CLASS(npc_android, CNPC_Android);

enum
{
	SCHED_ANDROID_RANGE_ATTACK,
};

//==================================================
// CNPC_Antlion::m_DataDesc
//==================================================

BEGIN_DATADESC(CNPC_Android)

DEFINE_FIELD(m_vecHeardSound, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_bHasHeardSound, FIELD_BOOLEAN),
DEFINE_FIELD(m_bAgitatedSound, FIELD_BOOLEAN),
DEFINE_FIELD(m_flZapDuration, FIELD_TIME),

DEFINE_FIELD(m_pBeam, FIELD_CLASSPTR),
DEFINE_FIELD(m_pLightGlow, FIELD_CLASSPTR),
DEFINE_FIELD(m_vLaserDir, FIELD_VECTOR),
DEFINE_FIELD(m_vLaserTargetPos, FIELD_POSITION_VECTOR),

DEFINE_KEYFIELD(m_flEludeDistance, FIELD_FLOAT, "eludedist"),

// Function Pointers
DEFINE_ENTITYFUNC(Touch),
DEFINE_THINKFUNC(ZapThink),

// DEFINE_FIELD( FIELD_SHORT, m_hFootstep ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::Spawn(void)
{
	Precache();

	SetModel(DefaultOrCustomModel(ANDROID_MODEL));
	SetBloodColor(BLOOD_COLOR_MECH);

	SetHullType(HULL_MEDIUM);
	SetHullSizeNormal();
	SetDefaultEyeOffset();

	SetNavType(NAV_GROUND);

	m_NPCState = NPC_STATE_NONE;
	m_iHealth = sk_android_health.GetFloat();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);


	SetMoveType(MOVETYPE_STEP);

	//Only do this if a squadname appears in the entity
	if (m_SquadName != NULL_STRING)
	{
		CapabilitiesAdd(bits_CAP_SQUAD);
	}

	SetCollisionGroup(HL2COLLISION_GROUP_ANTLION);

	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK2);

	NPCInit();

	// always pursue
	m_flDistTooFar = FLT_MAX;

	BaseClass::Spawn();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::Activate(void)
{
	BaseClass::Activate();
}


//-----------------------------------------------------------------------------
// Purpose: override this to simplify the physics shadow of the antlions
//-----------------------------------------------------------------------------
bool CNPC_Android::CreateVPhysics()
{
	bool bRet = BaseClass::CreateVPhysics();
	return bRet;
}

// Use all the gibs
#define	NUM_ANDROID_GIBS_UNIQUE	3
const char* pszAndroidGibs_Unique[NUM_ANDROID_GIBS_UNIQUE] = {
	"models/gibs/antlion_gib_large_1.mdl",
	"models/gibs/antlion_gib_large_2.mdl",
	"models/gibs/antlion_gib_large_3.mdl"
};

#define	NUM_ANDROID_GIBS_MEDIUM	3
const char* pszAndroidGibs_Medium[NUM_ANDROID_GIBS_MEDIUM] = {
	"models/gibs/antlion_gib_medium_1.mdl",
	"models/gibs/antlion_gib_medium_2.mdl",
	"models/gibs/antlion_gib_medium_3.mdl"
};

// XBox doesn't use the smaller gibs, so don't cache them
#define	NUM_ANDROID_GIBS_SMALL	3
const char* pszAndroidGibs_Small[NUM_ANDROID_GIBS_SMALL] = {
	"models/gibs/antlion_gib_small_1.mdl",
	"models/gibs/antlion_gib_small_2.mdl",
	"models/gibs/antlion_gib_small_3.mdl"
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::Precache(void)
{
	PrecacheModel("sprites/laser.vmt");

	PrecacheModel(ANDROID_MODEL);
	PropBreakablePrecacheAll(MAKE_STRING(ANDROID_MODEL));
	PrecacheParticleSystem("blood_impact_antlion_01");
	PrecacheParticleSystem("AntlionGib");

	for (int i = 0; i < NUM_ANDROID_GIBS_UNIQUE; ++i)
	{
		PrecacheModel(pszAndroidGibs_Unique[i]);
	}
	for (int i = 0; i < NUM_ANDROID_GIBS_MEDIUM; ++i)
	{
		PrecacheModel(pszAndroidGibs_Medium[i]);
	}
	for (int i = 0; i < NUM_ANDROID_GIBS_SMALL; ++i)
	{
		PrecacheModel(pszAndroidGibs_Small[i]);
	}

	PrecacheScriptSound("NPC_Antlion.RunOverByVehicle");
	PrecacheScriptSound("NPC_Antlion.MeleeAttack");
	m_hFootstep = PrecacheScriptSound("NPC_Antlion.Footstep");
	PrecacheScriptSound("NPC_Antlion.BurrowIn");
	PrecacheScriptSound("NPC_Antlion.BurrowOut");
	PrecacheScriptSound("NPC_Antlion.FootstepSoft");
	PrecacheScriptSound("NPC_Antlion.FootstepHeavy");
	PrecacheScriptSound("NPC_Antlion.MeleeAttackSingle");
	PrecacheScriptSound("NPC_Antlion.MeleeAttackDouble");
	PrecacheScriptSound("NPC_Antlion.Distracted");
	PrecacheScriptSound("NPC_Antlion.Idle");
	PrecacheScriptSound("NPC_Antlion.Pain");
	PrecacheScriptSound("NPC_Antlion.Land");
	PrecacheScriptSound("NPC_Antlion.WingsOpen");
	PrecacheScriptSound("NPC_Antlion.LoopingAgitated");
	PrecacheScriptSound("NPC_Antlion.Distracted");
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Cache whatever pose parameters we intend to use
//-----------------------------------------------------------------------------
void	CNPC_Android::PopulatePoseParameters(void)
{
	m_poseHead_Pitch = LookupPoseParameter("head_pitch");
	m_poseHead_Yaw = LookupPoseParameter("head_yaw");

	BaseClass::PopulatePoseParameters();
}

#define	ANDROID_VIEW_FIELD_NARROW	0.85f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::FInViewCone(CBaseEntity* pEntity)
{
	m_flFieldOfView = (GetEnemy() != NULL) ? ANDROID_VIEW_FIELD_NARROW : VIEW_FIELD_WIDE;

	return BaseClass::FInViewCone(pEntity);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecSpot - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::FInViewCone(const Vector& vecSpot)
{
	m_flFieldOfView = (GetEnemy() != NULL) ? ANDROID_VIEW_FIELD_NARROW : VIEW_FIELD_WIDE;

	return BaseClass::FInViewCone(vecSpot);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//-----------------------------------------------------------------------------
void CNPC_Android::Event_Killed(const CTakeDamageInfo& info)
{
	VacateStrategySlot();

	if (info.GetDamageType() & DMG_CRUSH)
	{
		CSoundEnt::InsertSound(SOUND_PHYSICS_DANGER, GetAbsOrigin(), 256, 0.5f, this);
	}

	BaseClass::Event_Killed(info);

	// Stop our zap effect!
	SetContextThink(NULL, gpGlobals->curtime, "ZapThink");
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Android::MeleeAttack(float distance, float damage, QAngle& viewPunch, Vector& shove)
{
	Vector vecForceDir;

	// Always hurt bullseyes for now
	if ((GetEnemy() != NULL) && (GetEnemy()->Classify() == CLASS_BULLSEYE))
	{
		vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		CTakeDamageInfo info(this, this, damage, DMG_SLASH);
		CalculateMeleeDamageForce(&info, vecForceDir, GetEnemy()->GetAbsOrigin());
		GetEnemy()->TakeDamage(info);
		return;
	}

	CBaseEntity* pHurt = CheckTraceHullAttack(distance, -Vector(16, 16, 32), Vector(16, 16, 32), damage, DMG_SLASH, 5.0f);

	if (pHurt)
	{
		vecForceDir = (pHurt->WorldSpaceCenter() - WorldSpaceCenter());

		CBasePlayer* pPlayer = ToBasePlayer(pHurt);

		if (pPlayer != NULL)
		{
			//Kick the player angles
			if (!(pPlayer->GetFlags() & FL_GODMODE) && pPlayer->GetMoveType() != MOVETYPE_NOCLIP)
			{
				pPlayer->ViewPunch(viewPunch);

				Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
				VectorNormalize(dir);

				QAngle angles;
				VectorAngles(dir, angles);
				Vector forward, right;
				AngleVectors(angles, &forward, &right, NULL);

				//Push the target back
				pHurt->ApplyAbsVelocityImpulse(-right * shove[1] - forward * shove[0]);
			}
		}

		// Play a random attack hit sound
		EmitSound("NPC_Antlion.MeleeAttack");
	}
}

// Number of times the antlions will attempt to generate a random chase position
#define NUM_CHASE_POSITION_ATTEMPTS		3

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &targetPos - 
//			&result - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::FindChasePosition(const Vector& targetPos, Vector& result)
{

	Vector runDir = (targetPos - GetAbsOrigin());
	VectorNormalize(runDir);

	Vector	vRight, vUp;
	VectorVectors(runDir, vRight, vUp);

	for (int i = 0; i < NUM_CHASE_POSITION_ATTEMPTS; i++)
	{
		result = targetPos;
		result += -runDir * random->RandomInt(64, 128);
		result += vRight * random->RandomInt(-128, 128);

		//FIXME: We need to do a more robust search here
		// Find a ground position and try to get there
		if (GetGroundPosition(result, result))
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &testPos - 
//-----------------------------------------------------------------------------
bool CNPC_Android::GetGroundPosition(const Vector& testPos, Vector& result)
{
	// Trace up to clear the ground
	trace_t	tr;
	AI_TraceHull(testPos, testPos + Vector(0, 0, 64), NAI_Hull::Mins(GetHullType()), NAI_Hull::Maxs(GetHullType()), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// If we're stuck in solid, this can't be valid
	if (tr.allsolid)
	{
		return false;
	}

	// Trace down to find the ground
	AI_TraceHull(tr.endpos, tr.endpos - Vector(0, 0, 128), NAI_Hull::Mins(GetHullType()), NAI_Hull::Maxs(GetHullType()), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// We must end up on the floor with this trace
	if (tr.fraction < 1.0f)
	{
		result = tr.endpos;
		return true;
	}

	// Ended up in open space
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the enemy has been seen within the time period supplied
// Input  : flTime - Timespan we consider
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::SeenEnemyWithinTime(float flTime)
{
	float flLastSeenTime = GetEnemies()->LastTimeSeen(GetEnemy());
	return (flLastSeenTime != 0.0f && (gpGlobals->curtime - flLastSeenTime) < flTime);
}

//-----------------------------------------------------------------------------
// Purpose: Test whether this antlion can hit the target
//-----------------------------------------------------------------------------
bool CNPC_Android::InnateWeaponLOSCondition(const Vector& ownerPos, const Vector& targetPos, bool bSetConditions)
{
	if (GetNextAttack() > gpGlobals->curtime)
		return false;

	return BaseClass::InnateWeaponLOSCondition(ownerPos, targetPos, bSetConditions);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDuration - 
//-----------------------------------------------------------------------------
void CNPC_Android::DelaySquadAttack(float flDuration)
{
	if (GetSquad())
	{
		// Reduce the duration by as much as 50% of the total time to make this less robotic
		float flAdjDuration = flDuration - random->RandomFloat(0.0f, (flDuration * 0.5f));
		GetSquad()->BroadcastInteraction(g_interactionAndroidFiredAtTarget, (void*)&flAdjDuration, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Android::HandleAnimEvent(animevent_t* pEvent)
{
	// Handle the ball event
	if (pEvent->event == AE_ANDROID_BALL)
	{
		
		return;
	}
	else if (pEvent->event == AE_ANDROID_UNBALL)
	{

		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}


//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
void CNPC_Android::StartTask(const Task_t* pTask)
{
	CAI_Schedule* pCurrentSchedule = GetCurSchedule();
	int scheduleId = pCurrentSchedule->GetId();
	const char* scheduleName = GetSchedulingSymbols()->ScheduleIdToSymbol(scheduleId);

	DevMsg("%s\n", scheduleName);
	switch (pTask->iTask)
	{
		case TASK_ANNOUNCE_ATTACK:
		{
			EmitSound("NPC_Antlion.MeleeAttackSingle");
			TaskComplete();
			break;
		}
		case TASK_RANGE_ATTACK1:
		{
			CBaseEntity* pEnemy = GetEnemy();
			if (pEnemy)
			{
				m_vLaserTargetPos = GetEnemyLKP() + pEnemy->GetViewOffset();

				// Never hit target on first try
				Vector missPos = m_vLaserTargetPos;

				if (pEnemy->Classify() == CLASS_BULLSEYE && hl2_episodic.GetBool())
				{
					missPos.x += 60 + 120 * random->RandomInt(-1, 1);
					missPos.y += 60 + 120 * random->RandomInt(-1, 1);
				}
				else
				{
					missPos.x += 80 * random->RandomInt(-1, 1);
					missPos.y += 80 * random->RandomInt(-1, 1);
				}

				// ----------------------------------------------------------------------
				// If target is facing me and not running towards me shoot below his feet
				// so he can see the laser coming
				// ----------------------------------------------------------------------
				CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pEnemy);
				if (pBCC)
				{
					Vector targetToMe = (pBCC->GetAbsOrigin() - GetAbsOrigin());
					Vector vBCCFacing = pBCC->BodyDirection2D();
					if ((DotProduct(vBCCFacing, targetToMe) < 0) &&
						(pBCC->GetSmoothedVelocity().Length() < 50))
					{
						missPos.z -= 150;
					}
					// --------------------------------------------------------
					// If facing away or running towards laser,
					// shoot above target's head 
					// --------------------------------------------------------
					else
					{
						missPos.z += 60;
					}
				}
				m_vLaserDir = missPos - LaserStartPosition(GetAbsOrigin());
				VectorNormalize(m_vLaserDir);
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}

			StartAttackBeam();
			SetActivity(ACT_RANGE_ATTACK1);
			break;
		}
		default:
			BaseClass::StartTask(pTask);
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_Android::RunTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
		case TASK_RANGE_ATTACK1:
			UpdateAttackBeam();
			if (!TaskIsRunning() || HasCondition(COND_TASK_FAILED))
			{
				KillAttackBeam();
			}
			break;
		default:
			BaseClass::RunTask(pTask);
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{
	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Android::TranslateSchedule(int scheduleType)
{
	// FIXME: Custom schedule doesn't work?
	/*switch (scheduleType)
	{
		case SCHED_RANGE_ATTACK1:
		{
			return SCHED_ANDROID_RANGE_ATTACK;
		}

	}*/

	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Activity CNPC_Android::NPC_TranslateActivity(Activity baseAct)
{
	return BaseClass::NPC_TranslateActivity(baseAct);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::ZapThink(void)
{
	CEffectData	data;
	data.m_nEntIndex = entindex();
	data.m_flMagnitude = 4;
	data.m_flScale = random->RandomFloat(0.25f, 1.0f);

	DispatchEffect("TeslaHitboxes", data);

	if (m_flZapDuration > gpGlobals->curtime)
	{
		SetContextThink(&CNPC_Android::ZapThink, gpGlobals->curtime + random->RandomFloat(0.05f, 0.25f), "ZapThink");
	}
	else
	{
		SetContextThink(NULL, gpGlobals->curtime, "ZapThink");
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Android::SelectSchedule(void)
{
	// See if a friendly player is pushing us away
	if (HasCondition(COND_PLAYER_PUSHING))
		return SCHED_MOVE_AWAY;

	//Flipped?
	if (HasCondition(COND_ANDROID_ZAPPED))
	{
		ClearCondition(COND_ANDROID_ZAPPED);

		// See if it's a forced, electrical flip
		if (m_flZapDuration > gpGlobals->curtime)
		{
			SetContextThink(&CNPC_Android::ZapThink, gpGlobals->curtime, "ZapThink");
			return SCHED_ANDROID_ZAP_RECOVER;
		}

		// Regular flip
		return SCHED_ANDROID_ZAP_RECOVER;
	}

	if (BehaviorSelectSchedule())
	{
		return BaseClass::SelectSchedule();
	}

	switch (m_NPCState)
	{
	case NPC_STATE_IDLE:
	case NPC_STATE_ALERT:
	{
		return SCHED_IDLE_STAND;

		break;
	}

	case NPC_STATE_COMBAT:
	{

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			if (OccupyStrategySlotRange(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2))
			{
				return SCHED_RANGE_ATTACK1;
			}
			else
			{
				return SCHED_COMBAT_PATROL;
			}
		}

		if (!HasCondition(COND_SEE_ENEMY))
		{
			return SCHED_COMBAT_PATROL;
		}

		return SCHED_COMBAT_FACE;

		break;
	}
	}

	// no special cases here, call the base class
	return BaseClass::SelectSchedule();
}

void CNPC_Android::Ignite(float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner)
{
	float flDamage = m_iHealth + 1;

	CTakeDamageInfo	dmgInfo(this, this, flDamage, DMG_GENERIC);
	GuessDamageForce(&dmgInfo, Vector(0, 0, 8), GetAbsOrigin());
	TakeDamage(dmgInfo);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Android::OnTakeDamage_Alive(const CTakeDamageInfo& info)
{
	CTakeDamageInfo newInfo = info;

	// If we're being hoisted by a barnacle, we only take damage from that barnacle (otherwise we can die too early!)
	if (IsEFlagSet(EFL_IS_BEING_LIFTED_BY_BARNACLE))
	{
		if (info.GetAttacker() && info.GetAttacker()->Classify() != CLASS_BARNACLE)
			return 0;
	}

	// Find out how much damage we're about to take
	int nDamageTaken = BaseClass::OnTakeDamage_Alive(newInfo);
	if (gpGlobals->curtime - m_flLastDamageTime < 0.5f)
	{
		// Accumulate it
		m_nSustainedDamage += nDamageTaken;
	}
	else
	{
		// Reset, it's been too long
		m_nSustainedDamage = nDamageTaken;
	}

	m_flLastDamageTime = gpGlobals->curtime;

	return nDamageTaken;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline bool CNPC_Android::IsZapped(void)
{
	return (GetActivity() == ACT_ANDROID_ZAP);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::TraceAttack(const CTakeDamageInfo& info, const Vector& vecDir, trace_t* ptr, CDmgAccumulator* pAccumulator)
{
	CTakeDamageInfo newInfo = info;

	Vector	vecShoveDir = vecDir;
	vecShoveDir.z = 0.0f;

	//Are we already flipped?
	if (IsZapped())
	{
		//If we were hit by physics damage, move with it
		if (newInfo.GetDamageType() & (DMG_CRUSH | DMG_PHYSGUN))
		{
			PainSound(newInfo);
		}

		//More vulnerable when flipped
		newInfo.ScaleDamage(4.0f);
	}
	else if (newInfo.GetDamageType() & (DMG_PHYSGUN) ||
		(newInfo.GetDamageType() & (DMG_BLAST | DMG_CRUSH) && newInfo.GetDamage() >= 25.0f))
	{
		// Don't do this if we're in an interaction
		if (!IsRunningDynamicInteraction())
		{
			//Grenades, physcannons, and physics impacts make us fuh-lip!

			if (hl2_episodic.GetBool())
			{
				PainSound(newInfo);

				if (GetFlags() & FL_ONGROUND)
				{
					// Only flip if on the ground.
					SetCondition(COND_ANDROID_ZAPPED);
				}
			}
			else
			{
				//Don't flip off the deck
				if (GetFlags() & FL_ONGROUND)
				{
					PainSound(newInfo);

					SetCondition(COND_ANDROID_ZAPPED);
				}
			}
		}
	}

	BaseClass::TraceAttack(newInfo, vecDir, ptr, pAccumulator);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::IdleSound(void)
{
	EmitSound("NPC_Antlion.Idle");
	m_flIdleDelay = gpGlobals->curtime + 4.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::PainSound(const CTakeDamageInfo& info)
{
	EmitSound("NPC_Antlion.Pain");
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
float CNPC_Android::GetIdealAccel(void) const
{
	return GetIdealSpeed() * 2.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CNPC_Android::MaxYawSpeed(void)
{
	switch (GetActivity())
	{
	case ACT_IDLE:
		return 32.0f;
		break;

	case ACT_WALK:
		return 16.0f;
		break;

	default:
	case ACT_RUN:
		return 32.0f;
		break;
	}

	return 32.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::ShouldPlayIdleSound(void)
{
	//Only do idles in the right states
	if ((m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT))
		return false;

	//Gagged monsters don't talk
	if (m_spawnflags & SF_NPC_GAG)
		return false;

	//Don't cut off another sound or play again too soon
	if (m_flIdleDelay > gpGlobals->curtime)
		return false;

	//Randomize it a bit
	if (random->RandomInt(0, 20))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Determine whether or not to check our attack conditions
//-----------------------------------------------------------------------------
bool CNPC_Android::FCanCheckAttacks(void)
{
	return BaseClass::FCanCheckAttacks();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::RangeAttack1Conditions(float flDot, float flDist)
{
	if (GetNextAttack() > gpGlobals->curtime)
		return COND_NOT_FACING_ATTACK;

	if (flDot < DOT_10DEGREE)
		return COND_NOT_FACING_ATTACK;

	if (flDist > (150 * 12))
		return COND_TOO_FAR_TO_ATTACK;

	if (flDist < (20 * 12))
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

int CNPC_Android::RangeAttack2Conditions(float flDot, float flDist)
{
	if (GetNextAttack() > gpGlobals->curtime)
		return COND_NOT_FACING_ATTACK;

	if (flDot < DOT_10DEGREE)
		return COND_NOT_FACING_ATTACK;

	if (flDist > (150 * 12))
		return COND_TOO_FAR_TO_ATTACK;

	if (flDist < (20 * 12))
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK2;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::MeleeAttack1Conditions(float flDot, float flDist)
{
#if 1 //NOTENOTE: Use predicted position melee attacks

	//Get our likely position in one half second
	Vector vecPrPos;
	UTIL_PredictedPosition(GetEnemy(), 0.5f, &vecPrPos);


	// Compare our target direction to our body facing
	Vector2D vec2DPrDir = (vecPrPos - GetAbsOrigin()).AsVector2D();
	Vector2D vec2DBodyDir = BodyDirection2D().AsVector2D();

	float flPrDot = DotProduct2D(vec2DPrDir, vec2DBodyDir);
	if (flPrDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	trace_t	tr;
	AI_TraceHull(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8, 8, 8), Vector(8, 8, 8), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// If the hit entity isn't our target and we don't hate it, don't hit it
#ifdef MAPBASE
	if (tr.m_pEnt != GetEnemy() && tr.fraction < 1.0f && IRelationType(tr.m_pEnt) > D_FR)
#else
	if (tr.m_pEnt != GetEnemy() && tr.fraction < 1.0f && IRelationType(tr.m_pEnt) != D_HT)
#endif
		return 0;

#else

	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = ANTLION_MELEE1_RANGE;

	if (GetEnemy())
	{
		// Give us extra space if our enemy is in a vehicle
		CBaseCombatCharacter* pCCEnemy = GetEnemy()->MyCombatCharacterPointer();
		if (pCCEnemy != NULL && pCCEnemy->IsInAVehicle())
		{
			flAdjustedDist *= 2.0f;
		}
	}

	if (flDist > flAdjustedDist)
		return COND_TOO_FAR_TO_ATTACK;

	trace_t	tr;
	AI_TraceHull(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8, 8, 8), Vector(8, 8, 8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
		return 0;

#endif
	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::Alone(void)
{
	if (m_pSquad == NULL)
		return true;

	if (m_pSquad->NumMembers() <= 1)
		return true;

	return false;
}

bool NPC_CheckBrushExclude(CBaseEntity* pEntity, CBaseEntity* pBrush);
//-----------------------------------------------------------------------------
// traceline methods
//-----------------------------------------------------------------------------
class CTraceFilterSimpleNPCExclude : public CTraceFilterSimple
{
public:
	DECLARE_CLASS(CTraceFilterSimpleNPCExclude, CTraceFilterSimple);

	CTraceFilterSimpleNPCExclude(const IHandleEntity* passentity, int collisionGroup)
		: CTraceFilterSimple(passentity, collisionGroup)
	{
	}

	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		Assert(dynamic_cast<CBaseEntity*>(pHandleEntity));
		CBaseEntity* pTestEntity = static_cast<CBaseEntity*>(pHandleEntity);

		if (GetPassEntity())
		{
			CBaseEntity* pEnt = gEntList.GetBaseEntity(GetPassEntity()->GetRefEHandle());

			if (pEnt->IsNPC())
			{
				if (NPC_CheckBrushExclude(pEnt, pTestEntity) == true)
					return false;
			}
		}
		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::QuerySeeEntity(CBaseEntity* pEntity, bool bOnlyHateOrFearIfNPC)
{
	//If we're under the ground, don't look at enemies
	if (IsEffectActive(EF_NODRAW))
		return false;

	return BaseClass::QuerySeeEntity(pEntity, bOnlyHateOrFearIfNPC);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSound - 
//-----------------------------------------------------------------------------
bool CNPC_Android::QueryHearSound(CSound* pSound)
{
	if (!BaseClass::QueryHearSound(pSound))
		return false;

	if (pSound->m_iType == SOUND_BUGBAIT)
	{
		//Must be more recent than the current
		if (pSound->SoundExpirationTime() <= m_flIgnoreSoundTime)
			return false;

		//If we can hear it, store it
		m_bHasHeardSound = (pSound != NULL);
		if (m_bHasHeardSound)
		{
			m_vecHeardSound = pSound->GetSoundOrigin();
			m_flIgnoreSoundTime = pSound->SoundExpirationTime();
		}
	}

	//Do the normal behavior at this point
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Android::BuildScheduleTestBits(void)
{
	//Don't allow any modifications when scripted
	if (m_NPCState == NPC_STATE_SCRIPT)
		return;

	//Make sure we interrupt a run schedule if we can jump
	if (IsCurSchedule(SCHED_CHASE_ENEMY))
	{
		SetCustomInterruptCondition(COND_ENEMY_UNREACHABLE);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::IsValidEnemy(CBaseEntity* pEnemy)
{

	if (pEnemy->IsWorld())
		return false;

	return BaseClass::IsValidEnemy(pEnemy);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::GatherConditions(void)
{
	BaseClass::GatherConditions();

	//Ignore the player pushing me if I'm flipped over!
	if (IsCurSchedule(SCHED_ANDROID_ZAP_RECOVER))
		ClearCondition(COND_PLAYER_PUSHING);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDamage - 
//			bitsDamageType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::IsLightDamage(const CTakeDamageInfo& info)
{
	if ((random->RandomInt(0, 1)) && (info.GetDamage() > 3))
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
//-----------------------------------------------------------------------------
void CNPC_Android::GatherEnemyConditions(CBaseEntity* pEnemy)
{
	// Do the base class
	BaseClass::GatherEnemyConditions(pEnemy);

	// If we're not already too far away, check again
	//TODO: Check to make sure we don't already have a condition set that removes the need for this
	if (HasCondition(COND_ENEMY_UNREACHABLE) == false)
	{
		Vector	predPosition;
		UTIL_PredictedPosition(GetEnemy(), 1.0f, &predPosition);

		Vector	predDir = (predPosition - GetAbsOrigin());
		float	predLength = VectorNormalize(predDir);

		// See if we'll be outside our effective target range
		if (predLength > m_flEludeDistance)
		{
			Vector	predVelDir = (predPosition - GetEnemy()->GetAbsOrigin());
			float	predSpeed = VectorNormalize(predVelDir);

			// See if the enemy is moving mostly away from us
			if ((predSpeed > 512.0f) && (DotProduct(predVelDir, predDir) > 0.0f))
			{
				// Mark the enemy as eluded and burrow away
				ClearEnemyMemory();
				SetEnemy(NULL);
				SetIdealState(NPC_STATE_ALERT);
				SetCondition(COND_ENEMY_UNREACHABLE);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CNPC_Android::Touch(CBaseEntity* pOther)
{
	//See if the touching entity is a vehicle
	CBasePlayer* pPlayer = ToBasePlayer(AI_GetSinglePlayer());

	// FIXME: Technically we'll want to check to see if a vehicle has touched us with the player OR NPC driver

	if (pPlayer && pPlayer->IsInAVehicle())
	{
		IServerVehicle* pVehicle = pPlayer->GetVehicle();
		CBaseEntity* pVehicleEnt = pVehicle->GetVehicleEnt();

		if (pVehicleEnt == pOther)
		{
			CPropVehicleDriveable* pDrivableVehicle = dynamic_cast<CPropVehicleDriveable*>(pVehicleEnt);

			if (pDrivableVehicle != NULL)
			{
				//Get tossed!
				Vector	vecShoveDir = pOther->GetAbsVelocity();
				Vector	vecTargetDir = GetAbsOrigin() - pOther->GetAbsOrigin();

				VectorNormalize(vecShoveDir);
				VectorNormalize(vecTargetDir);

				if (((pDrivableVehicle->m_nRPM > 75) && DotProduct(vecShoveDir, vecTargetDir) <= 0))
				{
					if (IsZapped())
					{
						float flDamage = m_iHealth;

						if (random->RandomInt(0, 10) > 4)
							flDamage += 25;

						CTakeDamageInfo	dmgInfo(pVehicleEnt, pPlayer, flDamage, DMG_VEHICLE);

						CalculateMeleeDamageForce(&dmgInfo, vecShoveDir, pOther->GetAbsOrigin());
						TakeDamage(dmgInfo);
					}
					else
					{
						// We're being shoved
						CTakeDamageInfo	dmgInfo(pVehicleEnt, pPlayer, 0, DMG_VEHICLE);
						PainSound(dmgInfo);

						SetCondition(COND_ANDROID_ZAPPED);

						vecTargetDir[2] = 0.0f;

						ApplyAbsVelocityImpulse((vecTargetDir * 250.0f) + Vector(0, 0, 64.0f));
						SetGroundEntity(NULL);

						CSoundEnt::InsertSound(SOUND_PHYSICS_DANGER, GetAbsOrigin(), 256, 0.5f, this);
					}
				}
			}
		}
	}

	BaseClass::Touch(pOther);

	// Did the player touch me?
	if (pOther->IsPlayer())
	{
		// Don't test for this if the pusher isn't friendly
		if (IsValidEnemy(pOther))
			return;

		// Ignore if pissed at player
		if (m_afMemory & bits_MEMORY_PROVOKED)
			return;
	}

	//Adrian: Explode if hit by gunship!
	//Maybe only do this if hit by the propellers?
	if (pOther->IsNPC())
	{
		if (pOther->Classify() == CLASS_COMBINE_GUNSHIP)
		{
			float flDamage = m_iHealth + 25;

			CTakeDamageInfo	dmgInfo(pOther, pOther, flDamage, DMG_GENERIC);
			GuessDamageForce(&dmgInfo, (pOther->GetAbsOrigin() - GetAbsOrigin()), pOther->GetAbsOrigin());
			TakeDamage(dmgInfo);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: turn in the direction of movement
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Android::OverrideMoveFacing(const AILocalMoveGoal_t& move, float flInterval)
{
	if (GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN)
	{
		// FIXME: this will break scripted sequences that walk when they have an enemy
		Vector vecEnemyLKP = GetEnemyLKP();
		if (UTIL_DistApprox(vecEnemyLKP, GetAbsOrigin()) < 512)
		{
			// Only start facing when we're close enough
			AddFacingTarget(GetEnemy(), vecEnemyLKP, 1.0, 0.2);
		}
	}

	return BaseClass::OverrideMoveFacing(move, flInterval);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::CreateBehaviors(void)
{
	AddBehavior(&m_AssaultBehavior);

	return BaseClass::CreateBehaviors();
}

//-----------------------------------------------------------------------------
// Purpose: Flip the antlion over
//-----------------------------------------------------------------------------
void CNPC_Android::Zap(bool bZapped /*= false*/)
{
	// We can't flip an already flipped antlion
	if (IsZapped())
		return;

	// Must be on the ground
	if ((GetFlags() & FL_ONGROUND) == false)
		return;

	// Can't be in a dynamic interation
	if (IsRunningDynamicInteraction())
		return;

	SetCondition(COND_ANDROID_ZAPPED);

	if (bZapped)
	{
		m_flZapDuration = gpGlobals->curtime + SequenceDuration(SelectWeightedSequence((Activity)ACT_ANDROID_ZAP)) + 0.1f;

		EmitSound("NPC_Antlion.ZappedFlip");
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Android::IsHeavyDamage(const CTakeDamageInfo& info)
{
	return BaseClass::IsHeavyDamage(info);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bForced - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Android::CanRunAScriptedNPCInteraction(bool bForced /*= false*/)
{
	return BaseClass::CanRunAScriptedNPCInteraction(bForced);
}

//------------------------------------------------------------------------------
// Purpose : Draw attack beam and do damage / decals
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_Android::KillAttackBeam(void)
{
	if (!m_pBeam)
		return;

	// Kill sound
	StopSound(m_pBeam->entindex(), "NPC_Stalker.BurnWall");
	StopSound(m_pBeam->entindex(), "NPC_Stalker.BurnFlesh");

	UTIL_Remove(m_pLightGlow);
	UTIL_Remove(m_pBeam);
	m_pBeam = NULL;

	ClearCondition(COND_CAN_RANGE_ATTACK1);

	RelaxAim();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CNPC_Android::LaserStartPosition(Vector vStalkerPos)
{
	// Get attachment position
	Vector vAttachPos;
	GetAttachment(ANDROID_LEFT_ARM, vAttachPos);

	// Now convert to vStalkerPos
	vAttachPos = vAttachPos - GetAbsOrigin() + vStalkerPos;
	return vAttachPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::StartAttackBeam(void)
{
	if (!m_pBeam)
	{
		Vector vecSrc = LaserStartPosition(GetAbsOrigin());
		trace_t tr;
		AI_TraceLine(vecSrc, vecSrc + m_vLaserDir * 99999, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction >= 1.0)
		{
			// too far
			TaskComplete();
			return;
		}

		m_pBeam = CBeam::BeamCreate("sprites/laser.vmt", 2.0);
		m_pBeam->PointEntInit(tr.endpos, this);
		m_pBeam->SetEndAttachment(ANDROID_LEFT_ARM);
		m_pBeam->SetBrightness(255);
		m_pBeam->SetNoise(0);
		m_pBeam->SetColor(255, 0, 0);
		m_pLightGlow = CSprite::SpriteCreate("sprites/redglow1.vmt", GetAbsOrigin(), FALSE);

		// ----------------------------
		// Light myself in a red glow
		// ----------------------------
		m_pLightGlow->SetTransparency(kRenderGlow, 255, 200, 200, 0, kRenderFxNoDissipation);
		m_pLightGlow->SetAttachment(this, ANDROID_LEFT_ARM);
		m_pLightGlow->SetBrightness(255);
		m_pLightGlow->SetScale(0.65);
	}

	SetNextThink(gpGlobals->curtime + g_AndroidBeamThinkTime);
	m_fBeamEndTime = gpGlobals->curtime + 99999;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::UpdateAttackBeam(void)
{
	DevMsg("Update\n");
	CBaseEntity* pEnemy = GetEnemy();
	// If not burning at a target 
	if (pEnemy)
	{
		Vector enemyLKP = GetEnemyLKP();
		m_vLaserTargetPos = pEnemy->GetAbsOrigin();

		// Face my enemy
		GetMotor()->SetIdealYawToTargetAndUpdate(enemyLKP);

		// ---------------------------------------------
		//	Get beam end point
		// ---------------------------------------------
		Vector vecSrc = LaserStartPosition(GetAbsOrigin());
		Vector targetDir = m_vLaserTargetPos - vecSrc;
		VectorNormalize(targetDir);
		// --------------------------------------------------------
		//	If beam position and laser dir are way off, end attack
		// --------------------------------------------------------
		if (DotProduct(targetDir, m_vLaserDir) < 0.5)
		{
			TaskComplete();
			return;
		}

		trace_t tr;
		AI_TraceLine(vecSrc, vecSrc + m_vLaserDir * 99999, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
		// ---------------------------------------------
		//  If beam not long enough, stop attacking
		// ---------------------------------------------
		if (tr.fraction == 1.0)
		{
			TaskComplete();
			return;
		}

		// If enemy hits laser, hurt them.
		if (tr.DidHit())
		{
			CBaseEntity* pHitEntity = tr.m_pEnt;

			if (pHitEntity && pHitEntity != this)
			{
				if (pHitEntity == pEnemy)
				{
					CTakeDamageInfo damageInfo;
					damageInfo.SetDamage(5.0f);
					damageInfo.SetAttacker(this);
					damageInfo.SetInflictor(this);
					damageInfo.SetDamageType(DMG_DISSOLVE);

					pHitEntity->TakeDamage(damageInfo);
				}
			}
		}

		CSoundEnt::InsertSound(SOUND_DANGER, tr.endpos, 60, 0.025, this);
	}
	else
	{
		TaskFail(FAIL_NO_ENEMY);
	}
}

AI_BEGIN_CUSTOM_NPC(npc_android, CNPC_Android)

//Conditions
DECLARE_CONDITION(COND_ANDROID_ZAPPED)

//Squad slots
// TODO

//Tasks
DECLARE_TASK(TASK_ANDROID_BALL)
DECLARE_TASK(TASK_ANDROID_UNBALL)

//Activities
DECLARE_ACTIVITY(ACT_ANDROID_ZAP)
DECLARE_ACTIVITY(ACT_ANDROID_BALL)
DECLARE_ACTIVITY(ACT_ANDROID_UNBALL)

//Events
DECLARE_ANIMEVENT(AE_ANDROID_BALL)
DECLARE_ANIMEVENT(AE_ANDROID_UNBALL)

//Schedules

DEFINE_SCHEDULE
(
	SCHED_ANDROID_ZAP_RECOVER,

	"	Tasks"
	"		TASK_STOP_MOVING	0"
	"		TASK_RESET_ACTIVITY		0"
	"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_ANDROID_ZAP"

	"	Interrupts"
	"		COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_RANGE_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_ENEMY					0"
	"		TASK_RANGE_ATTACK1				0"
	""
	"	Interrupts"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_HEAVY_DAMAGE"
	"		COND_REPEATED_DAMAGE"
	"		COND_HEAR_DANGER"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_OCCLUDED"	// Don't break on this.  Keep shooting at last location
)

AI_END_CUSTOM_NPC()
