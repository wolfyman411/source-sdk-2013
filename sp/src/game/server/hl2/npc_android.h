//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_ANDROID_H
#define NPC_ANDROID_H
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

class CNPC_Android;


enum Android_Weapons_e
{
	ANDROID_HAND,
	ANDROID_LASER,
	ANDROID_NAIL,
};

class CNPC_Android : public CAI_BaseActor
{
public:
	DECLARE_CLASS(CNPC_Android, CAI_BaseActor);

	void	Spawn(void);
	void	Precache(void);
	Class_T Classify(void);
	void	HandleAnimEvent(animevent_t* pEvent);
	int		GetSoundInterests(void);
	bool	CreateBehaviors(void);
	int		SelectSchedule(void);
	void	GatherConditions(void);
	void	PrescheduleThink(void);

	DECLARE_DATADESC()

private:
	CAI_FollowBehavior	m_FollowBehavior;
	void	UpdateHead(void);

protected:
	DEFINE_CUSTOM_AI;
	int m_poseHead_Yaw, m_poseHead_Pitch;
	virtual void PopulatePoseParameters(void);

	//Schedules
	enum
	{
		SCHED_ANDROID_CHASE_ENEMY,
	};
};

#endif // HL2_EPISODIC