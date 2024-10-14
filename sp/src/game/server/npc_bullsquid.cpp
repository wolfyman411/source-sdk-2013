#include "cbase.h"
#include "game.h"
#include "AI_Default.h"
#include "AI_Schedule.h"
#include "AI_Hull.h"
#include "AI_Route.h"
#include "AI_Hint.h"
#include "AI_Navigator.h"
#include "AI_Senses.h"
#include "NPCEvent.h"
#include "animation.h"
#include "npc_bullsquid.h"
#include "gib.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "grenade_spit.h"
#include "util.h"
#include "shake.h"
#include "movevars_shared.h"
#include "decals.h"
#include "hl2_shareddefs.h"
#include "ammodef.h"
#include "ai_behavior.h"
#include "AI_Squad.h"
#include "AI_SquadSlot.h"

#define		SQUID_SPRINT_DIST	256 // how close the squid has to get before starting to sprint and refusing to swerve
#define		SQUID_MAX_SQUAD_SIZE			5
#define		SQUID_SPIT_DISTANCE		2048

ConVar sk_bullsquid_health("sk_bullsquid_health", "125");
ConVar sk_bullsquid_dmg_bite("sk_bullsquid_dmg_bite", "35");
ConVar sk_bullsquid_dmg_whip("sk_bullsquid_dmg_whip", "20");
ConVar sk_bullsquid_dmg_spit("sk_bullsquid_dmg_spit", "25");;

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_SQUID_HURTHOP = LAST_SHARED_SCHEDULE,
	SCHED_SQUID_SEECRAB,
	SCHED_SQUID_EAT,
	SCHED_SQUID_SNIFF_AND_EAT,
	SCHED_SQUID_WALLOW,
	SCHED_SQUID_CHASE_ENEMY,
	SCHED_SQUID_RANGE_ATTACK1,
	SCHED_SQUID_FLANK_RANDOM,
	SCHED_SQUID_RUN_RANDOM,
	SCHED_SQUID_COVER_FROM_ENEMY,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_SQUID_HOPTURN = LAST_SHARED_TASK,
	TASK_SQUID_EAT,
};

//-----------------------------------------------------------------------------
// Squid Conditions
//-----------------------------------------------------------------------------
enum
{
	COND_SQUID_SMELL_FOOD = LAST_SHARED_CONDITION + 1,
};


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		BSQUID_AE_SPIT		( 1 )
#define		BSQUID_AE_BITE		( 2 )
#define		BSQUID_AE_BLINK		( 3 )
#define		BSQUID_AE_TAILWHIP	( 4 )
#define		BSQUID_AE_HOP		( 5 )
#define		BSQUID_AE_THROW		( 6 )

LINK_ENTITY_TO_CLASS(npc_bullsquid, CNPC_Bullsquid);

int ACT_SQUID_EXCITED;
int ACT_SQUID_EAT;
int ACT_SQUID_DETECT_SCENT;
int ACT_SQUID_INSPECT_FLOOR;

BEGIN_DATADESC(CNPC_Bullsquid)
DEFINE_FIELD(m_fCanThreatDisplay, FIELD_BOOLEAN),
DEFINE_FIELD(m_flLastHurtTime, FIELD_TIME),
DEFINE_FIELD(m_flNextSpitTime, FIELD_TIME),

DEFINE_FIELD(m_flHungryTime, FIELD_TIME),

DEFINE_FIELD(m_vecSaveSpitVelocity, FIELD_VECTOR),

END_DATADESC()

bool CNPC_Bullsquid::CreateBehaviors()
{
	AddBehavior( &m_AssaultBehavior );
	AddBehavior( &m_StandoffBehavior );
	AddBehavior( &m_FollowBehavior );
	return BaseClass::CreateBehaviors();
}

//=========================================================
// Spawn
//=========================================================
void CNPC_Bullsquid::Spawn()
{
	Precache();

	SetModel("models/model_xen/bullsquid.mdl");
	SetHullType(HULL_WIDE_SHORT);
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	m_bloodColor = BLOOD_COLOR_GREEN;

	SetRenderColor(255, 255, 255, 255);

	m_iHealth = sk_bullsquid_health.GetFloat();
	m_flFieldOfView = 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState = NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2);
	CapabilitiesAdd(bits_CAP_SQUAD);

	m_fCanThreatDisplay = TRUE;
	m_flNextSpitTime = gpGlobals->curtime;

	NPCInit();

	GetSenses()->SetDistLook(SQUID_SPIT_DISTANCE);
}

int CNPC_Bullsquid::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{
	// Catch the LOF failure and choose another route to take
	if (failedSchedule == SCHED_ESTABLISH_LINE_OF_FIRE)
		return SCHED_SQUID_FLANK_RANDOM;
	if (failedSchedule == SCHED_SQUID_COVER_FROM_ENEMY)
		return SCHED_SQUID_RANGE_ATTACK1;

	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

bool CNPC_Bullsquid::SeenEnemyWithinTime(float flTime)
{
	float flLastSeenTime = GetEnemies()->LastTimeSeen(GetEnemy());
	return (flLastSeenTime != 0.0f && (gpGlobals->curtime - flLastSeenTime) < flTime);
}

bool CNPC_Bullsquid::InnateWeaponLOSCondition(const Vector& ownerPos, const Vector& targetPos, bool bSetConditions)
{
	if (GetNextAttack() > gpGlobals->curtime)
		return false;

	// If we can see the enemy, or we've seen them in the last few seconds just try to lob in there
	if (SeenEnemyWithinTime(3.0f))
	{
		Vector vSpitPos;
		GetAttachment("mouth", vSpitPos);

		return GetSpitVector(vSpitPos, targetPos, &m_vecSaveSpitVelocity,true);
	}

	return BaseClass::InnateWeaponLOSCondition(ownerPos, targetPos, bSetConditions);
}

bool CNPC_Bullsquid::GetSpitVector(const Vector& vecStartPos, const Vector& vecTarget, Vector* vecOut, bool lobbing = false)
{
	float spitSpeed = 800.0f;

	if (lobbing)
	{
		spitSpeed = 400.0f;
	}

	Vector vecToss = VecCheckThrowTolerance(this, vecStartPos, vecTarget, spitSpeed, (10.0f * 12.0f), lobbing);

	// If this failed then try a little faster (flattens the arc)
	if (vecToss == vec3_origin)
	{
		vecToss = VecCheckThrowTolerance(this, vecStartPos, vecTarget, spitSpeed * 1.5f, (10.0f * 12.0f), lobbing);
		if (vecToss == vec3_origin)
			return false;
	}

	// Save out the result
	if (vecOut)
	{
		*vecOut = vecToss;
	}

	return true;
}

Vector CNPC_Bullsquid::VecCheckThrowTolerance(CBaseEntity* pEdict, const Vector& vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance, bool lobbing)
{
	flSpeed = MAX(1.0f, flSpeed);

	float flGravity = GetCurrentGravity();

	Vector vecGrenadeVel = (vecSpot2 - vecSpot1);

	// throw at a constant time
	float time = vecGrenadeVel.Length() / flSpeed;
	vecGrenadeVel = vecGrenadeVel * (2.0 / time);

	// adjust upward toss to compensate for gravity loss
	vecGrenadeVel.z += flGravity * time * 0.27;

	Vector vecApex = vecSpot1 + (vecSpot2 - vecSpot1) * 0.3;
	vecApex.z += 0.3 * flGravity * (time * 0.3) * (time * 0.3);

	if (lobbing)
	{
		float testVar = 0.427f;
		vecGrenadeVel = vecGrenadeVel * (1.0 / time);
		vecGrenadeVel.z += flGravity * time * testVar;

		vecApex = vecSpot1 + (vecSpot2 - vecSpot1) * testVar;
		vecApex.z += testVar * flGravity * (time * testVar) * (time * testVar);
	}


	trace_t tr;
	UTIL_TraceLine(vecSpot1, vecApex, MASK_SOLID, pEdict, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction != 1.0)
	{

		return vec3_origin;
	}

	UTIL_TraceLine(vecApex, vecSpot2, MASK_SOLID_BRUSHONLY, pEdict, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		bool bFail = true;

		// Didn't make it all the way there, but check if we're within our tolerance range
		if (flTolerance > 0.0f)
		{
			float flNearness = (tr.endpos - vecSpot2).LengthSqr();
			if (flNearness < Square(flTolerance))
			{
				bFail = false;
			}
		}

		if (bFail)
		{
			return vec3_origin;
		}
	}

	return vecGrenadeVel;
}

void CNPC_Bullsquid::Precache()
{
	BaseClass::Precache();

	PrecacheModel("models/model_xen/bullsquid.mdl");

	PrecacheScriptSound("Bullsquid.Idle");
	PrecacheScriptSound("Bullsquid.Pain");
	PrecacheScriptSound("Bullsquid.Alert");
	PrecacheScriptSound("Bullsquid.Die");
	PrecacheScriptSound("Bullsquid.Attack");
	PrecacheScriptSound("Bullsquid.Bite");
	PrecacheScriptSound("Bullsquid.Growl");
	PrecacheScriptSound("NPC_Antlion.PoisonShoot");
	PrecacheScriptSound("NPC_Antlion.PoisonBall");

	UTIL_PrecacheOther("grenade_spit");
}


int CNPC_Bullsquid::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_CHASE_ENEMY:
		return SCHED_SQUID_CHASE_ENEMY;
		break;
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this monster's place in the relationship table.
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Bullsquid::Classify(void)
{
	return CLASS_BULLSQUID;
}

//=========================================================
// IdleSound 
//=========================================================
#define SQUID_ATTN_IDLE	(float)1.5
void CNPC_Bullsquid::IdleSound(void)
{
	CPASAttenuationFilter filter(this, SQUID_ATTN_IDLE);
	EmitSound(filter, entindex(), "Bullsquid.Idle");
}

//=========================================================
// PainSound 
//=========================================================
void CNPC_Bullsquid::PainSound(const CTakeDamageInfo& info)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Bullsquid.Pain");
}

//=========================================================
// AlertSound
//=========================================================
void CNPC_Bullsquid::AlertSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Bullsquid.Alert");
}

//=========================================================
// DeathSound
//=========================================================
void CNPC_Bullsquid::DeathSound(const CTakeDamageInfo& info)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Bullsquid.Die");
}

//=========================================================
// AttackSound
//=========================================================
void CNPC_Bullsquid::AttackSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Bullsquid.Attack");
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CNPC_Bullsquid::MaxYawSpeed(void)
{
	float flYS = 0;

	switch (GetActivity())
	{
	case	ACT_WALK:			flYS = 90;	break;
	case	ACT_RUN:			flYS = 90;	break;
	case	ACT_IDLE:			flYS = 90;	break;
	case	ACT_RANGE_ATTACK1:	flYS = 90;	break;
	default:
		flYS = 90;
		break;
	}

	return flYS;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Bullsquid::HandleAnimEvent(animevent_t* pEvent)
{
	switch (pEvent->event)
	{
	case BSQUID_AE_SPIT:
	{
		if (GetEnemy())
		{
			Vector vSpitPos;
			GetAttachment("Mouth", vSpitPos);

			Vector	vTarget;

			// If our enemy is looking at us and far enough away, lead him
			if (HasCondition(COND_ENEMY_FACING_ME) && UTIL_DistApprox(GetAbsOrigin(), GetEnemy()->GetAbsOrigin()) > (40 * 12))
			{
				UTIL_PredictedPosition(GetEnemy(), 0.5f, &vTarget);
				vTarget.z = GetEnemy()->GetAbsOrigin().z;
			}
			else
			{
				// Otherwise he can't see us and he won't be able to dodge
				vTarget = GetEnemy()->BodyTarget(vSpitPos, true);
			}

			vTarget[2] += random->RandomFloat(0.0f, 32.0f);

			// Get dot with upwards to determine lob
			Vector direction = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin());
			direction.NormalizeInPlace();

			float dotProduct = DotProduct(direction, Vector(0, 0, 1));

			Msg("%f\n", dotProduct);

			// Try and spit at our target
			Vector	vecToss;
			if (GetSpitVector(vSpitPos, vTarget, &vecToss, dotProduct < -0.4f) == false)
			{
				// Now try where they were
				if (GetSpitVector(vSpitPos, m_vSavePosition, &vecToss, dotProduct < -0.4f) == false)
				{
					// Failing that, just shoot with the old velocity we calculated initially!
					vecToss = m_vecSaveSpitVelocity;
				}
			}

			// Find what our vertical theta is to estimate the time we'll impact the ground
			Vector vecToTarget = (vTarget - vSpitPos);
			VectorNormalize(vecToTarget);
			float flVelocity = VectorNormalize(vecToss);
			float flCosTheta = DotProduct(vecToTarget, vecToss);
			float flTime = (vSpitPos - vTarget).Length2D() / (flVelocity * flCosTheta);

			// Emit a sound where this is going to hit so that targets get a chance to act correctly
			CSoundEnt::InsertSound(SOUND_DANGER, vTarget, (15 * 12), flTime, this);

			// Don't fire again until this volley would have hit the ground (with some lag behind it)
			SetNextAttack(gpGlobals->curtime + flTime + random->RandomFloat(0.5f, 2.0f));

			CGrenadeSpit* pGrenade = (CGrenadeSpit*)CreateEntityByName("grenade_spit");
			pGrenade->SetAbsOrigin(vSpitPos);
			pGrenade->SetAbsAngles(vec3_angle);
			DispatchSpawn(pGrenade);
			pGrenade->SetThrower(this);
			pGrenade->SetOwnerEntity(this);

			pGrenade->SetSpitSize(SPIT_LARGE);
			pGrenade->SetAbsVelocity(vecToss * flVelocity);

			// Tumble through the air
			pGrenade->SetLocalAngularVelocity(
				QAngle(random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500)));

			EmitSound("NPC_Antlion.PoisonShoot");
		}
	}
	break;

	case BSQUID_AE_BITE:
	{
		// SOUND HERE!
		CBaseEntity* pHurt = CheckTraceHullAttack(70, Vector(-16, -16, -16), Vector(16, 16, 16), sk_bullsquid_dmg_bite.GetFloat(), DMG_SLASH);
		if (pHurt)
		{
			Vector forward, up;
			AngleVectors(GetAbsAngles(), &forward, NULL, &up);
			pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() - (forward * 100));
			pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() + (up * 100));
			pHurt->SetGroundEntity(NULL);
		}
	}
	break;

	case BSQUID_AE_TAILWHIP:
	{
		CBaseEntity* pHurt = CheckTraceHullAttack(70, Vector(-16, -16, -16), Vector(16, 16, 16), sk_bullsquid_dmg_whip.GetFloat(), DMG_CLUB | DMG_ALWAYSGIB);
		if (pHurt)
		{
			Vector right, up;
			AngleVectors(GetAbsAngles(), NULL, &right, &up);

			if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
				pHurt->ViewPunch(QAngle(20, 0, -20));

			pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() + (right * 200));
			pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() + (up * 100));
		}
	}
	break;

	case BSQUID_AE_BLINK:
	{
		// close eye. 
		m_nSkin = 1;
	}
	break;

	case BSQUID_AE_HOP:
	{
		float flGravity = sv_gravity.GetFloat();

		// throw the squid up into the air on this frame.
		if (GetFlags() & FL_ONGROUND)
		{
			SetGroundEntity(NULL);
		}

		// jump into air for 0.8 (24/30) seconds
		Vector vecVel = GetAbsVelocity();
		vecVel.z += (0.625 * flGravity) * 0.5;
		SetAbsVelocity(vecVel);
	}
	break;

	case BSQUID_AE_THROW:
	{
		// squid throws its prey IF the prey is a client. 
		CBaseEntity* pHurt = CheckTraceHullAttack(70, Vector(-16, -16, -16), Vector(16, 16, 16), 0, 0);


		if (pHurt)
		{
			// croonchy bite sound
			CPASAttenuationFilter filter(this);
			EmitSound(filter, entindex(), "Bullsquid.Bite");

			// screeshake transforms the viewmodel as well as the viewangle. No problems with seeing the ends of the viewmodels.
			UTIL_ScreenShake(pHurt->GetAbsOrigin(), 25.0, 1.5, 0.7, 2, SHAKE_START);

			if (pHurt->IsPlayer())
			{
				Vector forward, up;
				AngleVectors(GetAbsAngles(), &forward, NULL, &up);

				pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() + forward * 300 + up * 300);
			}
		}
	}
	break;

	default:
		BaseClass::HandleAnimEvent(pEvent);
	}
}

int CNPC_Bullsquid::RangeAttack1Conditions(float flDot, float flDist)
{
	if (GetNextAttack() > gpGlobals->curtime)
	{
		Msg("I failed 1\n");
		return COND_NONE;
	}

	if (flDot < DOT_10DEGREE)
	{
		Msg("I failed 2\n");
		return COND_NONE;
	}


	Msg("I can attack\n");
	return COND_CAN_RANGE_ATTACK1;
}

//=========================================================
// MeleeAttack2Conditions - bullsquid is a big guy, so has a longer
// melee range than most monsters. This is the tailwhip attack
//=========================================================
int CNPC_Bullsquid::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (GetEnemy()->m_iHealth <= sk_bullsquid_dmg_whip.GetFloat() && flDist <= 85 && flDot >= 0.7)
	{
		return (COND_CAN_MELEE_ATTACK1);
	}

	return(COND_NONE);
}

//=========================================================
// MeleeAttack2Conditions - bullsquid is a big guy, so has a longer
// melee range than most monsters. This is the bite attack.
// this attack will not be performed if the tailwhip attack
// is valid.
//=========================================================
int CNPC_Bullsquid::MeleeAttack2Conditions(float flDot, float flDist)
{
	if (flDist <= 85 && flDot >= 0.7 && !HasCondition(COND_CAN_MELEE_ATTACK1))		// The player & bullsquid can be as much as their bboxes 
		return (COND_CAN_MELEE_ATTACK2);

	return(COND_NONE);
}

bool CNPC_Bullsquid::FValidateHintType(CAI_Hint* pHint)
{
	if (pHint->HintType() == HINT_HL1_WORLD_HUMAN_BLOOD)
		return true;

	Msg("Couldn't validate hint type");

	return false;
}

void CNPC_Bullsquid::RemoveIgnoredConditions(void)
{
	if (m_flHungryTime > gpGlobals->curtime)
		ClearCondition(COND_SQUID_SMELL_FOOD);

	if (gpGlobals->curtime - m_flLastHurtTime <= 20)
	{
		// haven't been hurt in 20 seconds, so let the squid care about stink. 
		ClearCondition(COND_SMELL);
	}

	if (GetEnemy() != NULL)
	{
		// ( Unless after a tasty headcrab, yumm ^_^ )
		if (FClassnameIs(GetEnemy(), "npc_headcrab"))
			ClearCondition(COND_SMELL);
	}
}

Disposition_t CNPC_Bullsquid::IRelationType(CBaseEntity* pTarget)
{
	if (gpGlobals->curtime - m_flLastHurtTime < 5 && FClassnameIs(pTarget, "npc_headcrab"))
	{
		// if squid has been hurt in the last 5 seconds, and is getting relationship for a headcrab, 
		// tell squid to disregard crab. 
		return D_NU;
	}

	return BaseClass::IRelationType(pTarget);
}

//=========================================================
// TakeDamage - overridden for bullsquid so we can keep track
// of how much time has passed since it was last injured
//=========================================================
int CNPC_Bullsquid::OnTakeDamage_Alive(const CTakeDamageInfo& inputInfo)
{
	if (!FClassnameIs(inputInfo.GetAttacker(), "npc_headcrab"))
	{
		// don't forget about headcrabs if it was a headcrab that hurt the squid.
		m_flLastHurtTime = gpGlobals->curtime;
	}

	return BaseClass::OnTakeDamage_Alive(inputInfo);
}

//=========================================================
// GetSoundInterests - returns a bit mask indicating which types
// of sounds this monster regards. In the base class implementation,
// monsters care about all sounds, but no scents.
//=========================================================
int CNPC_Bullsquid::GetSoundInterests(void)
{
	return	SOUND_WORLD |
		SOUND_COMBAT |
		SOUND_CARCASS |
		SOUND_MEAT |
		SOUND_GARBAGE |
		SOUND_PLAYER;
}

//=========================================================
// OnListened - monsters dig through the active sound list for
// any sounds that may interest them. (smells, too!)
//=========================================================
void CNPC_Bullsquid::OnListened(void)
{
	AISoundIter_t iter;

	CSound* pCurrentSound;

	static int conditionsToClear[] =
	{
		COND_SQUID_SMELL_FOOD,
	};

	ClearConditions(conditionsToClear, ARRAYSIZE(conditionsToClear));

	pCurrentSound = GetSenses()->GetFirstHeardSound(&iter);

	while (pCurrentSound)
	{
		// the npc cares about this sound, and it's close enough to hear.
		int condition = COND_NONE;

		if (!pCurrentSound->FIsSound())
		{
			// if not a sound, must be a smell - determine if it's just a scent, or if it's a food scent
			if (pCurrentSound->IsSoundType(SOUND_MEAT | SOUND_CARCASS))
			{
				// the detected scent is a food item
				condition = COND_SQUID_SMELL_FOOD;
			}
		}

		if (condition != COND_NONE)
			SetCondition(condition);

		pCurrentSound = GetSenses()->GetNextHeardSound(&iter);
	}

	BaseClass::OnListened();
}

//========================================================
// RunAI - overridden for bullsquid because there are things
// that need to be checked every think.
//========================================================
void CNPC_Bullsquid::RunAI(void)
{
	// first, do base class stuff
	BaseClass::RunAI();

	if (m_nSkin != 0)
	{
		// close eye if it was open.
		m_nSkin = 0;
	}

	if (random->RandomInt(0, 39) == 0)
	{
		m_nSkin = 1;
	}

	if (GetEnemy() != NULL && GetActivity() == ACT_RUN)
	{
		// chasing enemy. Sprint for last bit
		if ((GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).Length2D() < SQUID_SPRINT_DIST)
		{
			m_flPlaybackRate = 1.25;
		}
	}

}

//=========================================================
// GetSchedule 
//=========================================================
int CNPC_Bullsquid::SelectSchedule(void)
{
	switch (m_NPCState)
	{
		case NPC_STATE_ALERT:
		{
			if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE))
			{
				return SCHED_SQUID_HURTHOP;
			}

			if (HasCondition(COND_SQUID_SMELL_FOOD))
			{
				CSound* pSound;

				pSound = GetBestScent();

				if (pSound && (!FInViewCone(pSound->GetSoundOrigin()) || !FVisible(pSound->GetSoundOrigin())))
				{
					// scent is behind or occluded
					return SCHED_SQUID_SNIFF_AND_EAT;
				}

				// food is right out in the open. Just go get it.
				return SCHED_SQUID_EAT;
			}

			if (HasCondition(COND_SMELL))
			{
				// there's something stinky. 
				CSound* pSound;

				pSound = GetBestScent();
				if (pSound)
					return SCHED_SQUID_WALLOW;
			}

			break;
		}
		case NPC_STATE_COMBAT:
		{
			if (HasCondition(COND_CAN_RANGE_ATTACK1))
			{
				return SCHED_SQUID_RANGE_ATTACK1;
			}

			// dead enemy
			if (HasCondition(COND_ENEMY_DEAD))
			{
				// call base class, all code to handle dead enemies is centralized there.
				return BaseClass::SelectSchedule();
			}

			if (HasCondition(COND_NEW_ENEMY))
			{
				if (m_fCanThreatDisplay && IRelationType(GetEnemy()) == D_HT && FClassnameIs(GetEnemy(), "monster_headcrab"))
				{
					// this means squid sees a headcrab!
					m_fCanThreatDisplay = FALSE;// only do the headcrab dance once per lifetime.
					return SCHED_SQUID_SEECRAB;
				}
				else
				{
					return SCHED_WAKE_ANGRY;
				}
			}

			if (HasCondition(COND_SQUID_SMELL_FOOD))
			{
				CSound* pSound;

				pSound = GetBestScent();

				if (pSound && (!FInViewCone(pSound->GetSoundOrigin()) || !FVisible(pSound->GetSoundOrigin())))
				{
					// scent is behind or occluded
					return SCHED_SQUID_SNIFF_AND_EAT;
				}

				// food is right out in the open. Just go get it.
				return SCHED_SQUID_EAT;
			}

			if (HasCondition(COND_CAN_MELEE_ATTACK1))
			{
				return SCHED_MELEE_ATTACK1;
			}

			if (HasCondition(COND_CAN_MELEE_ATTACK2))
			{
				return SCHED_MELEE_ATTACK2;
			}

			return SCHED_CHASE_ENEMY;

			break;
		}
	}
	if (m_AssaultBehavior.CanSelectSchedule())
	{
		DeferSchedulingToBehavior(&m_AssaultBehavior);
		return BaseClass::SelectSchedule();
	}

	return BaseClass::SelectSchedule();
}

//=========================================================
// FInViewCone - returns true is the passed vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
bool CNPC_Bullsquid::FInViewCone(Vector pOrigin)
{
	Vector los = (pOrigin - GetAbsOrigin());

	// do this in 2D
	los.z = 0;
	VectorNormalize(los);

	Vector facingDir = EyeDirection2D();

	float flDot = DotProduct(los, facingDir);

	if (flDot > m_flFieldOfView)
		return true;

	return false;
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target vector
//=========================================================
bool CNPC_Bullsquid::FVisible(Vector vecOrigin)
{
	trace_t tr;
	Vector		vecLookerOrigin;

	vecLookerOrigin = EyePosition();//look through the caller's 'eyes'
	UTIL_TraceLine(vecLookerOrigin, vecOrigin, MASK_BLOCKLOS, this/*pentIgnore*/, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction != 1.0)
		return false; // Line of sight is not established
	else
		return true;// line of sight is valid.
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule.  OVERRIDDEN for bullsquid because it needs to
// know explicitly when the last attempt to chase the enemy
// failed, since that impacts its attack choices.
//=========================================================
void CNPC_Bullsquid::StartTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_MELEE_ATTACK2:
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "Bullsquid.Growl");
		BaseClass::StartTask(pTask);
		break;
	}
	case TASK_SQUID_HOPTURN:
	{
		SetActivity(ACT_HOP);

		if (GetEnemy())
		{
			Vector	vecFacing = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			VectorNormalize(vecFacing);

			GetMotor()->SetIdealYaw(vecFacing);
		}

		break;
	}
	case TASK_SQUID_EAT:
	{
		m_flHungryTime = gpGlobals->curtime + pTask->flTaskData;
		break;
	}

	default:
	{
		BaseClass::StartTask(pTask);
		break;
	}
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_Bullsquid::RunTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_SQUID_HOPTURN:
	{
		if (GetEnemy())
		{
			Vector	vecFacing = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			VectorNormalize(vecFacing);
			GetMotor()->SetIdealYaw(vecFacing);
		}

		if (IsSequenceFinished())
		{
			TaskComplete();
		}
		break;
	}
	default:
	{
		BaseClass::RunTask(pTask);
		break;
	}
	}
}

//=========================================================
// GetIdealState - Overridden for Bullsquid to deal with
// the feature that makes it lose interest in headcrabs for 
// a while if something injures it. 
//=========================================================
NPC_STATE CNPC_Bullsquid::SelectIdealState(void)
{
	// If no schedule conditions, the new ideal state is probably the reason we're in here.
	switch (m_NPCState)
	{
	case NPC_STATE_COMBAT:
		/*
		COMBAT goes to ALERT upon death of enemy
		*/
	{
		if (GetEnemy() != NULL && (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE)) && FClassnameIs(GetEnemy(), "monster_headcrab"))
		{
			// if the squid has a headcrab enemy and something hurts it, it's going to forget about the crab for a while.
			SetEnemy(NULL);
			return NPC_STATE_ALERT;
		}
		break;
	}
	}

	return BaseClass::SelectIdealState();
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC(npc_bullsquid, CNPC_Bullsquid)

DECLARE_TASK(TASK_SQUID_HOPTURN)
DECLARE_TASK(TASK_SQUID_EAT)

DECLARE_CONDITION(COND_SQUID_SMELL_FOOD)

DECLARE_ACTIVITY(ACT_SQUID_EXCITED)
DECLARE_ACTIVITY(ACT_SQUID_EAT)
DECLARE_ACTIVITY(ACT_SQUID_DETECT_SCENT)
DECLARE_ACTIVITY(ACT_SQUID_INSPECT_FLOOR)

//=========================================================
// > SCHED_SQUID_HURTHOP
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SQUID_HURTHOP,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_SOUND_WAKE				0"
	"		TASK_SQUID_HOPTURN			0"
	"		TASK_FACE_ENEMY				0"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_SQUID_SEECRAB
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SQUID_SEECRAB,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_SOUND_WAKE				0"
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_SQUID_EXCITED"
	"		TASK_FACE_ENEMY				0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
)

//=========================================================
// > SCHED_SQUID_EAT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SQUID_EAT,

	"	Tasks"
	"		TASK_STOP_MOVING					0"
	"		TASK_SQUID_EAT						10"
	"		TASK_STORE_LASTPOSITION				0"
	"		TASK_GET_PATH_TO_BESTSCENT			0"
	"		TASK_WALK_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_EAT"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_EAT"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_EAT"
	"		TASK_SQUID_EAT						50"
	"		TASK_GET_PATH_TO_LASTPOSITION		0"
	"		TASK_WALK_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_CLEAR_LASTPOSITION				0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NEW_ENEMY"
	"		COND_SMELL"
)

//=========================================================
// > SCHED_SQUID_SNIFF_AND_EAT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SQUID_SNIFF_AND_EAT,

	"	Tasks"
	"		TASK_STOP_MOVING					0"
	"		TASK_SQUID_EAT						10"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_DETECT_SCENT"
	"		TASK_STORE_LASTPOSITION				0"
	"		TASK_GET_PATH_TO_BESTSCENT			0"
	"		TASK_WALK_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_EAT"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_EAT"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_SQUID_EAT"
	"		TASK_SQUID_EAT						50"
	"		TASK_GET_PATH_TO_LASTPOSITION		0"
	"		TASK_WALK_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_CLEAR_LASTPOSITION				0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NEW_ENEMY"
	"		COND_SMELL"
)

//=========================================================
// > SCHED_SQUID_WALLOW
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SQUID_WALLOW,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_SQUID_EAT					10"
	"		TASK_STORE_LASTPOSITION			0"
	"		TASK_GET_PATH_TO_BESTSCENT		0"
	"		TASK_WALK_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_SQUID_INSPECT_FLOOR"
	"		TASK_SQUID_EAT					50"
	"		TASK_GET_PATH_TO_LASTPOSITION	0"
	"		TASK_WALK_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_CLEAR_LASTPOSITION			0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NEW_ENEMY"
)

//=========================================================
// > SCHED_SQUID_CHASE_ENEMY
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SQUID_CHASE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ESTABLISH_LINE_OF_FIRE"
	"		TASK_GET_PATH_TO_ENEMY			0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_SMELL"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_TASK_FAILED"
)
DEFINE_SCHEDULE
(
	SCHED_SQUID_RANGE_ATTACK1,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_RANGE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_FLANK_RANDOM,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_SQUID_RUN_RANDOM"
	"		TASK_SET_TOLERANCE_DISTANCE				48"
	"		TASK_SET_ROUTE_SEARCH_TIME				1"	// Spend 1 second trying to build a path if stuck
	"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	30"
	"		TASK_RUN_PATH							0"
	"		TASK_WAIT_FOR_MOVEMENT					0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_HEAVY_DAMAGE"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_COND_SEE_ENEMY"
	"		COND_CAN_MELEE_ATTACK1"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_RUN_RANDOM,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_SQUID_COVER_FROM_ENEMY"
	"		TASK_SET_TOLERANCE_DISTANCE		48"
	"		TASK_SET_ROUTE_SEARCH_TIME		1"	// Spend 1 second trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE	128"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_COND_SEE_ENEMY"
	"		COND_CAN_RANGE_ATTACK1"
)
DEFINE_SCHEDULE
(
	SCHED_SQUID_COVER_FROM_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_TAKE_COVER"
	"		TASK_FIND_COVER_FROM_ENEMY		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	""
	"	Interrupts"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
)

AI_END_CUSTOM_NPC()
