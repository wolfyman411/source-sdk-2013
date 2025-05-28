
#include "cbase.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_behavior_follow.h"
#include "npc_android.h"
#include "ai_schedule.h"
#include "util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ANDROID_MODEL "models/aperture/android.mdl"
#define CLOSE_RANGE 300.0f
#define FAR_RANGE 1200.0f

#define WEAPON_ATTACHMENT_LEFT	1
#define WEAPON_ATTACHMENT_RIGHT	2
#define PARTICLE_ATTACHMENT		"particles"

ConVar sk_android_health("sk_android_health", "125");

LINK_ENTITY_TO_CLASS(npc_android, CNPC_Android);

BEGIN_DATADESC(CNPC_Android)

END_DATADESC()

//-----------------------------------------------------------------------------
// Classify - indicates this NPC's place in the 
// relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Android::Classify(void)
{
	return CLASS_MILITARY;
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
	PrecacheModel(ANDROID_MODEL);

	BaseClass::Precache();
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
	DevMsg("OW! HP:%d/%d\n",GetHealth(),GetMaxHealth());
	// Start smoking when we're nearly dead
	if (m_iHealth < m_iMaxHealth/2)
	{
		StartSmokeTrail();
	}
	if (m_iHealth < m_iMaxHealth / 4)
	{
		DevMsg("Fire\n");
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
		return SCHED_COMBAT_WALK;
	}

	switch (m_NPCState)
	{
		case NPC_STATE_COMBAT:
		{
			if (HasCondition(COND_CAN_RANGE_ATTACK1))
			{
				return SCHED_ANDROID_LASER_ATTACK;
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
		}

		case TASK_ANDROID_LASER_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				if (!m_pBeamL)
				{
					CreateLaser(m_pBeamL, m_pLightGlowL, WEAPON_ATTACHMENT_LEFT);
				}
				if (!m_pBeamR)
				{
					CreateLaser(m_pBeamR, m_pLightGlowR, WEAPON_ATTACHMENT_RIGHT);
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
				DevMsg("Lost enemy\n");
				TaskFail("Lost enemy");
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

			if (GetNavigator()->CurWaypointIsGoal())
			{
				DevMsg("Goal Reached!");
			}

			break;
		}

		case TASK_ANDROID_LASER_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				UpdateLaser(m_pBeamL, WEAPON_ATTACHMENT_LEFT);
				UpdateLaser(m_pBeamR, WEAPON_ATTACHMENT_RIGHT);
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

void CNPC_Android::LaserPosition(CBeam* beam, int attachment)
{
	Vector vAttachPos;
	QAngle vAttachAng;
	GetAttachment(attachment, vAttachPos, vAttachAng);

	Vector vForward;
	AngleVectors(vAttachAng, &vForward);

	Vector vecSrc = vAttachPos;
	trace_t tr;
	AI_TraceLine(vecSrc, vecSrc + vForward * 1000.0f, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	beam->PointEntInit(tr.endpos, this);
	beam->SetEndAttachment(attachment);
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
	}
}

void CNPC_Android::UpdateLaser(CBeam* beam, int attachment)
{
	if (beam)
	{
		LaserPosition(beam, attachment);
	}
}

void CNPC_Android::GatherConditions(void)
{
	BaseClass::GatherConditions();
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
DECLARE_TASK(TASK_ANDROID_CIRCLE_ENEMY);

//-----------------------------------------------------------------------------
// AI Schedules Specific to this NPC
//-----------------------------------------------------------------------------

DEFINE_SCHEDULE
(
	SCHED_ANDROID_LASER_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"       TASK_ANDROID_CIRCLE_ENEMY       0"
	"		TASK_ANDROID_LASER_ATTACK		0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"       COND_LOST_ENEMY"
)

AI_END_CUSTOM_NPC()