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

#define ANDROIDBALL_MODEL "models/aperture/android_ball.mdl"

LINK_ENTITY_TO_CLASS(npc_androidball, CNPC_AndroidBall);

BEGIN_DATADESC(CNPC_AndroidBall)

END_DATADESC()

void CNPC_AndroidBall::Spawn()
{
	Precache();

	SetModel(ANDROIDBALL_MODEL);

	SetHullType(HULL_MEDIUM);
	SetHullSizeNormal();
	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);

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

	BaseClass::Spawn();
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

AI_BEGIN_CUSTOM_NPC(npc_androidball, CNPC_AndroidBall)


AI_END_CUSTOM_NPC()