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
#include <beam_shared.h>
#include <Sprite.h>
#include <smoke_trail.h>
#include <particle_system.h>
#include "explode.h"
#include "props_shared.h"

class CNPC_Android;


enum Android_Weapons_e
{
	ANDROID_HAND,
	ANDROID_LASER,
	ANDROID_GUN,
	ANDROID_NONE,
};

class CNPC_Android : public CAI_BaseActor
{
public:
	DECLARE_CLASS(CNPC_Android, CAI_BaseActor);

	void	SetVars(float health, Android_Weapons_e leftWeapon, Android_Weapons_e rightWeapon);

	void	Spawn(void);
	void	Activate();
	void	Precache(void);
	Class_T Classify(void);
	void	HandleAnimEvent(animevent_t* pEvent);
	int		GetSoundInterests(void);
	float	GetIdealAccel(void) const;
	float	MaxYawSpeed(void);
	bool	CreateBehaviors(void);
	int		OnTakeDamage_Alive(const CTakeDamageInfo& info);

	//Damage Effects
	void	Zap(void);
	void	StartSmokeTrail(void);
	void	StartFire(void);
	void	Event_Killed(const CTakeDamageInfo& info);
	void	Gib(void);

	virtual int	RangeAttack1Conditions(float flDot, float flDist);

	void	LaserPosition(CBeam* beam, int attachment);
	void	CreateLaser(CBeam* &beam, CSprite* &sprite, int attachment);
	void	UpdateLaser(CBeam* beam, int attachment);
	void	LaserEndPoint();
	Vector	CalcEndPoint();
	void	KillLaser(CBeam* &beam, CSprite* &sprite);

	void	ShootGun(int attachment);

	bool	OverrideMoveFacing(const AILocalMoveGoal_t& move, float flInterval);

	void	StartTask(const Task_t* pTask);
	void	RunTask(const Task_t* pTask);

	int		SelectSchedule(void);
	void	SwapWeapon(Android_Weapons_e &ref);
	int		TranslateSchedule(int scheduleType);
	void	GatherConditions(void);
	void	Think(void);
	void	PrescheduleThink(void);

	//Weapon Hands
	Android_Weapons_e left_wpn = ANDROID_HAND;
	Android_Weapons_e right_wpn = ANDROID_HAND;

	Android_Weapons_e forced_left = ANDROID_NONE;
	Android_Weapons_e forced_right = ANDROID_NONE;
	
	//Lasers
	CBeam* m_pBeamL;
	CSprite* m_pLightGlowL;
	CBeam* m_pBeamR;
	CSprite* m_pLightGlowR;
	CSprite* m_pLightGlowEnd;

	bool	m_bPlayingLaserSound;
	Vector	m_laserEndpoint;
	Vector m_laserStartpoint;
	Vector m_laserTarget;

	//Effects
	SmokeTrail* m_pSmokeTrail;
	CParticleSystem* m_pFire;

	bool m_startBalled = false;

	DECLARE_DATADESC()

private:
	//Delays
	float	m_attackDurL;
	float	m_attackDurR;
	float	m_nextAttackL;
	float	m_nextAttackR;
	int	m_nextSwapL;
	int	m_nextSwapR;

	//Weapon Specifics
	int		m_gunShotsL;
	int		m_gunShotsR;

	//Zap
	bool m_zapped;
	float m_zaptime;

	float m_nextForwardMoveTime;

	bool m_ballMode = false;
	float m_tooClose = 0.0f;
	float m_lastBall = 0.0f;
	void	UpdateHead(void);
	
	DEFINE_CUSTOM_AI;

	//Schedules
	enum
	{
		SCHED_ANDROID_CHASE_ENEMY = LAST_SHARED_SCHEDULE,
		SCHED_ANDROID_LASER_ATTACK,
		SCHED_ANDROID_GUN_ATTACK,
		SCHED_ANDROID_FLANK_RANDOM,
		SCHED_ANDROID_RUN_RANDOM,
		SCHED_ANDROID_COVER_FROM_ENEMY,
		SCHED_ANDROID_ZAP,
		SCHED_ANDROID_BALL_MODE,
		SCHED_ANDROID_UNBALL,
	};

	//Tasks
	enum
	{
		TASK_ANDROID_LASER_ATTACK = LAST_SHARED_TASK,
		TASK_ANDROID_GUN_ATTACK,
		TASK_ANDROID_BALL_MODE,
	};

	//Conditions
	enum
	{
		COND_ANDROID_IS_LEFT = LAST_SHARED_CONDITION,
		COND_ANDROID_IS_RIGHT,
		COND_ANDROID_ZAPPED,
	};

protected:
	int m_poseHead_Yaw, m_poseHead_Pitch;
	virtual void PopulatePoseParameters(void);

	CAI_AssaultBehavior			m_AssaultBehavior;
	CAI_StandoffBehavior		m_StandoffBehavior;
	CAI_FollowBehavior			m_FollowBehavior;
};

#endif // HL2_EPISODIC