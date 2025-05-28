
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

	m_iHealth = 20;

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
				DevMsg("Laser Attack Sched\n");
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
	DevMsg("Start: %s\n", TaskName(pTask->iTask));
	switch (pTask->iTask)
	{
		case TASK_ANDROID_LASER_ATTACK:
		{
			CBaseEntity* pTarget = GetEnemy();
			if (pTarget)
			{
				if (!m_pBeamL)
				{
					Vector vAttachPos;
					QAngle vAttachAng;
					GetAttachment(WEAPON_ATTACHMENT_LEFT, vAttachPos, vAttachAng);

					Vector vForward;
					AngleVectors(vAttachAng, &vForward);

					Vector vecSrc = vAttachPos;
					trace_t tr;
					AI_TraceLine(vecSrc, vecSrc + vForward * 1000.0f, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

					m_pBeamL = CBeam::BeamCreate("sprites/laser.vmt", 2.0);
					m_pBeamL->PointEntInit(tr.endpos, this);
					m_pBeamL->SetEndAttachment(WEAPON_ATTACHMENT_LEFT);
					m_pBeamL->SetBrightness(255);
					m_pBeamL->SetNoise(0);


					m_pBeamL->SetColor(255, 0, 0);
					m_pLightGlowL = CSprite::SpriteCreate("sprites/redglow1.vmt", GetAbsOrigin(), FALSE);

					m_pLightGlowL->SetTransparency(kRenderGlow, 255, 200, 200, 0, kRenderFxNoDissipation);
					m_pLightGlowL->SetAttachment(this, 1);
					m_pLightGlowL->SetBrightness(255);
					m_pLightGlowL->SetScale(0.65);
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
	DevMsg("Run: %s\n",TaskName(pTask->iTask));
	switch (pTask->iTask)
	{
		case TASK_ANDROID_LASER_ATTACK:
		{
			DevMsg("Run Task\n");
			break;
		}
		default:
		{
			BaseClass::RunTask(pTask);
			break;
		}
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
)

AI_END_CUSTOM_NPC()