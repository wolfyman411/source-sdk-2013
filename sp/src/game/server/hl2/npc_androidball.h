//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#ifndef NPC_ANDROIDBALL_H
#define NPC_ANDROIDBALL_H
#ifdef _WIN32
#pragma once
#endif
#endif

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

class CBallController : public IMotionEvent
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

class CNPC_AndroidBall : public CNPCBaseInteractive<CAI_BaseNPC>, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS(CNPC_AndroidBall, CNPCBaseInteractive<CAI_BaseNPC>);

public:

	CNPC_AndroidBall(void) { m_bUniformSight = false; }
	~CNPC_AndroidBall(void);

	void	SetVars(float health, Android_Weapons_e leftWeapon = ANDROID_NONE, Android_Weapons_e rightWeapon = ANDROID_NONE);

	void	Spawn(void);
	bool	CreateVPhysics();
	void	RunAI();
	void	StartTask(const Task_t* pTask);
	void	RunTask(const Task_t* pTask);
	float	GetAttackDamageScale(CBaseEntity* pVictim);
	void	VPhysicsCollision(int index, gamevcollisionevent_t* pEvent);
	void	Precache(void);
	void	OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason);
	void	OnPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason);
	void	PrescheduleThink();
	bool	ShouldSavePhysics() { return true; }
	void	OnRestore();
	bool	QuerySeeEntity(CBaseEntity* pSightEnt, bool bOnlyHateOrFearIfNPC = false);

	int		RangeAttack1Conditions(float flDot, float flDist);
	int		SelectSchedule(void);
	int		TranslateSchedule(int scheduleType);

	bool	OverrideMove(float flInterval) { return true; }
	bool	IsValidEnemy(CBaseEntity* pEnemy);
	float	RollingSpeed();
	float	GetStunDelay();
	void	EmbedOnGroundImpact();
	void	UpdateEfficiency(bool bInPVS) { SetEfficiency((GetSleepState() != AISS_AWAKE) ? AIE_DORMANT : AIE_NORMAL); SetMoveEfficiency(AIME_NORMAL); }
	void	DrawDebugGeometryOverlays()
	{
		if (m_debugOverlays & OVERLAY_BBOX_BIT)
		{
			float dist = GetSenses()->GetDistLook();
			Vector range(dist, dist, 64);
			NDebugOverlay::Box(GetAbsOrigin(), -range, range, 255, 0, 0, 0, 0);
		}
		BaseClass::DrawDebugGeometryOverlays();
	}
	// UNDONE: Put this in the qc file!
	Vector EyePosition()
	{
		// This takes advantage of the fact that the system knows
		// that the abs origin is at the center of the rollermine
		// and that the OBB is actually world-aligned despite the
		// fact that SOLID_VPHYSICS is being used
		Vector eye = CollisionProp()->GetCollisionOrigin();
		eye.z += CollisionProp()->OBBMaxs().z;
		return eye;
	}

	int		OnTakeDamage(const CTakeDamageInfo& info);
	void	TraceAttack(const CTakeDamageInfo& info, const Vector& vecDir, trace_t* ptr, CDmgAccumulator* pAccumulator);

	Class_T	Classify()
	{
		return CLASS_APERTURE;
	}

	virtual bool ShouldGoToIdleState()
	{
		return gpGlobals->curtime > m_flGoIdleTime ? true : false;
	}

	virtual	void OnStateChange(NPC_STATE OldState, NPC_STATE NewState);

	NPC_STATE SelectIdealState();

	// Vehicle sticking

	virtual unsigned int	PhysicsSolidMaskForEntity(void) const;

	COutputEvent m_OnPhysGunDrop;
	COutputEvent m_OnPhysGunPickup;

	bool m_retreating = false;

private:

	bool unball = false;

	float android_health = 0.0f;
	Android_Weapons_e forced_left;
	Android_Weapons_e forced_right;

	enum
	{
		SCHED_BALL_RANGE_ATTACK1 = LAST_SHARED_SCHEDULE,
		SCHED_BALL_CHASE_ENEMY,
		SCHED_BALL_FLEE,
		SCHED_BALL_RETREAT,
		SCHED_BALL_ALERT_STAND,
		SCHED_BALL_NUDGE_TOWARDS_NODES,
		SCHED_BALL_PATH_TO_PLAYER,
		SCHED_BALL_ROLL_TO_PLAYER,
		SCHED_BALL_UNBALL,
	};

	enum
	{
		TASK_BALL_CHARGE_ENEMY = LAST_SHARED_TASK,
		TASK_BALL_GET_PATH_TO_FLEE,
		TASK_BALL_GET_PATH_TO_RETREAT,
		TASK_BALL_NUDGE_TOWARDS_NODES,
		TASK_BALL_RETURN_TO_PLAYER,
		TASK_BALL_UNBALL,
	};

protected:
	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

	bool	BecomePhysical();

	void	Explode(void);
	void	PreDetonate(void);

	bool	IsActive() { return m_flActiveTime > gpGlobals->curtime ? false : true; }

	inline float	GetForwardSpeed() const
	{
#ifdef MAPBASE
		if (m_flSpeedModifier != 1.0f)
			return m_flForwardSpeed * m_flSpeedModifier;
		else
#endif
			return m_flForwardSpeed;
	}

	// INPCInteractive Functions
	virtual bool	CanInteractWith(CAI_BaseNPC* pUser) { return true; }
	virtual void	NotifyInteraction(CAI_BaseNPC* pUser);

	CSoundPatch* m_pRollSound;
	CSoundPatch* m_pPingSound;

	CBallController			m_RollerController;
	IPhysicsMotionController* m_pMotionController;

	float	m_flChargeTime;
	float	m_flGoIdleTime;
	float	m_flForwardSpeed;
	int		m_iSoundEventFlags;

	CNetworkVar(bool, m_bIsOpen);
	CNetworkVar(float, m_flActiveTime);	//If later than the current time, this will force the mine to be active

	bool	m_bHeld;		//Whether or not the player is holding the mine
	bool	m_bEmbedOnGroundImpact;

	// Constraint used to stick us to a vehicle
	IPhysicsConstraint* m_pConstraint;

	bool	m_bUniformSight;

	static string_t gm_iszDropshipClassname;
};