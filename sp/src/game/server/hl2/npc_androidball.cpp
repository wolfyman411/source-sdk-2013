//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_squadslot.h"
#include "ai_basenpc.h"
#include "ai_navigator.h"
#include "ai_interactions.h"
#include "ndebugoverlay.h"
#include "explode.h"
#include "bitstring.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "decals.h"
#include "antlion_dust.h"
#include "ai_memory.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "beam_shared.h"
#include "iservervehicle.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "physics_saverestore.h"
#include "vphysics/constraints.h"
#include "vehicle_base.h"
#include "eventqueue.h"
#include "te_effect_dispatch.h"
#include "func_break.h"
#include "soundenvelope.h"
#include "mapentities.h"
#include "RagdollBoogie.h"
#include "physics_collisionevent.h"
#include "npc_androidball.h"
#include "npc_android.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BALL_MAX_TORQUE_FACTOR	5
#define BALL_MODEL "models/aperture/android_ball.mdl"
extern short g_sModelIndexWExplosion;

ConVar	sk_ball_stun_delay("sk_ball_stun_delay", "1");

//-----------------------------------------------------------------------------
// CRollerController implementation
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC(CBallController)

DEFINE_FIELD(m_vecAngular, FIELD_VECTOR),
DEFINE_FIELD(m_vecLinear, FIELD_VECTOR),
DEFINE_FIELD(m_fIsStopped, FIELD_BOOLEAN),

END_DATADESC()


//-----------------------------------------------------------------------------
IMotionEvent::simresult_e CBallController::Simulate(IPhysicsMotionController* pController, IPhysicsObject* pObject, float deltaTime, Vector& linear, AngularImpulse& angular)
{
	if (m_fIsStopped)
	{
		return SIM_NOTHING;
	}

	linear = m_vecLinear;
	angular = m_vecAngular;

	return IMotionEvent::SIM_LOCAL_ACCELERATION;
}
//-----------------------------------------------------------------------------


#define BALL_IDLE_SEE_DIST					2048
#define BALL_NORMAL_SEE_DIST					2048

#define BALL_RETURN_TO_PLAYER_DIST			(200*200)

#define BALL_MIN_ATTACK_DIST	200
#define BALL_MAX_ATTACK_DIST	4096

#define	BALL_FEAR_DISTANCE			(300*300)

//=========================================================
//=========================================================

string_t CNPC_AndroidBall::gm_iszDropshipClassname;

LINK_ENTITY_TO_CLASS(npc_androidball, CNPC_AndroidBall);

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CNPC_AndroidBall)

DEFINE_EMBEDDED(m_RollerController),
DEFINE_PHYSPTR(m_pMotionController),

DEFINE_FIELD(m_flActiveTime, FIELD_TIME),
DEFINE_FIELD(m_flChargeTime, FIELD_TIME),
DEFINE_FIELD(m_flGoIdleTime, FIELD_TIME),
DEFINE_FIELD(m_flForwardSpeed, FIELD_FLOAT),
DEFINE_FIELD(m_bHeld, FIELD_BOOLEAN),
DEFINE_FIELD(m_iSoundEventFlags, FIELD_INTEGER),

DEFINE_FIELD(m_bEmbedOnGroundImpact, FIELD_BOOLEAN),

DEFINE_PHYSPTR(m_pConstraint),

DEFINE_KEYFIELD(m_bUniformSight, FIELD_BOOLEAN, "uniformsightdist"),

DEFINE_INPUTFUNC(FIELD_VOID, "PowerDown", InputPowerdown),

// Function Pointers
DEFINE_THINKFUNC(Explode),
DEFINE_THINKFUNC(PreDetonate),

DEFINE_OUTPUT(m_OnPhysGunDrop, "OnPhysGunDrop"),
DEFINE_OUTPUT(m_OnPhysGunPickup, "OnPhysGunPickup"),

DEFINE_BASENPCINTERACTABLE_DATADESC(),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_AndroidBall::~CNPC_AndroidBall(void)
{
	if (m_pMotionController != NULL)
	{
		physenv->DestroyMotionController(m_pMotionController);
		m_pMotionController = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::Precache(void)
{
	PrecacheModel(BALL_MODEL);
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::Spawn(void)
{
	Precache();
	SetModel(BALL_MODEL);

	SetSolid(SOLID_VPHYSICS);
	AddSolidFlags(FSOLID_FORCE_WORLD_ALIGNED | FSOLID_NOT_STANDABLE);

	BaseClass::Spawn();

	AddEFlags(EFL_NO_DISSOLVE);
#ifdef MAPBASE
	AddEFlags(EFL_NO_MEGAPHYSCANNON_RAGDOLL);
#endif

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_SQUAD);

	m_flFieldOfView = -1.0f;
	m_flForwardSpeed = -1200;
	m_bloodColor = DONT_BLEED;

	SetHullType(HULL_SMALL_CENTERED);

	SetHullSizeNormal();

	m_flActiveTime = 0;

	NPCInit();

	m_takedamage = DAMAGE_EVENTS_ONLY;
	SetDistLook(BALL_IDLE_SEE_DIST);

	//Suppress superfluous warnings from animation system
	m_flGroundSpeed = 20;
	m_NPCState = NPC_STATE_NONE;

	m_pConstraint = NULL;

	//Set their yaw speed to 0 so the motor doesn't rotate them.
	GetMotor()->SetYawSpeed(0.0f);
}

//-----------------------------------------------------------------------------
// Set the contents types that are solid by default to this NPC
//-----------------------------------------------------------------------------
unsigned int CNPC_AndroidBall::PhysicsSolidMaskForEntity(void) const
{
	return MASK_NPCSOLID;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::OnStateChange(NPC_STATE OldState, NPC_STATE NewState)
{
	if (NewState == NPC_STATE_IDLE)
	{
		SetDistLook(BALL_IDLE_SEE_DIST);
		m_flDistTooFar = BALL_IDLE_SEE_DIST;


		m_RollerController.m_vecAngular = vec3_origin;
	}
	else
	{
		SetDistLook(BALL_NORMAL_SEE_DIST);

		m_flDistTooFar = BALL_NORMAL_SEE_DIST;
	}
	BaseClass::OnStateChange(OldState, NewState);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
NPC_STATE CNPC_AndroidBall::SelectIdealState(void)
{
	switch (m_NPCState)
	{
	case NPC_STATE_COMBAT:
	{
		if (HasCondition(COND_ENEMY_TOO_FAR))
		{
			ClearEnemyMemory();
			SetEnemy(NULL);
			m_flGoIdleTime = gpGlobals->curtime + 10;
			return NPC_STATE_ALERT;
		}
	}
	}

	return BaseClass::SelectIdealState();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_AndroidBall::BecomePhysical(void)
{
	VPhysicsDestroyObject();

	RemoveSolidFlags(FSOLID_NOT_SOLID);

	//Setup the physics controller on the roller
	IPhysicsObject* pPhysicsObject = VPhysicsInitNormal(SOLID_VPHYSICS, GetSolidFlags(), false);

	if (pPhysicsObject == NULL)
		return false;

	m_pMotionController = physenv->CreateMotionController(&m_RollerController);
	m_pMotionController->AttachObject(pPhysicsObject, true);

	SetMoveType(MOVETYPE_VPHYSICS);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::OnRestore()
{
	BaseClass::OnRestore();
	if (m_pMotionController)
	{
		m_pMotionController->SetEventHandler(&m_RollerController);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_AndroidBall::CreateVPhysics()
{
	return BecomePhysical();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::RunAI()
{
	// Scare combine if hacked by Alyx.
	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();

	Vector vecVelocity;

	if (pPhysicsObject != NULL)
	{
		pPhysicsObject->GetVelocity(&vecVelocity, NULL);
	}

	if (!m_bHeld && vecVelocity.Length() > 64.0)
	{
		// Scare player allies
		CSoundEnt::InsertSound((SOUND_DANGER | SOUND_CONTEXT_EXCLUDE_COMBINE | SOUND_CONTEXT_REACT_TO_SOURCE | SOUND_CONTEXT_DANGER_APPROACH), WorldSpaceCenter() + Vector(0, 0, 32) + vecVelocity * 0.5f, 120.0f, 0.2f, this, SOUNDENT_CHANNEL_REPEATED_DANGER);
	}

	BaseClass::RunAI();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_AndroidBall::RangeAttack1Conditions(float flDot, float flDist)
{
	if (HasCondition(COND_SEE_ENEMY) == false)
		return COND_NONE;

	if (flDist > BALL_MAX_ATTACK_DIST)
		return COND_TOO_FAR_TO_ATTACK;

	if (flDist < BALL_MIN_ATTACK_DIST)
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_AndroidBall::SelectSchedule(void)
{
	//If we're held, don't try and do anything
	if ((m_bHeld) || !IsActive())
		return SCHED_ALERT_STAND;

	if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
		return SCHED_BALL_UNBALL;

	// If we can see something we're afraid of, run from it
	if (HasCondition(COND_SEE_FEAR))
		return SCHED_BALL_FLEE;

	switch (m_NPCState)
	{
	case NPC_STATE_COMBAT:

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
			return SCHED_BALL_RANGE_ATTACK1;

		return SCHED_BALL_CHASE_ENEMY;
		break;

	default:
		break;
	}

	// Rollermines never wait to fall to the ground
	ClearCondition(COND_FLOATING_OFF_GROUND);

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_AndroidBall::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	break;

	case SCHED_ALERT_STAND:
	{
		return SCHED_BALL_ALERT_STAND;
	}
	break;

	case SCHED_BALL_RANGE_ATTACK1:
		if (HasCondition(COND_ENEMY_OCCLUDED))
		{
			// Because of an unfortunate arrangement of cascading failing schedules, the rollermine
			// could end up here with instructions to drive towards the target, although the target is
			// not in sight. Nudge around randomly until we're back on the nodegraph.
			return SCHED_BALL_NUDGE_TOWARDS_NODES;
		}
		break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSightEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_AndroidBall::QuerySeeEntity(CBaseEntity* pSightEnt, bool bOnlyHateOrFearIfNPC)
{
	if (IRelationType(pSightEnt) == D_FR)
	{
		// Only see feared objects up close
		float flDist = (WorldSpaceCenter() - pSightEnt->WorldSpaceCenter()).LengthSqr();
		if (flDist > BALL_FEAR_DISTANCE)
			return false;
	}

	return BaseClass::QuerySeeEntity(pSightEnt, bOnlyHateOrFearIfNPC);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::StartTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_FACE_REASONABLE:
	case TASK_FACE_SAVEPOSITION:
	case TASK_FACE_LASTPOSITION:
	case TASK_FACE_TARGET:
	case TASK_FACE_AWAY_FROM_SAVEPOSITION:
	case TASK_FACE_HINTNODE:
	case TASK_FACE_ENEMY:
	case TASK_FACE_PLAYER:
	case TASK_FACE_PATH:
	case TASK_FACE_IDEAL:
		// This only applies to NPCs that aren't spheres with omnidirectional eyesight.
		TaskComplete();
		break;

	case TASK_STOP_MOVING:

		//Stop turning
		m_RollerController.m_vecAngular = vec3_origin;

		TaskComplete();
		return;
		break;

	case TASK_WAIT_FOR_MOVEMENT:
	{
		// TASK_RUN_PATH and TASK_WALK_PATH work different on the rollermine and run until movement is done,
		// so movement is already complete when entering this task.
		TaskComplete();
	}
	break;

	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
	{
		IPhysicsObject* pPhysicsObject = VPhysicsGetObject();

		if (pPhysicsObject == NULL)
		{
			assert(0);
			TaskFail("Roller lost internal physics object?");
			return;
		}

		pPhysicsObject->Wake();
	}
	break;

	case TASK_BALL_CHARGE_ENEMY:
	case TASK_BALL_RETURN_TO_PLAYER:
	{
		IPhysicsObject* pPhysicsObject = VPhysicsGetObject();

		if (pPhysicsObject == NULL)
		{
			assert(0);
			TaskFail("Roller lost internal physics object?");
			return;
		}

		pPhysicsObject->Wake();

		m_flChargeTime = gpGlobals->curtime;
	}

	break;

	case TASK_BALL_GET_PATH_TO_FLEE:
	{
		// Find the nearest thing we're afraid of, and move away from it.
		float flNearest = BALL_FEAR_DISTANCE;
		EHANDLE hNearestEnemy = NULL;
		AIEnemiesIter_t iter;
		for (AI_EnemyInfo_t* pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter))
		{
			CBaseEntity* pEnemy = pEMemory->hEnemy;
			if (!pEnemy || !pEnemy->IsAlive())
				continue;
			if (IRelationType(pEnemy) != D_FR)
				continue;

			float flDist = (WorldSpaceCenter() - pEnemy->WorldSpaceCenter()).LengthSqr();
			if (flDist < flNearest)
			{
				flNearest = flDist;
				hNearestEnemy = pEnemy;
			}
		}

		if (!hNearestEnemy)
		{
			TaskFail("Couldn't find nearest feared object.");
			break;
		}

		GetMotor()->SetIdealYawToTarget(hNearestEnemy->WorldSpaceCenter());
		ChainStartTask(TASK_MOVE_AWAY_PATH, pTask->flTaskData);
	}
	break;

	case TASK_BALL_NUDGE_TOWARDS_NODES:
	{
		IPhysicsObject* pPhysicsObject = VPhysicsGetObject();

		if (pPhysicsObject)
		{
			// Try a few times to find a direction to shove ourself
			for (int i = 0; i < 4; i++)
			{
				int x, y;

				x = random->RandomInt(-1, 1);
				y = random->RandomInt(-1, 1);

				Vector vecNudge(x, y, 0.0f);

				trace_t tr;

				// Try to move in a direction with a couple of feet of clearance.
				UTIL_TraceLine(WorldSpaceCenter(), WorldSpaceCenter() + vecNudge * 24.0f, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

				if (tr.fraction == 1.0)
				{
					vecNudge *= (pPhysicsObject->GetMass() * 75.0f);
					vecNudge += Vector(0, 0, pPhysicsObject->GetMass() * 75.0f);
					pPhysicsObject->ApplyForceCenter(vecNudge);
					break;
				}
			}
		}

		TaskComplete();
	}
	break;
	case TASK_BALL_UNBALL:
	{
		CBaseEntity* pTarget = GetEnemy();
		if (pTarget && !unball)
		{
			//create a new android where the ball is
			unball = true;

			CBaseEntity* newEnt = CreateEntityByName("npc_android");
			if (newEnt)
			{
				CAI_BaseNPC* newNPC = dynamic_cast<CAI_BaseNPC*>(newEnt);
				newNPC->SetAbsOrigin(GetAbsOrigin());

				QAngle angLookAtEnemy;
				VectorAngles(pTarget->GetAbsOrigin(), angLookAtEnemy);

				newNPC->SetAbsAngles(QAngle(0, angLookAtEnemy.y,0));
				DispatchSpawn(newNPC);
				newNPC->Activate();
				newNPC->SetEnemy(GetEnemy());
				newNPC->SetState(GetState());
				UTIL_Remove(this);
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
		BaseClass::StartTask(pTask);
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::RunTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{

	case TASK_BALL_GET_PATH_TO_FLEE:
	{
		ChainRunTask(TASK_MOVE_AWAY_PATH, pTask->flTaskData);
	}
	break;

	case TASK_WAIT_FOR_MOVEMENT:
	{
		// TASK_RUN_PATH and TASK_WALK_PATH work different on the rollermine and run until movement is done,
		// so movement is already complete when entering this task.
		TaskComplete();
	}
	break;

	case TASK_RUN_PATH:
	case TASK_WALK_PATH:

		if (m_bHeld)
		{
			TaskFail("Player interrupted by grabbing");
			break;
		}

		// If we were fleeing, but we've lost sight of the thing scaring us, stop
		if (IsCurSchedule(SCHED_BALL_FLEE) && !HasCondition(COND_SEE_FEAR))
		{
			TaskComplete();
			break;
		}

		if (!GetNavigator()->IsGoalActive())
		{
			TaskComplete();
			return;
		}

		// Start turning early
		if ((GetLocalOrigin() - GetNavigator()->GetCurWaypointPos()).Length() <= 64)
		{
			if (GetNavigator()->CurWaypointIsGoal())
			{
				// Hit the brakes a bit.
				float yaw = UTIL_VecToYaw(GetNavigator()->GetCurWaypointPos() - GetLocalOrigin());
				Vector vecRight;
				AngleVectors(QAngle(0, yaw, 0), NULL, &vecRight, NULL);

				m_RollerController.m_vecAngular += WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecRight, -GetForwardSpeed() * 5);

				TaskComplete();
				return;
			}

			GetNavigator()->AdvancePath();
		}

		{
			float flForwardSpeed = GetForwardSpeed();

			float yaw = UTIL_VecToYaw(GetNavigator()->GetCurWaypointPos() - GetLocalOrigin());

			Vector vecRight;
			Vector vecToPath; // points at the path
			AngleVectors(QAngle(0, yaw, 0), &vecToPath, &vecRight, NULL);

			// figure out if the roller is turning. If so, cut the throttle a little.
			float flDot;
			Vector vecVelocity;

			IPhysicsObject* pPhysicsObject = VPhysicsGetObject();

			if (pPhysicsObject == NULL)
			{
				assert(0);
				TaskFail("Roller lost internal physics object?");
				return;
			}

			pPhysicsObject->GetVelocity(&vecVelocity, NULL);

			VectorNormalize(vecVelocity);

			vecVelocity.z = 0;

			flDot = DotProduct(vecVelocity, vecToPath);

			m_RollerController.m_vecAngular = vec3_origin;

			if (flDot > 0.25 && flDot < 0.7)
			{
				// Feed a little torque backwards into the axis perpendicular to the velocity.
				// This will help get rid of momentum that would otherwise make us overshoot our goal.
				Vector vecCompensate;

				vecCompensate.x = vecVelocity.y;
				vecCompensate.y = -vecVelocity.x;
				vecCompensate.z = 0;

				m_RollerController.m_vecAngular = WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecCompensate, flForwardSpeed * -0.75);
			}

			m_RollerController.m_vecAngular += WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecRight, flForwardSpeed);
		}
		break;

	case TASK_BALL_CHARGE_ENEMY:
	{
		if (!GetEnemy())
		{
			TaskFail(FAIL_NO_ENEMY);
			break;
		}

		if (m_bHeld)
		{
			TaskComplete();
			break;
		}

		CBaseEntity* pEnemy = GetEnemy();
		Vector vecTargetPosition = pEnemy->GetAbsOrigin();

		float flTorqueFactor;
		Vector vecToTarget = vecTargetPosition - GetLocalOrigin();
		float yaw = UTIL_VecToYaw(vecToTarget);
		Vector vecRight;

		AngleVectors(QAngle(0, yaw, 0), NULL, &vecRight, NULL);

		//NDebugOverlay::Line( GetLocalOrigin(), GetLocalOrigin() + (GetEnemy()->GetLocalOrigin() - GetLocalOrigin()), 0,255,0, true, 0.1 );

		float flDot;

		// Figure out whether to continue the charge.
		// (Have I overrun the target?)			
		IPhysicsObject* pPhysicsObject = VPhysicsGetObject();

		if (pPhysicsObject == NULL)
		{
			//				Assert(0);
			TaskFail("Roller lost internal physics object?");
			return;
		}

		Vector vecVelocity;
		pPhysicsObject->GetVelocity(&vecVelocity, NULL);
		VectorNormalize(vecVelocity);

		VectorNormalize(vecToTarget);

		flDot = DotProduct(vecVelocity, vecToTarget);

		// more torque the longer the roller has been going.
		flTorqueFactor = 1 + (gpGlobals->curtime - m_flChargeTime) * 2;

		float flMaxTorque = BALL_MAX_TORQUE_FACTOR;

		if (flTorqueFactor < 1)
		{
			flTorqueFactor = 1;
		}
		else if (flTorqueFactor > flMaxTorque)
		{
			flTorqueFactor = flMaxTorque;
		}

		Vector vecCompensate;

		vecCompensate.x = vecVelocity.y;
		vecCompensate.y = -vecVelocity.x;
		vecCompensate.z = 0;
		VectorNormalize(vecCompensate);

		float flForwardSpeed = GetForwardSpeed();

		m_RollerController.m_vecAngular = WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecCompensate, flForwardSpeed * -0.75);
		m_RollerController.m_vecAngular += WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecRight, flForwardSpeed * flTorqueFactor);

		if (vecVelocity.x != 0 && vecVelocity.y != 0)
		{
			if (gpGlobals->curtime - m_flChargeTime > 1.0 && flTorqueFactor > 1 && flDot < 0.0)
			{

				TaskComplete();
			}
		}
	}
	break;

	case TASK_BALL_RETURN_TO_PLAYER:
	{
		if (ConditionsGathered() && !HasCondition(COND_SEE_PLAYER))
		{
			TaskFail(FAIL_NO_PLAYER);
			return;
		}

		CBaseEntity* pPlayer = gEntList.FindEntityByName(NULL, "!player");
		if (!pPlayer || m_bHeld)
		{
			TaskFail(FAIL_NO_TARGET);
			return;
		}

		Vector vecTargetPosition = pPlayer->GetAbsOrigin();
		float flTorqueFactor;
		Vector vecToTarget = vecTargetPosition - GetLocalOrigin();
		float yaw = UTIL_VecToYaw(vecToTarget);
		Vector vecRight;

		AngleVectors(QAngle(0, yaw, 0), NULL, &vecRight, NULL);

		float flDot;

		IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
		if (pPhysicsObject == NULL)
		{
			TaskFail("Roller lost internal physics object?");
			return;
		}

		Vector vecVelocity;
		pPhysicsObject->GetVelocity(&vecVelocity, NULL);
		VectorNormalize(vecVelocity);
		VectorNormalize(vecToTarget);

		flDot = DotProduct(vecVelocity, vecToTarget);

		// more torque the longer the roller has been going.
		flTorqueFactor = 1 + (gpGlobals->curtime - m_flChargeTime) * 2;

		float flMaxTorque = BALL_MAX_TORQUE_FACTOR * 0.75;
		if (flTorqueFactor < 1)
		{
			flTorqueFactor = 1;
		}
		else if (flTorqueFactor > flMaxTorque)
		{
			flTorqueFactor = flMaxTorque;
		}

		Vector vecCompensate;

		vecCompensate.x = vecVelocity.y;
		vecCompensate.y = -vecVelocity.x;
		vecCompensate.z = 0;
		VectorNormalize(vecCompensate);

		float flForwardSpeed = GetForwardSpeed();

		m_RollerController.m_vecAngular = WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecCompensate, flForwardSpeed * -0.75);
		m_RollerController.m_vecAngular += WorldToLocalRotation(SetupMatrixAngles(GetLocalAngles()), vecRight, flForwardSpeed * flTorqueFactor);

		// Once we're near the player, slow & stop
		if (GetAbsOrigin().DistToSqr(vecTargetPosition) < (BALL_RETURN_TO_PLAYER_DIST * 2.0))
		{
			TaskComplete();
		}
	}
	break;

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
// Output : float
//-----------------------------------------------------------------------------
float CNPC_AndroidBall::GetAttackDamageScale(CBaseEntity* pVictim)
{
	return BaseClass::GetAttackDamageScale(pVictim);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::NotifyInteraction(CAI_BaseNPC* pUser)
{
	// For now, turn green so we can tell who is hacked.
	GetEnemies()->SetFreeKnowledgeDuration(30.0f);

#ifdef MAPBASE
	m_OnHacked.FireOutput(pUser, this);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::VPhysicsCollision(int index, gamevcollisionevent_t* pEvent)
{
	// Make sure we don't keep hitting the same entity
	int otherIndex = !index;
	CBaseEntity* pOther = pEvent->pEntities[otherIndex];
	if (pEvent->deltaCollisionTime < 0.5 && (pOther == this))
		return;

	BaseClass::VPhysicsCollision(index, pEvent);

	// If we've just hit a vehicle, we want to stick to it
	if (m_bHeld)
	{
		// Are we supposed to be embedding ourselves?
		if (m_bEmbedOnGroundImpact)
		{
			// clear the flag so we don't queue more than once
			m_bEmbedOnGroundImpact = false;
		}
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysGunUser - 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason)
{
	// Are we just being punted?
	if (reason == PUNTED_BY_CANNON)
	{
		// Be stunned
		m_flActiveTime = gpGlobals->curtime + GetStunDelay();
		return;
	}

	//Stop turning
	m_RollerController.m_vecAngular = vec3_origin;

	m_OnPhysGunPickup.FireOutput(pPhysGunUser, this);
	m_bHeld = true;
	m_RollerController.Off();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysGunUser - 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::OnPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason)
{
	m_bHeld = false;
	m_flActiveTime = gpGlobals->curtime + GetStunDelay();
	m_RollerController.On();

	// explode on contact if launched from the physgun
	if (Reason == LAUNCHED_BY_CANNON)
	{
		if (m_bIsOpen)
		{
			// enable world/prop touch too
			VPhysicsGetObject()->SetCallbackFlags(VPhysicsGetObject()->GetCallbackFlags() | CALLBACK_GLOBAL_TOUCH | CALLBACK_GLOBAL_TOUCH_STATIC);
		}
	}

	m_OnPhysGunDrop.FireOutput(pPhysGunUser, this);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : float
//-----------------------------------------------------------------------------
int CNPC_AndroidBall::OnTakeDamage(const CTakeDamageInfo& info)
{
	if (!(info.GetDamageType() & DMG_BURN))
	{
		if (GetMoveType() == MOVETYPE_VPHYSICS)
		{
			AngularImpulse	angVel;
			angVel.Random(-400.0f, 400.0f);
			VPhysicsGetObject()->AddVelocity(NULL, &angVel);
			m_RollerController.m_vecAngular *= 0.8f;

			VPhysicsTakeDamage(info);
		}
		SetCondition(COND_LIGHT_DAMAGE);
	}

	if (info.GetDamageType() & (DMG_BURN | DMG_BLAST))
	{
		if (info.GetAttacker() && info.GetAttacker()->m_iClassname != m_iClassname)
		{
			SetNextThink(gpGlobals->curtime + random->RandomFloat(0.1f, 0.5f));
		}
		else
		{
			// dazed
			m_RollerController.m_vecAngular.Init();
			m_flActiveTime = gpGlobals->curtime + GetStunDelay();
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Makes warning noise before actual explosion occurs
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::PreDetonate(void)
{
	SetTouch(NULL);
	SetNextThink(gpGlobals->curtime + 0.5f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::Explode(void)
{
	m_takedamage = DAMAGE_NO;

	//FIXME: Hack to make thrown mines more deadly and fun
	float expDamage = 25;

	// Underwater explosion?
	if (UTIL_PointContents(GetAbsOrigin()) & MASK_WATER)
	{
		CEffectData	data;
		data.m_vOrigin = WorldSpaceCenter();
		data.m_flMagnitude = expDamage;
		data.m_flScale = 128;
		data.m_fFlags = (SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE);
		DispatchEffect("WaterSurfaceExplosion", data);
	}
	else
	{
		ExplosionCreate(WorldSpaceCenter(), GetLocalAngles(), this, expDamage, 128, true);
	}

	CTakeDamageInfo	info(this, this, 1, DMG_GENERIC);
	Event_Killed(info);

	// Remove myself a frame from now to avoid doing it in the middle of running AI
	SetThink(&CNPC_AndroidBall::SUB_Remove);
	SetNextThink(gpGlobals->curtime);
}

const float MAX_ROLLING_SPEED = 720;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_AndroidBall::RollingSpeed()
{
	IPhysicsObject* pPhysics = VPhysicsGetObject();
	if (!m_bHeld && pPhysics && !pPhysics->IsAsleep())
	{
		AngularImpulse angVel;
		pPhysics->GetVelocity(NULL, &angVel);
		float rollingSpeed = angVel.Length() - 90;
		rollingSpeed = clamp(rollingSpeed, 1, MAX_ROLLING_SPEED);
		rollingSpeed *= (1 / MAX_ROLLING_SPEED);
#ifdef MAPBASE
		if (m_flSpeedModifier != 1.0f)
		{
			rollingSpeed *= m_flSpeedModifier;
		}
#endif
		return rollingSpeed;
	}
	return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_AndroidBall::GetStunDelay()
{
	return sk_ball_stun_delay.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: We've been dropped by a dropship. Embed in the ground if we land on it.
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::EmbedOnGroundImpact()
{
	m_bEmbedOnGroundImpact = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::PrescheduleThink()
{
	// Are we underwater?
	if (UTIL_PointContents(GetAbsOrigin()) & MASK_WATER)
	{
		// As soon as we're far enough underwater, detonate
		Vector vecAboveMe = GetAbsOrigin() + Vector(0, 0, 64);
		if (UTIL_PointContents(vecAboveMe) & MASK_WATER)
		{
			Explode();
			return;
		}
	}
	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_AndroidBall::IsValidEnemy(CBaseEntity* pEnemy)
{
	// If the enemy's over the vehicle detection range, and it's not a player in a vehicle, ignore it
	if (pEnemy)
	{
		// Never pick something I fear
		if (IRelationType(pEnemy) == D_FR)
			return false;

		// Don't attack flying things.
		if (pEnemy->GetMoveType() == MOVETYPE_FLY)
			return false;
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//			&vecDir - 
//			*ptr - 
//-----------------------------------------------------------------------------
void CNPC_AndroidBall::TraceAttack(const CTakeDamageInfo& info, const Vector& vecDir, trace_t* ptr, CDmgAccumulator* pAccumulator)
{
	if (info.GetDamageType() & (DMG_BULLET | DMG_CLUB))
	{
		CTakeDamageInfo newInfo(info);

		// If we're stuck to the car, increase it even more
		newInfo.SetDamageForce(info.GetDamageForce() * 20);

		BaseClass::TraceAttack(newInfo, vecDir, ptr, pAccumulator);
		return;
	}

	BaseClass::TraceAttack(info, vecDir, ptr, pAccumulator);
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC(npc_androidball, CNPC_AndroidBall)

//Tasks
DECLARE_TASK(TASK_BALL_CHARGE_ENEMY)
DECLARE_TASK(TASK_BALL_GET_PATH_TO_FLEE)
DECLARE_TASK(TASK_BALL_NUDGE_TOWARDS_NODES)
DECLARE_TASK(TASK_BALL_RETURN_TO_PLAYER)
DECLARE_TASK(TASK_BALL_UNBALL)

//Schedules

DEFINE_SCHEDULE
(
	SCHED_BALL_RANGE_ATTACK1,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY"
	"		TASK_BALL_CHARGE_ENEMY	0"
	"	"
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_OCCLUDED"
	"		COND_ENEMY_TOO_FAR"
)

DEFINE_SCHEDULE
(
	SCHED_BALL_CHASE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_BALL_RANGE_ATTACK1"
	"		TASK_SET_TOLERANCE_DISTANCE		24"
	"		TASK_GET_PATH_TO_ENEMY			0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"	"
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_UNREACHABLE"
	"		COND_ENEMY_TOO_FAR"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_TASK_FAILED"
	"		COND_SEE_FEAR"
)

DEFINE_SCHEDULE
(
	SCHED_BALL_FLEE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_IDLE_STAND"
	"		TASK_BALL_GET_PATH_TO_FLEE	300"
	"		TASK_RUN_PATH						0"
	"		TASK_STOP_MOVING					0"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_BALL_ALERT_STAND,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_REASONABLE		0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
	"		TASK_WAIT					2"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_PROVOKED"
	"		COND_SMELL"
	"		COND_HEAR_COMBAT"		// sound flags
	"		COND_HEAR_WORLD"
	"		COND_HEAR_PLAYER"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_BULLET_IMPACT"
	"		COND_IDLE_INTERRUPT"
)

DEFINE_SCHEDULE
(
	SCHED_BALL_NUDGE_TOWARDS_NODES,

	"	Tasks"
	"		TASK_BALL_NUDGE_TOWARDS_NODES		0"
	"		TASK_WAIT								1.5"
	""
	"	Interrupts"
	""
)

DEFINE_SCHEDULE
(
	SCHED_BALL_PATH_TO_PLAYER,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_BALL_ALERT_STAND"
	"		TASK_SET_TOLERANCE_DISTANCE		200"
	"		TASK_GET_PATH_TO_PLAYER			0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_PROVOKED"
	"		COND_SMELL"
	"		COND_HEAR_COMBAT"		// sound flags
	"		COND_HEAR_WORLD"
	"		COND_HEAR_PLAYER"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_BULLET_IMPACT"
	"		COND_IDLE_INTERRUPT"
	"		COND_SEE_PLAYER"
)

DEFINE_SCHEDULE
(
	SCHED_BALL_ROLL_TO_PLAYER,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_BALL_ALERT_STAND"
	"		TASK_SET_TOLERANCE_DISTANCE			200"
	"		TASK_BALL_RETURN_TO_PLAYER	0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_PROVOKED"
	"		COND_SMELL"
	"		COND_HEAR_COMBAT"		// sound flags
	"		COND_HEAR_WORLD"
	"		COND_HEAR_PLAYER"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_BULLET_IMPACT"
	"		COND_IDLE_INTERRUPT"
)
DEFINE_SCHEDULE
(
	SCHED_BALL_UNBALL,
	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_BALL_UNBALL	0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"       COND_LOST_ENEMY"
	"		COND_ANDROID_ZAPPED"
)

AI_END_CUSTOM_NPC()
