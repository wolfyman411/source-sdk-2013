//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_ANDROIDBALL_H
#define NPC_ANDROIDBALL_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "hl2_gamerules.h"
#include "ai_behavior.h"
#include "ai_baseactor.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_follow.h"
#include "explode.h"
#include "props_shared.h"

class CNPC_AndroidBall;


class CNPC_AndroidBall : public CAI_BaseActor
{
public:
	DECLARE_CLASS(CNPC_AndroidBall, CAI_BaseActor);

	void	Spawn(void);
	void	Precache(void);
	Class_T Classify(void);
	bool	CreateBehaviors(void);

	void	Event_Killed(const CTakeDamageInfo& info);

	void	StartTask(const Task_t* pTask);
	void	RunTask(const Task_t* pTask);

	int		SelectSchedule(void);
	int		TranslateSchedule(int scheduleType);
	void	GatherConditions(void);
	void	Think(void);
	void	PrescheduleThink(void);

	DECLARE_DATADESC()

private:
	CAI_FollowBehavior	m_FollowBehavior;

	DEFINE_CUSTOM_AI;

protected:
};

class CAndroidBallController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	IMotionEvent::simresult_e Simulate(IPhysicsMotionController* pController, IPhysicsObject* pObject, float deltaTime, Vector& linear, AngularImpulse& angular);

	AngularImpulse	m_vecAngular;
	Vector			m_vecLinear;

	void Off(void) { m_fIsStopped = true; }
	void On(void) { m_fIsStopped = false; }

	bool IsOn(void) { return !m_fIsStopped; }

private:
	bool	m_fIsStopped;
};

#endif // HL2_EPISODIC