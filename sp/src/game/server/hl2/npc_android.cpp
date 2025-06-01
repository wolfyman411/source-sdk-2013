
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

//Activities
Activity ACT_SWAP_LEFT_WPN;
Activity ACT_SWAP_RIGHT_WPN;

#define ANDROID_MODEL "models/aperture/android.mdl"
#define CLOSE_RANGE 200.0f
#define FAR_RANGE 2000.0f
#define LASER_DURATION 3.0f
#define SHOT_DELAY 0.1f
#define SHOT_AMOUNT 3
#define ATTACK_DELAY 0.3f

#define WEAPON_ATTACHMENT_LEFT	1
#define WEAPON_ATTACHMENT_RIGHT	2
#define PARTICLE_ATTACHMENT		"particles"

ConVar sk_android_health("sk_android_health", "200");

LINK_ENTITY_TO_CLASS(npc_android, CNPC_Android);

BEGIN_DATADESC(CNPC_Android)

END_DATADESC()

//-----------------------------------------------------------------------------
// Classify - indicates this NPC's place in the 
// relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Android::Classify(void)
{
	return CLASS_APERTURE;
}



//-----------------------------------------------------------------------------
// HandleAnimEvent - catches the NPC-specific messages
// that occur when tagged animation frames are played.
//-----------------------------------------------------------------------------
void CNPC_Android::HandleAnimEvent(animevent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	default:
		BaseClass::HandleAnimEvent(pEvent);
		break;
	}
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

	BaseClass::Spawn();
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
	// Start smoking when we're nearly dead
	if (m_iHealth < m_iMaxHealth/2)
	{
		StartSmokeTrail();
	}
	if (m_iHealth < m_iMaxHealth / 4)
	{
		StartFire();
	}

	return (BaseClass::OnTakeDamage_Alive(info));
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

	if (m_pBeamL)
	{
		UTIL_Remove(m_pBeamL);
		UTIL_Remove(m_pLightGlowL);
	}

	if (m_pBeamR)
	{
		UTIL_Remove(m_pBeamR);
		UTIL_Remove(m_pLightGlowR);
	}

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
				// Do we want to swap weapons?
				CBaseEntity* pTarget = GetEnemy();
				Android_Weapons_e idealWeapon_left = ANDROID_GUN;
				Android_Weapons_e idealWeapon_right = ANDROID_GUN;
				if (pTarget)
				{
					float dis = (pTarget->GetAbsOrigin() - GetAbsOrigin()).Length();
					
					// Go with gun
					if (dis > CLOSE_RANGE && dis < FAR_RANGE / 3)
					{
						idealWeapon_left = ANDROID_GUN;
						idealWeapon_right = ANDROID_GUN;
					}
					// Middle Ground 
					else if (dis >= FAR_RANGE / 3 && dis < FAR_RANGE/2)
					{
						idealWeapon_left = ANDROID_GUN;
						idealWeapon_right = ANDROID_LASER;
					}
					// Otherwise Laser
					else
					{
						idealWeapon_left = ANDROID_LASER;
						idealWeapon_right = ANDROID_LASER;
					}
				}


				if (m_nextAttackL < gpGlobals->curtime)
				{
					if (forced_left != ANDROID_NONE)
					{
						idealWeapon_left = forced_left;
					}

					if (left_wpn != idealWeapon_left)
					{
						left_wpn = idealWeapon_left;
						AddGesture(ACT_SWAP_LEFT_WPN);
						m_nextAttackL = gpGlobals->curtime + 1.0f;
					}
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
					if (forced_right != ANDROID_NONE)
					{
						idealWeapon_right = forced_right;
					}

					if (right_wpn != idealWeapon_right)
					{
						right_wpn = idealWeapon_right;
						AddGesture(ACT_SWAP_RIGHT_WPN);
						m_nextAttackR = gpGlobals->curtime + 1.0f;
					}
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
		case TASK_ANDROID_CIRCLE_ENEMY:
		{
			if (!GetEnemy())
			{
				TaskFail("No enemy");
				return;
			}
			break;

			if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
			{
				TaskFail("Too Close");
				return;
			}
			break;
		}

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
				}
				else if (HasCondition(COND_ANDROID_IS_LEFT) && left_wpn == ANDROID_GUN)
				{
					m_gunShotsL = 0;
					m_nextAttackL = gpGlobals->curtime + ATTACK_DELAY + RandomFloat(0.1f, 0.5f);
					m_attackDurL = gpGlobals->curtime;
				}
				else if (HasCondition(COND_ANDROID_IS_RIGHT) && right_wpn == ANDROID_GUN)
				{
					m_gunShotsR = 0;
					m_nextAttackR = gpGlobals->curtime + ATTACK_DELAY + RandomFloat(0.1f, 0.5f);
					m_attackDurR = gpGlobals->curtime;
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
				}
				else if (!m_pBeamL && HasCondition(COND_ANDROID_IS_LEFT) && left_wpn == ANDROID_LASER)
				{
					laserAccuracy = 0.0f;
					m_laserStartpoint = CalcEndPoint();

					CreateLaser(m_pBeamL, m_pLightGlowL, WEAPON_ATTACHMENT_LEFT);
					m_attackDurL = gpGlobals->curtime + LASER_DURATION;
					m_nextAttackL = gpGlobals->curtime + LASER_DURATION + ATTACK_DELAY + RandomFloat(0.5f,2.0f);
				}
				else if (!m_pBeamR && HasCondition(COND_ANDROID_IS_RIGHT) && right_wpn == ANDROID_LASER)
				{
					laserAccuracy = 0.0f;
					m_laserStartpoint = CalcEndPoint();

					CreateLaser(m_pBeamR, m_pLightGlowR, WEAPON_ATTACHMENT_RIGHT);
					m_attackDurR = gpGlobals->curtime + LASER_DURATION;
					m_nextAttackR = gpGlobals->curtime + LASER_DURATION + ATTACK_DELAY + RandomFloat(0.5f, 2.0f);
				}
				break;
			}
			else
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
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
		case TASK_ANDROID_CIRCLE_ENEMY:
		{
			if (!GetEnemy())
			{
				TaskFail("Lost enemy");
				return;
			}

			if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
			{
				TaskFail("Too Close");
				return;
			}

			Vector vecEnemyPos = GetEnemy()->GetAbsOrigin();
			Vector vecDir = GetAbsOrigin() - vecEnemyPos;
			vecDir.z = 0;
			float flDist = vecDir.NormalizeInPlace();

			QAngle angMove;
			VectorAngles(vecDir, angMove);
			angMove.y += RandomFloat(-100.0f, 100.0f);
			AngleVectors(angMove, &vecDir);

			// Set new position (maintain distance)
			Vector vecTargetPos = vecEnemyPos + vecDir * flDist;

			AI_NavGoal_t goal(vecTargetPos);
			GetNavigator()->SetGoal(goal, AIN_CLEAR_TARGET);

			break;
		}

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
		info.m_flDamage = 3.0f;
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

		if (pHitEntity && pHitEntity != this)
		{
			CTakeDamageInfo damageInfo;
			damageInfo.SetDamage(5.0f);
			damageInfo.SetAttacker(this);
			damageInfo.SetInflictor(this);
			damageInfo.SetDamageType(DMG_DISSOLVE);

			pHitEntity->TakeDamage(damageInfo);
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
	m_poseWeaponL = LookupPoseParameter("weapon_left_yaw");
	m_poseWeaponR = LookupPoseParameter("weapon_right_yaw");

	BaseClass::PopulatePoseParameters();
}

void CNPC_Android::UpdateHead(void)
{

	float yaw = GetPoseParameter(m_poseHead_Yaw);
	float pitch = GetPoseParameter(m_poseHead_Pitch);
	float weaponL = GetPoseParameter(m_poseWeaponL);
	float weaponR = GetPoseParameter(m_poseWeaponR);

	CBaseEntity* pTarget = (GetTarget() != NULL) ? GetTarget() : GetEnemy();

	if (pTarget != NULL)
	{
		Vector	enemyDir = pTarget->WorldSpaceCenter() - WorldSpaceCenter();
		VectorNormalize(enemyDir);

		if (DotProduct(enemyDir, BodyDirection3D()) < 0.0f)
		{
			SetPoseParameter(m_poseHead_Yaw, UTIL_Approach(0, yaw, 10));
			SetPoseParameter(m_poseHead_Pitch, UTIL_Approach(0, pitch, 10));
			SetPoseParameter(m_poseWeaponL, UTIL_Approach(0, weaponL, 10));
			SetPoseParameter(m_poseWeaponR, UTIL_Approach(0, weaponR, 10));

			return;
		}

		float facingYaw = VecToYaw(BodyDirection3D());
		float yawDiff = VecToYaw(enemyDir);
		yawDiff = UTIL_AngleDiff(yawDiff, facingYaw + yaw);

		float yawLeftDiff = VecToYaw(enemyDir);
		yawLeftDiff = UTIL_AngleDiff(yawLeftDiff, facingYaw + weaponL);

		float yawRightDiff = VecToYaw(enemyDir);
		yawRightDiff = UTIL_AngleDiff(yawRightDiff, facingYaw + weaponR);

		float facingPitch = UTIL_VecToPitch(BodyDirection3D());
		float pitchDiff = UTIL_VecToPitch(enemyDir);
		pitchDiff = UTIL_AngleDiff(pitchDiff, facingPitch + pitch);

		SetPoseParameter(m_poseHead_Yaw, UTIL_Approach(yaw + yawDiff, yaw, 10));
		SetPoseParameter(m_poseHead_Pitch, UTIL_Approach(pitch + pitchDiff, pitch, 50));
		SetPoseParameter(m_poseWeaponL, UTIL_Approach(weaponL + yawLeftDiff, weaponL, 10));
		SetPoseParameter(m_poseWeaponR, UTIL_Approach(weaponR + yawRightDiff, weaponR, 10));
	}
	else
	{
		SetPoseParameter(m_poseHead_Yaw, UTIL_Approach(0, yaw, 10));
		SetPoseParameter(m_poseHead_Pitch, UTIL_Approach(0, pitch, 10));
		SetPoseParameter(m_poseWeaponL, UTIL_Approach(0, weaponL, 10));
		SetPoseParameter(m_poseWeaponR, UTIL_Approach(0, weaponR, 10));
	}
}

AI_BEGIN_CUSTOM_NPC(npc_android, CNPC_Android)

DECLARE_TASK(TASK_ANDROID_LASER_ATTACK);
DECLARE_TASK(TASK_ANDROID_GUN_ATTACK);
DECLARE_TASK(TASK_ANDROID_CIRCLE_ENEMY);

DECLARE_CONDITION(COND_ANDROID_IS_LEFT);
DECLARE_CONDITION(COND_ANDROID_IS_RIGHT);

DECLARE_ACTIVITY(ACT_SWAP_LEFT_WPN);
DECLARE_ACTIVITY(ACT_SWAP_RIGHT_WPN);

//-----------------------------------------------------------------------------
// AI Schedules Specific to this NPC
//-----------------------------------------------------------------------------

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
)

DEFINE_SCHEDULE
(
	SCHED_ANDROID_GUN_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANDROID_GUN_ATTACK		0"
	"       TASK_ANDROID_CIRCLE_ENEMY       0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"       COND_LOST_ENEMY"
)

AI_END_CUSTOM_NPC()