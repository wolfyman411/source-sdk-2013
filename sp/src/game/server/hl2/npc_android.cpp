
#include "cbase.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_behavior_follow.h"
#include "npc_android.h"
#include "ai_schedule.h"
#include "util.h"
#include "tier0/memdbgon.h"
#include <particle_parse.h>
#include <ammodef.h>
#include "npc_androidball.h"

//Activities
Activity ACT_SWAP_LEFT_WPN;
Activity ACT_SWAP_RIGHT_WPN;
Activity ACT_ZAPPED;
Activity ACT_BALL;
Activity ACT_UNBALL;

int AE_ANDROID_SWAP_RIGHT;
int AE_ANDROID_SWAP_LEFT;

#define ANDROID_MODEL "models/aperture/android.mdl"
#define CLOSE_RANGE 200.0f
#define FAR_RANGE 700.0f
#define LASER_DURATION 3.0f
#define SHOT_DELAY 0.1f
#define SHOT_AMOUNT 3
#define ATTACK_DELAY 0.3f
#define ZAP_STUN 5.0f

#define WEAPON_ATTACHMENT_LEFT	1
#define WEAPON_ATTACHMENT_RIGHT	2
#define PARTICLE_ATTACHMENT		"particles"

ConVar sk_android_health("sk_android_health", "400");

LINK_ENTITY_TO_CLASS(npc_android, CNPC_Android);

BEGIN_DATADESC(CNPC_Android)

DEFINE_FIELD(left_wpn, FIELD_INTEGER),
DEFINE_FIELD(right_wpn, FIELD_INTEGER),

DEFINE_FIELD(m_gunShotsL, FIELD_INTEGER),
DEFINE_FIELD(m_gunShotsR, FIELD_INTEGER),

DEFINE_FIELD(m_nextSwapL, FIELD_INTEGER),
DEFINE_FIELD(m_nextSwapR, FIELD_INTEGER),

DEFINE_FIELD(m_attackDurL, FIELD_FLOAT),
DEFINE_FIELD(m_attackDurR, FIELD_FLOAT),

DEFINE_FIELD(m_nextAttackL, FIELD_FLOAT),
DEFINE_FIELD(m_nextAttackR, FIELD_FLOAT),

DEFINE_FIELD(m_laserStartpoint, FIELD_VECTOR),
DEFINE_FIELD(m_laserEndpoint, FIELD_VECTOR),
DEFINE_FIELD(m_laserTarget, FIELD_VECTOR),
DEFINE_FIELD(laserAccuracy, FIELD_FLOAT),

DEFINE_FIELD(m_pSmokeTrail, FIELD_CLASSPTR),
DEFINE_FIELD(m_pFire, FIELD_CLASSPTR),
DEFINE_FIELD(m_pBeamL, FIELD_CLASSPTR),
DEFINE_FIELD(m_pBeamR, FIELD_CLASSPTR),
DEFINE_FIELD(m_pLightGlowR, FIELD_CLASSPTR),
DEFINE_FIELD(m_pLightGlowL, FIELD_CLASSPTR),
DEFINE_FIELD(m_pLightGlowEnd, FIELD_CLASSPTR),

DEFINE_FIELD(m_zapped, FIELD_BOOLEAN),



//-----------------------------------------------------------------------------
DEFINE_KEYFIELD(forced_left, FIELD_INTEGER,"forced_left"),
DEFINE_KEYFIELD(forced_right, FIELD_INTEGER, "forced_right"),

END_DATADESC()

//-----------------------------------------------------------------------------
// Classify - indicates this NPC's place in the 
// relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Android::Classify(void)
{
	return CLASS_APERTURE;
}

void CNPC_Android::SetVars(float health, Android_Weapons_e leftWeapon, Android_Weapons_e rightWeapon)
{
	SetHealth(health);
	forced_left = leftWeapon;
	forced_right = rightWeapon;

	//Update HP state
	CTakeDamageInfo newinfo;
	TakeDamage(newinfo);
}


//-----------------------------------------------------------------------------
// HandleAnimEvent - catches the NPC-specific messages
// that occur when tagged animation frames are played.
//-----------------------------------------------------------------------------
void CNPC_Android::HandleAnimEvent(animevent_t* pEvent)
{
	if (pEvent->event == AE_ANDROID_SWAP_LEFT)
	{
		SetBodygroup(1, left_wpn + 1);
		return;
	}

	if (pEvent->event == AE_ANDROID_SWAP_RIGHT)
	{
		SetBodygroup(2, right_wpn + 1);
		return;
	}
	BaseClass::HandleAnimEvent(pEvent);
}

//-----------------------------------------------------------------------------
// GetSoundInterests - generic NPC can't hear.
//-----------------------------------------------------------------------------
int CNPC_Android::GetSoundInterests(void)
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CNPC_Android::Spawn()
{
	Precache();

	SetModel(ANDROID_MODEL);

	SetHullType(HULL_MEDIUM);
	SetHullSizeNormal();
	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);

	SetMoveType(MOVETYPE_STEP);
	SetNavType(NAV_GROUND);

	SetBloodColor(BLOOD_COLOR_MECH);

	m_iHealth = sk_android_health.GetFloat();

	m_flFieldOfView = 0.8;
	m_NPCState = NPC_STATE_NONE;

	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_TURN_HEAD | bits_CAP_INNATE_RANGE_ATTACK1);

	if (m_SquadName != NULL_STRING)
	{
		CapabilitiesAdd(bits_CAP_SQUAD);
	}

	NPCInit();

	m_flDistTooFar = FLT_MAX;
	SetBodygroup(1, 1);
	SetBodygroup(2, 1);

	BaseClass::Spawn();
}

void CNPC_Android::Activate()
{
	BaseClass::Activate();
	if (m_startBalled)
	{
		DevMsg("unball\n");
		m_startBalled = false;
		SetActivity(ACT_UNBALL);
		SetSchedule(SCHED_ANDROID_UNBALL);
		SetActivity(ACT_UNBALL);
		SetIdealActivity(ACT_UNBALL);
	}
}

//-----------------------------------------------------------------------------
// Precache - precaches all resources this NPC needs
//-----------------------------------------------------------------------------
void CNPC_Android::Precache()
{
	PrecacheScriptSound("NPC_Stalker.BurnWall");

	PrecacheParticleSystem("hunter_muzzle_flash");

	PrecacheModel(ANDROID_MODEL);
	m_attackDurL = 0.0f;
	m_attackDurR = 0.0f;
	m_nextAttackL = 0.0f;
	m_nextAttackR = 0.0f;

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
float CNPC_Android::GetIdealAccel(void) const
{
	return GetIdealSpeed() * 5.0;
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

//=========================================================
// Purpose:
//=========================================================
bool CNPC_Android::CreateBehaviors()
{
	AddBehavior(&m_FollowBehavior);

	return BaseClass::CreateBehaviors();
}

int CNPC_Android::OnTakeDamage_Alive(const CTakeDamageInfo& info)
{
	CTakeDamageInfo newinfo = info;

	// Zap our robot if physgun
	if (newinfo.GetDamageType() & (DMG_PHYSGUN) && !m_zapped)
	{
		Zap();
	}


	// Start smoking when we're nearly dead
	if (m_iHealth < m_iMaxHealth/2)
	{
		StartSmokeTrail();
	}
	if (m_iHealth < m_iMaxHealth / 4)
	{
		StartFire();
	}

	// If zapped, deal insane damage
	if (m_zapped && IsCurSchedule(SCHED_ANDROID_ZAP))
	{
		newinfo.SetDamage(newinfo.GetDamage() * 5.0f);
	}

	return (BaseClass::OnTakeDamage_Alive(newinfo));
}

void CNPC_Android::Zap(void)
{
	//Cannot be zapped already
	if (m_zapped)
		return;

	// Must be on the ground
	if ((GetFlags() & FL_ONGROUND) == false)
		return;

	// Can't be in a dynamic interation
	if (IsRunningDynamicInteraction())
		return;

	KillLaser(m_pBeamR, m_pLightGlowR);
	KillLaser(m_pBeamL, m_pLightGlowL);

	ClearSchedule("Zapped");
	SetCondition(COND_ANDROID_ZAPPED);
	m_zapped = true;
	m_zaptime = gpGlobals->curtime + ZAP_STUN;
}

void CNPC_Android::StartSmokeTrail(void)
{
	if (m_pSmokeTrail != NULL)
		return;

	m_pSmokeTrail = SmokeTrail::CreateSmokeTrail();

	if (m_pSmokeTrail)
	{
		m_pSmokeTrail->m_SpawnRate = 10;
		m_pSmokeTrail->m_ParticleLifetime = 1;
		m_pSmokeTrail->m_StartSize = 8;
		m_pSmokeTrail->m_EndSize = 50;
		m_pSmokeTrail->m_SpawnRadius = 10;
		m_pSmokeTrail->m_MinSpeed = 25;
		m_pSmokeTrail->m_MaxSpeed = 45;

		m_pSmokeTrail->m_StartColor.Init(1.0f, 1.0f, 1.0f);
		m_pSmokeTrail->m_EndColor.Init(0, 0, 0);
		m_pSmokeTrail->SetLifetime(500.0f);

		m_pSmokeTrail->FollowEntity(this,PARTICLE_ATTACHMENT);
	}
}

void CNPC_Android::StartFire(void)
{
	m_pFire = (CParticleSystem*)CreateEntityByName("info_particle_system");
	if (m_pFire != NULL)
	{
		Vector vecUp;
		GetVectors(NULL, NULL, &vecUp);

		// Setup our basic parameters
		m_pFire->KeyValue("start_active", "1");
		m_pFire->KeyValue("effect_name", "explosion_turret_fizzle");
		m_pFire->SetParent(this);
		m_pFire->FollowEntity(this, PARTICLE_ATTACHMENT);
		m_pFire->SetAbsOrigin(WorldSpaceCenter() + (vecUp * 5.0f));
		DispatchSpawn(m_pFire);
		m_pFire->Activate();
	}
}

void CNPC_Android::Event_Killed(const CTakeDamageInfo& info)
{
	SetCondition(COND_SCHEDULE_DONE);

	Gib();
}

void CNPC_Android::Gib(void)
{
	if (IsMarkedForDeletion())
		return;

	// Light
	CBroadcastRecipientFilter filter;
	te->DynamicLight(filter, 0.0, &WorldSpaceCenter(), 255, 180, 100, 0, 100, 0.1, 0);

	// Cover the gib spawn
	ExplosionCreate(WorldSpaceCenter(), GetAbsAngles(), this, 64, 64, false);

	// Turn off any smoke trail
	if (m_pSmokeTrail)
	{
		m_pSmokeTrail->m_ParticleLifetime = 0;
		UTIL_Remove(m_pSmokeTrail);
		m_pSmokeTrail = NULL;
	}

	if (m_pFire)
	{
		UTIL_Remove(m_pFire);
	}

	KillLaser(m_pBeamL, m_pLightGlowL);
	KillLaser(m_pBeamR, m_pLightGlowR);

	Vector vecUp;
	GetVectors(NULL, NULL, &vecUp);
	Vector vecOrigin = WorldSpaceCenter() + (vecUp * 12.0f);
	CPVSFilter filter2(vecOrigin);
	for (int i = 0; i < 15; i++)
	{
		Vector gibVelocity = RandomVector(-100, 100);
		int iModelIndex = modelinfo->GetModelIndex(g_PropDataSystem.GetRandomChunkModel("MetalChunks"));
		te->BreakModel(filter2, 0.0, vecOrigin, GetAbsAngles(), Vector(40, 40, 40), gibVelocity, iModelIndex, 150, 4, 2.5, BREAK_METAL);
	}

	UTIL_Remove(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::SelectSchedule(void)
{

	if (HasCondition(COND_ANDROID_ZAPPED))
	{
		ClearCondition(COND_ANDROID_ZAPPED);
		return SCHED_ANDROID_ZAP;
	}

	if (HasCondition(COND_TOO_FAR_TO_ATTACK))
	{
		return SCHED_ANDROID_BALL_MODE;
	}

	if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
	{
		return SCHED_MOVE_AWAY_FROM_ENEMY;
	}

	switch (m_NPCState)
	{
		case NPC_STATE_COMBAT:
		{
			if (HasCondition(COND_CAN_RANGE_ATTACK1))
			{
				// Swap Weapons every so often
				if (forced_left == ANDROID_NONE)
				{
					if (m_nextSwapL <= 0)
					{
						SwapWeapon(left_wpn);
						m_nextSwapL = RandomInt(2, 4);

						AddGesture(ACT_SWAP_LEFT_WPN);
						m_nextAttackL = gpGlobals->curtime + 1.0f;
					}
				}
				else if (left_wpn != forced_left)
				{
					m_nextSwapL = INFINITY;
					left_wpn = forced_left;
					AddGesture(ACT_SWAP_LEFT_WPN);
					m_nextAttackL = gpGlobals->curtime + 1.0f;
				}
				
				if (forced_right == ANDROID_NONE)
				{
					if (m_nextSwapR <= 0)
					{
						SwapWeapon(right_wpn);
						m_nextSwapR = RandomInt(2, 4);

						AddGesture(ACT_SWAP_RIGHT_WPN);
						m_nextAttackR = gpGlobals->curtime + 1.0f;
					}
				}
				else if (right_wpn != forced_right)
				{
					m_nextSwapR = INFINITY;
					right_wpn = forced_right;
					AddGesture(ACT_SWAP_RIGHT_WPN);
					m_nextAttackR = gpGlobals->curtime + 1.0f;
				}

				if (m_nextAttackL < gpGlobals->curtime)
				{
					switch (left_wpn)
					{
						case ANDROID_LASER:
						{
							return SCHED_ANDROID_LASER_ATTACK;
							break;
						}
						case ANDROID_GUN:
						{
							return SCHED_ANDROID_GUN_ATTACK;
							break;
						}
					}
				}
				else if (m_nextAttackR < gpGlobals->curtime)
				{
					switch (right_wpn)
					{
						case ANDROID_LASER:
						{
							return SCHED_ANDROID_LASER_ATTACK;
							break;
						}
						case ANDROID_GUN:
						{
							return SCHED_ANDROID_GUN_ATTACK;
							break;
						}
					}
				}
			}
			else
			{
				return SCHED_CHASE_ENEMY;
			}
			break;
		}
	}

	return BaseClass::SelectSchedule();
}

void CNPC_Android::SwapWeapon(Android_Weapons_e &ref)
{
	switch (ref)
	{
	case ANDROID_GUN:
		ref = ANDROID_LASER;
		break;
	case ANDROID_LASER:
		ref = ANDROID_GUN;
		break;
	default:
		ref = static_cast<Android_Weapons_e>(RandomInt(1, 2));
		break;
	}
}

int CNPC_Android::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_CHASE_ENEMY:
		return SCHED_ANDROID_CHASE_ENEMY;
		break;
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

int CNPC_Android::RangeAttack1Conditions(float flDot, float flDist)
{
	if (GetNextAttack() > gpGlobals->curtime)
		return COND_NOT_FACING_ATTACK;

	if (flDot < DOT_10DEGREE)
		return COND_NOT_FACING_ATTACK;

	if (flDist > FAR_RANGE)
		return COND_TOO_FAR_TO_ATTACK;

	if (flDist < CLOSE_RANGE)
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

void CNPC_Android::StartTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
		case TASK_ANDROID_GUN_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				//Check if both
				if (HasCondition(COND_ANDROID_IS_LEFT) && left_wpn == ANDROID_GUN && left_wpn == right_wpn)
				{
					m_gunShotsL = 0;
					m_nextAttackL = gpGlobals->curtime + ATTACK_DELAY + RandomFloat(0.1f, 0.5f);
					m_attackDurL = gpGlobals->curtime;

					m_gunShotsR = 0;
					m_nextAttackR = m_nextAttackL + 0.1f;;
					m_attackDurR = m_attackDurL;

					m_nextSwapL--;
					m_nextSwapR--;
				}
				else if (HasCondition(COND_ANDROID_IS_LEFT) && left_wpn == ANDROID_GUN)
				{
					m_gunShotsL = 0;
					m_nextAttackL = gpGlobals->curtime + ATTACK_DELAY + RandomFloat(0.1f, 0.5f);
					m_attackDurL = gpGlobals->curtime;

					m_nextSwapL--;
				}
				else if (HasCondition(COND_ANDROID_IS_RIGHT) && right_wpn == ANDROID_GUN)
				{
					m_gunShotsR = 0;
					m_nextAttackR = gpGlobals->curtime + ATTACK_DELAY + RandomFloat(0.1f, 0.5f);
					m_attackDurR = gpGlobals->curtime;

					m_nextSwapR--;
				}
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
			break;
		}

		case TASK_ANDROID_LASER_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				//Check if both
				if (!m_pBeamL && !m_pBeamR && HasCondition(COND_ANDROID_IS_LEFT) && left_wpn == ANDROID_LASER && left_wpn == right_wpn)
				{
					laserAccuracy = 0.0f;
					m_laserStartpoint = CalcEndPoint();

					CreateLaser(m_pBeamL, m_pLightGlowL, WEAPON_ATTACHMENT_LEFT);
					m_attackDurL = gpGlobals->curtime + LASER_DURATION;
					m_nextAttackL = gpGlobals->curtime + LASER_DURATION + ATTACK_DELAY + RandomFloat(0.5f, 2.0f);

					CreateLaser(m_pBeamR, m_pLightGlowR, WEAPON_ATTACHMENT_RIGHT);
					m_attackDurR = m_attackDurL;
					m_nextAttackR = m_nextAttackL + 0.1f;

					m_nextSwapL--;
					m_nextSwapR--;
				}
				else if (!m_pBeamL && HasCondition(COND_ANDROID_IS_LEFT) && left_wpn == ANDROID_LASER)
				{
					laserAccuracy = 0.0f;
					m_laserStartpoint = CalcEndPoint();

					CreateLaser(m_pBeamL, m_pLightGlowL, WEAPON_ATTACHMENT_LEFT);
					m_attackDurL = gpGlobals->curtime + LASER_DURATION;
					m_nextAttackL = gpGlobals->curtime + LASER_DURATION + ATTACK_DELAY + RandomFloat(0.5f,2.0f);

					m_nextSwapL--;
				}
				else if (!m_pBeamR && HasCondition(COND_ANDROID_IS_RIGHT) && right_wpn == ANDROID_LASER)
				{
					laserAccuracy = 0.0f;
					m_laserStartpoint = CalcEndPoint();

					CreateLaser(m_pBeamR, m_pLightGlowR, WEAPON_ATTACHMENT_RIGHT);
					m_attackDurR = gpGlobals->curtime + LASER_DURATION;
					m_nextAttackR = gpGlobals->curtime + LASER_DURATION + ATTACK_DELAY + RandomFloat(0.5f, 2.0f);

					m_nextSwapR--;
				}
				break;
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
		}
		case TASK_ANDROID_BALL_MODE:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget && !m_ballMode)
			{
				//create a new android ball where the android is
				CBaseEntity *newEnt = CreateEntityByName("npc_androidball");
				if (newEnt)
				{
					KillLaser(m_pBeamL, m_pLightGlowL);
					KillLaser(m_pBeamR, m_pLightGlowR);

					m_ballMode = true;
					CNPC_AndroidBall* newNPC = dynamic_cast<CNPC_AndroidBall*>(newEnt);
					newNPC->SetAbsOrigin(Vector(GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z+10.0f));
					newNPC->SetAbsAngles(GetAbsAngles());
					newNPC->SetSquad(GetSquad());

					DispatchSpawn(newNPC);

					newNPC->SetVars(GetHealth(), forced_left, forced_right);

					newNPC->Activate();
					newNPC->SetEnemy(GetEnemy());
					newNPC->SetState(GetState());
					UTIL_Remove(this);
					TaskComplete();
					return;
				}
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
			break;
		}
		default:
			BaseClass::StartTask(pTask);
			break;
	}
}

void CNPC_Android::RunTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
		case TASK_ANDROID_GUN_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				//Check if both
				if (HasCondition(COND_ANDROID_IS_LEFT) && m_gunShotsL < SHOT_AMOUNT && left_wpn == ANDROID_GUN && left_wpn == right_wpn)
				{
					if (gpGlobals->curtime >= m_attackDurL)
					{
						ShootGun(WEAPON_ATTACHMENT_LEFT);
						ShootGun(WEAPON_ATTACHMENT_RIGHT);
						m_gunShotsL++;
						m_attackDurL = gpGlobals->curtime + SHOT_DELAY;
					}
				}
				else if (HasCondition(COND_ANDROID_IS_LEFT) && m_gunShotsL < SHOT_AMOUNT && left_wpn == ANDROID_GUN)
				{
					if (gpGlobals->curtime >= m_attackDurL)
					{
						ShootGun(WEAPON_ATTACHMENT_LEFT);
						m_gunShotsL++;
						m_attackDurL = gpGlobals->curtime + SHOT_DELAY;
					}
				}
				else if (HasCondition(COND_ANDROID_IS_RIGHT) && m_gunShotsR < SHOT_AMOUNT && right_wpn == ANDROID_GUN)
				{
					if (gpGlobals->curtime >= m_attackDurR)
					{
						ShootGun(WEAPON_ATTACHMENT_RIGHT);
						m_gunShotsR++;
						m_attackDurR = gpGlobals->curtime + SHOT_DELAY;
					}
				}
				else
				{
					TaskComplete();
				}
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
			break;
		}

		case TASK_ANDROID_LASER_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				//Kill laser if way off.
				Vector vEnemyForward, vForward;

				GetEnemy()->GetVectors(&vEnemyForward, NULL, NULL);
				GetVectors(&vForward, NULL, NULL);

				float flDot = DotProduct(vForward, vEnemyForward);

				if (flDot > 0.0)
				{
					KillLaser(m_pBeamL, m_pLightGlowL);
					KillLaser(m_pBeamR, m_pLightGlowR);

					TaskFail("Laser off.");
					return;
				}

				//Check if both
				if (HasCondition(COND_ANDROID_IS_LEFT) && gpGlobals->curtime < m_attackDurL && left_wpn == ANDROID_LASER && left_wpn == right_wpn)
				{
					laserAccuracy += 0.02f;
					LaserEndPoint();
					UpdateLaser(m_pBeamL, WEAPON_ATTACHMENT_LEFT);
					UpdateLaser(m_pBeamR, WEAPON_ATTACHMENT_RIGHT);
				}
				else if (HasCondition(COND_ANDROID_IS_LEFT) && gpGlobals->curtime < m_attackDurL && left_wpn == ANDROID_LASER)
				{
					laserAccuracy += 0.02f;
					LaserEndPoint();
					UpdateLaser(m_pBeamL, WEAPON_ATTACHMENT_LEFT);
				}
				else if (HasCondition(COND_ANDROID_IS_RIGHT) && gpGlobals->curtime < m_attackDurR && right_wpn == ANDROID_LASER)
				{
					laserAccuracy += 0.02f;
					LaserEndPoint();
					UpdateLaser(m_pBeamR, WEAPON_ATTACHMENT_RIGHT);
				}
				else
				{
					if (m_pBeamL)
					{
						KillLaser(m_pBeamL, m_pLightGlowL);
					}
					if (m_pBeamR)
					{
						KillLaser(m_pBeamR, m_pLightGlowR);
					}
					TaskComplete();
				}
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
			break;
		}
		case TASK_ANDROID_BALL_MODE:
		{
			return;
		}
		default:
		{
			BaseClass::RunTask(pTask);
			break;
		}
	}
}

Vector CNPC_Android::CalcEndPoint()
{
	CBaseEntity* pTarget = GetEnemy();
	Vector resultVec = GetEnemyLKP() + pTarget->GetViewOffset();

	//Default Function
	resultVec.x += 60 + 120 * random->RandomInt(-1, 1);
	resultVec.y += 60 + 120 * random->RandomInt(-1, 1);

	//If facing instead shoot ground
	CBaseCombatCharacter* pBCC = ToBaseCombatCharacter(pTarget);
	if (pBCC)
	{
		Vector targetToMe = (pBCC->GetAbsOrigin() - GetAbsOrigin());
		Vector vBCCFacing = pBCC->BodyDirection2D();
		if ((DotProduct(vBCCFacing, targetToMe) < 0.5) &&
			(pBCC->GetSmoothedVelocity().Length() < 50))
		{
			resultVec.z -= 150;
		}
	}

	return resultVec;
}

void CNPC_Android::LaserEndPoint()
{
	Vector vecToEnemy = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	vecToEnemy.NormalizeInPlace();
	m_laserEndpoint = GetAbsOrigin() + (vecToEnemy * FAR_RANGE * 2.0f);

	//Combine the start and end
	m_laserTarget = Lerp(laserAccuracy, m_laserStartpoint, m_laserEndpoint);
}

void CNPC_Android::ShootGun(int attachment)
{
	Vector vecMuzzleOrigin;
	QAngle vecMuzzleAngles;
	GetAttachment(attachment, vecMuzzleOrigin, vecMuzzleAngles);

	Vector vecShootDir = GetShootEnemyDir(vecMuzzleOrigin,true);

	AngleVectors(vecMuzzleAngles, &vecShootDir);

	for (int i = 0; i < 3; i++)
	{
		//Determine position of shot
		switch (i)
		{
			case 1:
				vecMuzzleOrigin.z -= 2.0;
				vecMuzzleOrigin.x -= 2.0;
			case 2:
				vecMuzzleOrigin.x += 4.0;
			default:
				vecMuzzleOrigin.z += 4.0;
		}


		FireBulletsInfo_t info;
		info.m_vecSrc = vecMuzzleOrigin;
		info.m_vecDirShooting = vecShootDir;
		info.m_vecSpread = VECTOR_CONE_20DEGREES;
		info.m_iShots = 1;
		info.m_flDistance = 8192;
		info.m_flDamage = 2.2f;
		info.m_pAttacker = this;
		info.m_iAmmoType = GetAmmoDef()->Index("SMG1");
		info.m_pAdditionalIgnoreEnt = this;

		FireBullets(info);

		UTIL_Tracer(vecMuzzleOrigin, vecMuzzleOrigin + vecShootDir * info.m_flDistance, 0, TRACER_DONT_USE_ATTACHMENT, 5000.0f, false, "AR2Tracer");
		DispatchParticleEffect("hunter_muzzle_flash", PATTACH_POINT_FOLLOW, this, attachment);
		EmitSound("Weapon_SMG1.Single");
	}
}

void CNPC_Android::LaserPosition(CBeam* beam, int attachment)
{
	Vector vAttachPos;
	QAngle vAttachAng;
	GetAttachment(attachment, vAttachPos, vAttachAng);

	Vector vForward;
	AngleVectors(vAttachAng, &vForward);

	Vector vecSrc = vAttachPos;
	trace_t tr;
	AI_TraceLine(vecSrc, m_laserTarget, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	beam->PointEntInit(tr.endpos, this);
	beam->SetEndAttachment(attachment);

	if (tr.DidHit())
	{
		CBaseEntity* pHitEntity = tr.m_pEnt;

		CBaseCombatCharacter* pVictimBCC = pHitEntity->MyCombatCharacterPointer();

		if (pVictimBCC && pVictimBCC != this)
		{
			CTakeDamageInfo damageInfo;
			damageInfo.SetDamage(5.0f);
			damageInfo.SetAttacker(this);
			damageInfo.SetInflictor(this);
			damageInfo.SetDamageType(DMG_DISSOLVE);

			pVictimBCC->TakeDamage(damageInfo);
		}

		if (!m_bPlayingLaserSound)
		{
			CPASAttenuationFilter filter(beam, "NPC_Stalker.BurnWall");
			filter.MakeReliable();

			EmitSound(filter, beam->entindex(), "NPC_Stalker.BurnWall");
			m_bPlayingLaserSound = true;
		}
	}

	UTIL_DecalTrace(&tr, "RedGlowFade");
	UTIL_DecalTrace(&tr, "FadingScorch");
}

void CNPC_Android::CreateLaser(CBeam*& beam, CSprite*& sprite, int attachment)
{
	if (!beam)
	{
		beam = CBeam::BeamCreate("sprites/laser.vmt", 2.0);

		LaserPosition(beam,attachment);

		beam->SetBrightness(255);
		beam->SetNoise(0);


		beam->SetColor(255, 0, 0);
		sprite = CSprite::SpriteCreate("sprites/redglow1.vmt", GetAbsOrigin(), FALSE);

		sprite->SetTransparency(kRenderGlow, 255, 200, 200, 0, kRenderFxNoDissipation);
		sprite->SetAttachment(this, attachment);
		sprite->SetBrightness(255);
		sprite->SetScale(0.65);

		m_pLightGlowEnd = CSprite::SpriteCreate("sprites/redglow1.vmt", GetAbsOrigin(), FALSE);

		m_pLightGlowEnd->SetTransparency(kRenderGlow, 255, 200, 200, 0, kRenderFxNoDissipation);
		m_pLightGlowEnd->SetAttachment(beam, beam->GetEndAttachment());
		m_pLightGlowEnd->SetBrightness(255);
		m_pLightGlowEnd->SetScale(0.65);
	}
}

void CNPC_Android::UpdateLaser(CBeam* beam, int attachment)
{
	if (beam)
	{
		LaserPosition(beam, attachment);
	}
}

void CNPC_Android::KillLaser(CBeam* &beam, CSprite* &sprite)
{

	if (beam)
	{
		StopSound(beam->entindex(), "NPC_Stalker.BurnWall");
		UTIL_Remove(beam);
		beam = NULL;
		UTIL_Remove(m_pLightGlowEnd);
	}

	if (sprite)
	{
		UTIL_Remove(sprite);
		sprite = NULL;
	}
}

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

void CNPC_Android::Think(void)
{
	if (!GetEnemy())
	{
		KillLaser(m_pBeamR, m_pLightGlowR);
		KillLaser(m_pBeamL, m_pLightGlowL);
	}

	if (m_zaptime < gpGlobals->curtime)
	{
		m_zapped = false;
	}

	BaseClass::Think();
}

void CNPC_Android::GatherConditions(void)
{
	BaseClass::GatherConditions();

	if (HasCondition(COND_CAN_RANGE_ATTACK1))
	{
		if (m_nextAttackL < gpGlobals->curtime)
		{
			SetCondition(COND_ANDROID_IS_LEFT);
		}
		else if (m_nextAttackR < gpGlobals->curtime)
		{
			SetCondition(COND_ANDROID_IS_RIGHT);
		}
	}
}

void CNPC_Android::PrescheduleThink(void)
{
	UpdateHead();
	BaseClass::PrescheduleThink();
}

void CNPC_Android::PopulatePoseParameters(void)
{
	m_poseHead_Pitch = LookupPoseParameter("head_pitch");
	m_poseHead_Yaw = LookupPoseParameter("head_yaw");

	BaseClass::PopulatePoseParameters();
}

void CNPC_Android::UpdateHead(void)
{

	float yaw = GetPoseParameter(m_poseHead_Yaw);
	float pitch = GetPoseParameter(m_poseHead_Pitch);

	CBaseEntity* pTarget = (GetTarget() != NULL) ? GetTarget() : GetEnemy();

	if (pTarget != NULL)
	{
		Vector	enemyDir = pTarget->WorldSpaceCenter() - WorldSpaceCenter();
		VectorNormalize(enemyDir);

		if (DotProduct(enemyDir, BodyDirection3D()) < 0.0f)
		{
			SetPoseParameter(m_poseHead_Yaw, UTIL_Approach(0, yaw, 10));
			SetPoseParameter(m_poseHead_Pitch, UTIL_Approach(0, pitch, 10));

			return;
		}

		float facingYaw = VecToYaw(BodyDirection3D());
		float yawDiff = VecToYaw(enemyDir);
		yawDiff = UTIL_AngleDiff(yawDiff, facingYaw + yaw);

		float facingPitch = UTIL_VecToPitch(BodyDirection3D());
		float pitchDiff = UTIL_VecToPitch(enemyDir);
		pitchDiff = UTIL_AngleDiff(pitchDiff, facingPitch + pitch);

		SetPoseParameter(m_poseHead_Yaw, UTIL_Approach(yaw + yawDiff, yaw, 50));
		SetPoseParameter(m_poseHead_Pitch, UTIL_Approach(pitch + pitchDiff, pitch, 50));
	}
	else
	{
		SetPoseParameter(m_poseHead_Yaw, UTIL_Approach(0, yaw, 10));
		SetPoseParameter(m_poseHead_Pitch, UTIL_Approach(0, pitch, 10));
	}
}


AI_BEGIN_CUSTOM_NPC(npc_android, CNPC_Android)

DECLARE_TASK(TASK_ANDROID_LASER_ATTACK);
DECLARE_TASK(TASK_ANDROID_GUN_ATTACK);
DECLARE_TASK(TASK_ANDROID_BALL_MODE);

DECLARE_CONDITION(COND_ANDROID_IS_LEFT);
DECLARE_CONDITION(COND_ANDROID_IS_RIGHT);
DECLARE_CONDITION(COND_ANDROID_ZAPPED);

DECLARE_ACTIVITY(ACT_SWAP_LEFT_WPN);
DECLARE_ACTIVITY(ACT_SWAP_RIGHT_WPN);
DECLARE_ACTIVITY(ACT_ZAPPED);
DECLARE_ACTIVITY(ACT_BALL);
DECLARE_ACTIVITY(ACT_UNBALL);

DECLARE_ANIMEVENT(AE_ANDROID_SWAP_LEFT);
DECLARE_ANIMEVENT(AE_ANDROID_SWAP_RIGHT);

//-----------------------------------------------------------------------------
// AI Schedules Specific to this NPC
//-----------------------------------------------------------------------------

DEFINE_SCHEDULE
(
	SCHED_ANDROID_CHASE_ENEMY,

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
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_TASK_FAILED"
	"		COND_ANDROID_ZAPPED"
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_LASER_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANDROID_LASER_ATTACK		0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"       COND_LOST_ENEMY"
	"		COND_ANDROID_ZAPPED"
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_GUN_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANDROID_GUN_ATTACK		0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"       COND_LOST_ENEMY"
	"		COND_ANDROID_ZAPPED"
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_FLANK_RANDOM,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_ANDROID_RUN_RANDOM"
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
	"		COND_ANDROID_ZAPPED"
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_RUN_RANDOM,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ANDROID_COVER_FROM_ENEMY"
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
	"		COND_ANDROID_ZAPPED"
)
DEFINE_SCHEDULE
(
	SCHED_ANDROID_COVER_FROM_ENEMY,

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
	"		COND_ANDROID_ZAPPED"
)
DEFINE_SCHEDULE
(
	SCHED_ANDROID_ZAP,

	"	Tasks"
	"		TASK_STOP_MOVING	0"
	"		TASK_RESET_ACTIVITY		0"
	"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_ZAPPED"

	"	Interrupts"
	"		COND_TASK_FAILED"
)
DEFINE_SCHEDULE
(
	SCHED_ANDROID_BALL_MODE,
	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_BALL"
	"		TASK_ANDROID_BALL_MODE	0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"       COND_LOST_ENEMY"
	"		COND_ANDROID_ZAPPED"
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_UNBALL,
	"	Tasks"
	"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_UNBALL"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_ANDROID_ZAPPED"
)

AI_END_CUSTOM_NPC()