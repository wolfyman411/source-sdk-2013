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

#include "ai_blended_movement.h"
#include "soundent.h"
#include "ai_basenpc.h"
#include "ai_behavior.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_follow.h"
#include "ai_sentence.h"
#include "ai_baseactor.h"

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

	CNPC_Android(void);

	float		GetIdealAccel(void) const;
	float		MaxYawSpeed(void);
	bool		FInViewCone(CBaseEntity* pEntity);
	bool		FInViewCone(const Vector& vecSpot);

	void		Activate(void);
	void		HandleAnimEvent(animevent_t* pEvent);
	void		StartTask(const Task_t* pTask);
	void		RunTask(const Task_t* pTask);
	void		IdleSound(void);
	void		PainSound(const CTakeDamageInfo& info);
	void		Precache(void);
	void		Spawn(void);
	int			OnTakeDamage_Alive(const CTakeDamageInfo& info);
	void		TraceAttack(const CTakeDamageInfo& info, const Vector& vecDir, trace_t* ptr, CDmgAccumulator* pAccumulator);
	void		BuildScheduleTestBits(void);
	void		GatherConditions(void);
	void		PrescheduleThink(void);
	void		ZapThink(void);
	bool		CreateVPhysics();

	bool		QuerySeeEntity(CBaseEntity* pEntity, bool bOnlyHateOrFearIfNPC = false);
	bool		ShouldPlayIdleSound(void);
	bool		OverrideMoveFacing(const AILocalMoveGoal_t& move, float flInterval);
	bool		IsValidEnemy(CBaseEntity* pEnemy);
	bool		QueryHearSound(CSound* pSound);
	bool		IsLightDamage(const CTakeDamageInfo& info);
	bool		CreateBehaviors(void);
	int			SelectSchedule(void);

	void		Touch(CBaseEntity* pOther);

	virtual int		RangeAttack1Conditions(float flDot, float flDist);
	virtual int		RangeAttack2Conditions(float flDot, float flDist);
	virtual int		MeleeAttack1Conditions(float flDot, float flDist);
	virtual int		GetSoundInterests(void) { return (BaseClass::GetSoundInterests()) | (SOUND_DANGER | SOUND_PHYSICS_DANGER | SOUND_THUMPER | SOUND_BUGBAIT); }
	virtual	bool	IsHeavyDamage(const CTakeDamageInfo& info);

	Class_T		Classify(void) { return CLASS_ANTLION; }

	void		Event_Killed(const CTakeDamageInfo& info);
	void		GatherEnemyConditions(CBaseEntity* pEnemy);

	int			TranslateSchedule(int scheduleType);

	virtual		Activity NPC_TranslateActivity(Activity baseAct);

	DECLARE_DATADESC();

	virtual float GetAutoAimRadius() { return 36.0f; }

	void	Zap(bool bZapped = false);


private:

	bool	FindChasePosition(const Vector& targetPos, Vector& result);
	bool	GetGroundPosition(const Vector& testPos, Vector& result);
	inline bool	IsZapped(void);

	bool	Alone(void);

	void	MeleeAttack(float distance, float damage, QAngle& viewPunch, Vector& shove);

	int		SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);

	virtual bool CanRunAScriptedNPCInteraction(bool bForced = false);

	virtual void Ignite(float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner);
	virtual bool InnateWeaponLOSCondition(const Vector& ownerPos, const Vector& targetPos, bool bSetConditions);
	virtual bool FCanCheckAttacks(void);

	bool SeenEnemyWithinTime(float flTime);
	void DelaySquadAttack(float flDuration);

	float		m_flIdleDelay;
	float		m_flIgnoreSoundTime;

	CAI_AssaultBehavior			m_AssaultBehavior;

	Vector		m_vecHeardSound;
	bool		m_bHasHeardSound;
	bool		m_bAgitatedSound;	//Playing agitated sound?

	int			m_nBodyBone;

	// Used to trigger a heavy damage interrupt if sustained damage is taken
	int			m_nSustainedDamage;
	float		m_flLastDamageTime;
	float		m_flZapDuration;
	float		m_flEludeDistance;

protected:
	int m_poseHead_Yaw, m_poseHead_Pitch;
	virtual void	PopulatePoseParameters(void);

private:

	HSOUNDSCRIPTHANDLE	m_hFootstep;

	DEFINE_CUSTOM_AI;

	//==================================================
	// AntlionConditions
	//==================================================

	enum
	{
		COND_ANDROID_ZAPPED = LAST_SHARED_CONDITION,
	};

	//==================================================
	// AntlionSchedules
	//==================================================

	enum
	{
		SCHED_ANDROID_ZAP_RECOVER,
		SCHED_ANDROID_BALL,
		SCHED_ANDROID_UNBALL,
	};

	//==================================================
	// AntlionTasks
	//==================================================

	enum
	{
		TASK_ANDROID_BALL,
		TASK_ANDROID_UNBALL,
	};
};
#endif // HL2_EPISODIC