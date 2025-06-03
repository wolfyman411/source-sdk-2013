#include "cbase.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_behavior_follow.h"
#include "npc_androidball.h"
#include "ai_schedule.h"
#include "util.h"
#include "tier0/memdbgon.h"
#include <particle_parse.h>
#include <ammodef.h>
#include <physics_saverestore.h>

BEGIN_SIMPLE_DATADESC(CAndroidBallController)

DEFINE_FIELD(m_vecAngular, FIELD_VECTOR),
DEFINE_FIELD(m_vecLinear, FIELD_VECTOR),
DEFINE_FIELD(m_fIsStopped, FIELD_BOOLEAN),

END_DATADESC()


//-----------------------------------------------------------------------------
IMotionEvent::simresult_e CAndroidBallController::Simulate(IPhysicsMotionController* pController, IPhysicsObject* pObject, float deltaTime, Vector& linear, AngularImpulse& angular)
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

#define ANDROIDBALL_MODEL "models/aperture/android_ball.mdl"

LINK_ENTITY_TO_CLASS(npc_androidball, CNPC_AndroidBall);

BEGIN_DATADESC(CNPC_AndroidBall)

DEFINE_EMBEDDED(m_RollerController),
DEFINE_PHYSPTR(m_pMotionController),

END_DATADESC()

void CNPC_AndroidBall::Spawn()
{
	Precache();

	SetModel(ANDROIDBALL_MODEL);

	SetHullType(HULL_MEDIUM);
	SetHullSizeNormal();

	SetSolid(SOLID_VPHYSICS);
	AddSolidFlags(FSOLID_FORCE_WORLD_ALIGNED | FSOLID_NOT_STANDABLE);

	BaseClass::Spawn();

	SetMoveType(MOVETYPE_STEP);
	SetNavType(NAV_GROUND);

	SetBloodColor(BLOOD_COLOR_MECH);

	m_NPCState = NPC_STATE_NONE;

	CapabilitiesAdd(bits_CAP_MOVE_GROUND);

	if (m_SquadName != NULL_STRING)
	{
		CapabilitiesAdd(bits_CAP_SQUAD);
	}

	NPCInit();

	m_takedamage = DAMAGE_EVENTS_ONLY;

	m_flGroundSpeed = 20;
	m_NPCState = NPC_STATE_NONE;

	GetMotor()->SetYawSpeed(0.0f);
}

void CNPC_AndroidBall::Precache()
{
	PrecacheModel(ANDROIDBALL_MODEL);

	BaseClass::Precache();
}

Class_T	CNPC_AndroidBall::Classify(void)
{
	return CLASS_APERTURE;
}

bool CNPC_AndroidBall::CreateBehaviors()
{
	AddBehavior(&m_FollowBehavior);

	return BaseClass::CreateBehaviors();
}

void CNPC_AndroidBall::Event_Killed(const CTakeDamageInfo& info)
{
	SetCondition(COND_SCHEDULE_DONE);
}

void CNPC_AndroidBall::StartTask(const Task_t* pTask)
{

	BaseClass::StartTask(pTask);
}

void CNPC_AndroidBall::RunTask(const Task_t* pTask)
{
	BaseClass::RunTask(pTask);
}

int CNPC_AndroidBall::SelectSchedule(void)
{
	return BaseClass::SelectSchedule();
}

int CNPC_AndroidBall::TranslateSchedule(int scheduleType)
{
	return BaseClass::TranslateSchedule(scheduleType);
}

void CNPC_AndroidBall::GatherConditions(void)
{
	BaseClass::GatherConditions();
}

void CNPC_AndroidBall::Think(void)
{
	BaseClass::Think();
}

void CNPC_AndroidBall::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();
}

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

AI_BEGIN_CUSTOM_NPC(npc_androidball, CNPC_AndroidBall)


AI_END_CUSTOM_NPC()