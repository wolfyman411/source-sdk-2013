//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Cute hound like Alien.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "AI_Default.h"
#include "AI_Schedule.h"
#include "AI_Hull.h"
#include "AI_Navigator.h"
#include "AI_Route.h"
#include "AI_Squad.h"
#include "AI_SquadSlot.h"
#include "AI_Hint.h"
#include "NPCEvent.h"
#include "animation.h"
#include "npc_houndeye.h"
#include "gib.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "AI_Senses.h"
#include "animation.h"
#include "util.h"
#include "shake.h"
#include "movevars_shared.h"
#include "hl2_shareddefs.h"
#include "particle_parse.h"
#include "vehicle_base.h"

// houndeye does 20 points of damage spread over a sphere 384 units in diameter, and each additional 
// squad member increases the BASE damage by 110%, per the spec.
#define HOUNDEYE_MAX_SQUAD_SIZE			10
#define	HOUNDEYE_MAX_ATTACK_RADIUS		384
#define	HOUNDEYE_SQUAD_BONUS			(float)1.1

#define HOUNDEYE_EYE_FRAMES 4 // how many different switchable maps for the eye

#define HOUNDEYE_SOUND_STARTLE_VOLUME	128 // how loud a sound has to be to badly scare a sleeping houndeye

#define HOUNDEYE_TOP_MASS	 300.0f

ConVar sk_houndeye_health("sk_houndeye_health", "35");
ConVar sk_houndeye_dmg_blast("sk_houndeye_dmg_blast", "15");


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		HOUND_AE_WARN			1
#define		HOUND_AE_STARTATTACK	2
#define		HOUND_AE_THUMP			3
#define		HOUND_AE_ANGERSOUND1	4
#define		HOUND_AE_ANGERSOUND2	5
#define		HOUND_AE_HOPBACK		6
#define		HOUND_AE_CLOSE_EYE		7
int ACT_HOUNDEYE_FLIP;

BEGIN_DATADESC(CNPC_Houndeye)
DEFINE_FIELD(m_iSpriteTexture, FIELD_INTEGER),
//DEFINE_FIELD(m_fAsleep, FIELD_BOOLEAN),
DEFINE_FIELD(m_fDontBlink, FIELD_BOOLEAN),
DEFINE_FIELD(m_vecPackCenter, FIELD_POSITION_VECTOR),

DEFINE_INPUTFUNC(FIELD_VOID, "StartPatrolling", InputStartPatrolling),
DEFINE_INPUTFUNC(FIELD_VOID, "StopPatrolling", InputStopPatrolling),
DEFINE_INPUTFUNC(FIELD_VOID, "StartSleep", InputStartSleep),

END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_houndeye, CNPC_Houndeye);

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_HOUND_CLOSE_EYE = LAST_SHARED_TASK,
	TASK_HOUND_OPEN_EYE,
	TASK_HOUND_THREAT_DISPLAY,
	TASK_HOUND_FALL_ASLEEP,
	TASK_HOUND_WAKE_UP,
	TASK_HOUND_HOP_BACK,
};

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_HOUND_AGITATED = LAST_SHARED_SCHEDULE,
	SCHED_HOUND_HOP_RETREAT,
	SCHED_HOUND_YELL1,
	SCHED_HOUND_YELL2,
	SCHED_HOUND_RANGEATTACK,
	SCHED_HOUND_SLEEP,
	SCHED_HOUND_WAKE_LAZY,
	SCHED_HOUND_WAKE_URGENT,
	SCHED_HOUND_SPECIALATTACK,
	SCHED_HOUND_COMBAT_FAIL_PVS,
	SCHED_HOUND_COMBAT_FAIL_NOPVS,
	SCHED_HOUND_CHASE_ENEMY,
	SCHED_HOUND_FLIP,
	//	SCHED_HOUND_FAIL,
};

enum {
	COND_HOUNDEYE_FLIPPED = LAST_SHARED_CONDITION,
};

enum HoundEyeSquadSlots
{
	SQUAD_SLOTS_HOUND_ATTACK = LAST_SHARED_SQUADSLOT
};


//=========================================================
// Spawn
//=========================================================
void CNPC_Houndeye::Spawn()
{
	Precache();

	SetRenderColor(255, 255, 255, 255);

	SetModel("models/model_xen/houndeye.mdl");

	SetHullType(HULL_SMALL);
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	m_bloodColor = BLOOD_COLOR_YELLOW;
	ClearEffects();
	m_iHealth = sk_houndeye_health.GetFloat();
	m_flFieldOfView = 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState = NPC_STATE_NONE;
	m_fDontBlink = FALSE;

	CapabilitiesClear();

	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1);
	CapabilitiesAdd(bits_CAP_SQUAD);

	NPCInit();

	if (m_fAsleep)
	{
		SetSchedule(SCHED_HOUND_SLEEP);
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Houndeye::Precache()
{
	PrecacheModel("models/model_xen/houndeye.mdl");

	m_iSpriteTexture = PrecacheModel("sprites/shockwave.vmt");

	PrecacheScriptSound("HoundEye.Idle");
	PrecacheScriptSound("HoundEye.Warn");
	PrecacheScriptSound("HoundEye.Hunt");
	PrecacheScriptSound("HoundEye.Alert");
	PrecacheScriptSound("HoundEye.Die");
	PrecacheScriptSound("HoundEye.Pain");
	PrecacheScriptSound("HoundEye.Anger1");
	PrecacheScriptSound("HoundEye.Anger2");
	PrecacheScriptSound("HoundEye.Sonic");

	PrecacheParticleSystem("houndeye_shockwave_1");
	PrecacheParticleSystem("houndeye_shockwave_2");
	PrecacheParticleSystem("houndeye_shockwave_3");

	BaseClass::Precache();
}

void CNPC_Houndeye::Event_Killed(const CTakeDamageInfo& info)
{
	// Close the eye to make death more obvious
	m_nSkin = 1;
	BaseClass::Event_Killed(info);
}

int CNPC_Houndeye::RangeAttack1Conditions(float flDot, float flDist)
{
	// I'm not allowed to attack if standing in another hound eye 
	// (note houndeyes allowed to interpenetrate)
	trace_t tr;
	UTIL_TraceHull(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, 0.1),
		GetHullMins(), GetHullMaxs(),
		MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);
	if (tr.startsolid)
	{
		CBaseEntity* pEntity = tr.m_pEnt;
		if (pEntity->Classify() == CLASS_HOUNDEYE)
		{
			return(COND_NONE);
		}
	}

	// If I'm really close to my enemy allow me to attack if 
	// I'm facing regardless of next attack time
	if (flDist < 100 && flDot >= 0.3)
	{
		return COND_CAN_RANGE_ATTACK1;
	}
	if (gpGlobals->curtime < m_flNextAttack)
	{
		return(COND_NONE);
	}
	if (flDist > (HOUNDEYE_MAX_ATTACK_RADIUS * 0.5))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	if (flDot < 0.3)
	{
		return COND_NOT_FACING_ATTACK;
	}
	return COND_CAN_RANGE_ATTACK1;
}

//=========================================================
// IdleSound
//=========================================================
void CNPC_Houndeye::IdleSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Idle");
}

//=========================================================
// IdleSound
//=========================================================
void CNPC_Houndeye::WarmUpSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Warn");
}

//=========================================================
// WarnSound 
//=========================================================
void CNPC_Houndeye::WarnSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Hunt");
}

//=========================================================
// AlertSound 
//=========================================================
void CNPC_Houndeye::AlertSound(void)
{

	if (m_pSquad && !m_pSquad->IsLeader(this))
		return; // only leader makes ALERT sound.

	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Alert");
}

//=========================================================
// DeathSound 
//=========================================================
void CNPC_Houndeye::DeathSound(const CTakeDamageInfo& info)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Die");
}

//=========================================================
// PainSound 
//=========================================================
void CNPC_Houndeye::PainSound(const CTakeDamageInfo& info)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Pain");
}

//=========================================================
// MaxYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CNPC_Houndeye::MaxYawSpeed(void)
{
	int flYS;

	flYS = 90;

	switch (GetActivity())
	{
	case ACT_CROUCHIDLE://sleeping!
		flYS = 0;
		break;
	case ACT_IDLE:
		flYS = 60;
		break;
	case ACT_WALK:
		flYS = 90;
		break;
	case ACT_RUN:
		flYS = 90;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		flYS = 90;
		break;
	}

	return flYS;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
Class_T	CNPC_Houndeye::Classify(void)
{
	return CLASS_HOUNDEYE;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Houndeye::HandleAnimEvent(animevent_t* pEvent)
{
	switch (pEvent->event)
	{
	case HOUND_AE_WARN:
		// do stuff for this event.
		WarnSound();
		break;

	case HOUND_AE_STARTATTACK:
		WarmUpSound();
		break;

	case HOUND_AE_HOPBACK:
	{
		float flGravity = sv_gravity.GetFloat();
		Vector v_forward;
		GetVectors(&v_forward, NULL, NULL);

		SetGroundEntity(NULL);

		Vector vecVel = v_forward * -200;
		vecVel.z += (0.6 * flGravity) * 0.5;
		SetAbsVelocity(vecVel);

		break;
	}

	case HOUND_AE_THUMP:
		// emit the shockwaves
		SonicAttack();
		break;

	case HOUND_AE_ANGERSOUND1:
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "HoundEye.Anger1");
	}
	break;

	case HOUND_AE_ANGERSOUND2:
	{
		CPASAttenuationFilter filter2(this);
		EmitSound(filter2, entindex(), "HoundEye.Anger2");
	}
	break;

	case HOUND_AE_CLOSE_EYE:
		if (!m_fDontBlink)
		{
			m_nSkin = HOUNDEYE_EYE_FRAMES - 1;
		}
		break;

	default:
		BaseClass::HandleAnimEvent(pEvent);
		break;
	}
}

inline bool CNPC_Houndeye::IsFlipped(void)
{
	return (GetActivity() == ACT_HOUNDEYE_FLIP);
}

//-----------------------------------------------------------------------------
// Purpose: Flip the antlion over
//-----------------------------------------------------------------------------
void CNPC_Houndeye::Flip(bool bZapped /*= false*/)
{
	// We can't flip an already flipped antlion
	if (IsFlipped())
		return;

	// Must be on the ground
	if ((GetFlags() & FL_ONGROUND) == false)
		return;

	// Can't be in a dynamic interation
	if (IsRunningDynamicInteraction())
		return;

	SetCondition(COND_HOUNDEYE_FLIPPED);
}

//-----------------------------------------------------------------------------
// Purpose: Antlion who are flipped will knock over other antlions behind them!
//-----------------------------------------------------------------------------
void CNPC_Houndeye::CascadePush(const Vector& vecForce)
{
	// Controlled via this convar until this is proven worthwhile
	if (hl2_episodic.GetBool() == false /*|| g_antlion_cascade_push.GetBool() == false*/)
		return;

	Vector vecForceDir = vecForce;
	float flMagnitude = VectorNormalize(vecForceDir);
	Vector vecPushBack = GetAbsOrigin() + (vecForceDir * (flMagnitude * 0.1f));

	// Make houndeyes flip all around us!
	CBaseEntity* pEnemySearch[32];
	int nNumEnemies = UTIL_EntitiesInBox(pEnemySearch, ARRAYSIZE(pEnemySearch), vecPushBack - Vector(32, 32, 0), vecPushBack + Vector(32, 32, 64), FL_NPC);
	for (int i = 0; i < nNumEnemies; i++)
	{
		// We only care about antlions
		if (pEnemySearch[i] == NULL || pEnemySearch[i]->Classify() != CLASS_HOUNDEYE || pEnemySearch[i] == this)
			continue;

		CNPC_Houndeye* pHoundeye = dynamic_cast<CNPC_Houndeye*>(pEnemySearch[i]);
		if (pHoundeye != NULL)
		{
			Vector vecDir = (pHoundeye->GetAbsOrigin() - GetAbsOrigin());
			vecDir[2] = 0.0f;
			float flDist = VectorNormalize(vecDir);
			float flFalloff = RemapValClamped(flDist, 0, 256, 1.0f, 0.1f);

			vecDir *= (flMagnitude * flFalloff);
			vecDir[2] += ((flMagnitude * 0.25f) * flFalloff);

			pHoundeye->ApplyAbsVelocityImpulse(vecDir);

			// Turn them over
			pHoundeye->Flip(false);
		}
	}
}

void CNPC_Houndeye::Touch(CBaseEntity* pOther) {
	//See if the touching entity is a vehicle
	CBasePlayer* pPlayer = ToBasePlayer(AI_GetSinglePlayer());

	// FIXME: Technically we'll want to check to see if a vehicle has touched us with the player OR NPC driver

	if (pPlayer && pPlayer->IsInAVehicle())
	{
		IServerVehicle* pVehicle = pPlayer->GetVehicle();
		CBaseEntity* pVehicleEnt = pVehicle->GetVehicleEnt();

		if (pVehicleEnt == pOther)
		{
			CPropVehicleDriveable* pDrivableVehicle = dynamic_cast<CPropVehicleDriveable*>(pVehicleEnt);

			if (pDrivableVehicle != NULL)
			{
				//Get tossed!
				Vector	vecShoveDir = pOther->GetAbsVelocity();
				Vector	vecTargetDir = GetAbsOrigin() - pOther->GetAbsOrigin();

				VectorNormalize(vecShoveDir);
				VectorNormalize(vecTargetDir);

				if (((pDrivableVehicle->m_nRPM > 75) && DotProduct(vecShoveDir, vecTargetDir) <= 0) && !IsFlipped() )
				{
						CTakeDamageInfo	dmgInfo(pVehicleEnt, pPlayer, 0, DMG_VEHICLE);
						PainSound(dmgInfo);

						SetCondition(COND_HOUNDEYE_FLIPPED);

						vecTargetDir[2] = 0.0f;

						ApplyAbsVelocityImpulse((vecTargetDir * 250.0f) + Vector(0, 0, 64.0f));
						SetGroundEntity(NULL);

						CSoundEnt::InsertSound(SOUND_PHYSICS_DANGER, GetAbsOrigin(), 256, 0.5f, this);
					}
				}
			}
		}
}

void CNPC_Houndeye::TraceAttack(const CTakeDamageInfo& info, const Vector& vecDir, trace_t* ptr, CDmgAccumulator* pAccumulator) {
	CTakeDamageInfo newInfo = info;

	Vector	vecShoveDir = vecDir;
	vecShoveDir.z = 0.0f;

	//Are we already flipped?
	if (IsFlipped())
	{
		//If we were hit by physics damage, move with it
		if (newInfo.GetDamageType() & (DMG_CRUSH | DMG_PHYSGUN))
		{
			PainSound(newInfo);
			Vector vecForce = (vecShoveDir * random->RandomInt(500.0f, 1000.0f)) + Vector(0, 0, 64.0f);
			CascadePush(vecForce);
			ApplyAbsVelocityImpulse(vecForce);
			SetGroundEntity(NULL);
		}

		//More vulnerable when flipped
		newInfo.ScaleDamage(4.0f);
	}
	else if (newInfo.GetDamageType() & (DMG_PHYSGUN) ||
		(newInfo.GetDamageType() & (DMG_BLAST | DMG_CRUSH) && newInfo.GetDamage() >= 25.0f))
	{
		// Don't do this if we're in an interaction
		if (!IsRunningDynamicInteraction())
		{
			//Grenades, physcannons, and physics impacts make us fuh-lip!

			if (hl2_episodic.GetBool())
			{
				PainSound(newInfo);

				if (GetFlags() & FL_ONGROUND)
				{
					// Only flip if on the ground.
					SetCondition(COND_HOUNDEYE_FLIPPED);
				}

				Vector vecForce = (vecShoveDir * random->RandomInt(500.0f, 1000.0f)) + Vector(0, 0, 64.0f);

				CascadePush(vecForce);
				ApplyAbsVelocityImpulse(vecForce);
				SetGroundEntity(NULL);
			}
			else
			{
				//Don't flip off the deck
				if (GetFlags() & FL_ONGROUND)
				{
					PainSound(newInfo);

					SetCondition(COND_HOUNDEYE_FLIPPED);

					//Get tossed!
					ApplyAbsVelocityImpulse((vecShoveDir * random->RandomInt(500.0f, 1000.0f)) + Vector(0, 0, 64.0f));
					SetGroundEntity(NULL);
				}
			}
		}
	}

	BaseClass::TraceAttack(newInfo, vecDir, ptr, pAccumulator);
}

//=========================================================
// SonicAttack
//=========================================================
void CNPC_Houndeye::SonicAttack(void)
{

	float		flAdjustedDamage;
	float		flDist;

	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "HoundEye.Sonic");

	CBroadcastRecipientFilter filter2;

	if (m_pSquad)
	{
		switch (m_pSquad->NumMembers()) {
		case 2:
			DispatchParticleEffect("houndeye_shockwave_1", GetAbsOrigin(), GetAbsAngles(), NULL);
			break;
		case 3:
			DispatchParticleEffect("houndeye_shockwave_2", GetAbsOrigin(), GetAbsAngles(), NULL);
			break;
		case 4:
			DispatchParticleEffect("houndeye_shockwave_3", GetAbsOrigin(), GetAbsAngles(), NULL);
			break;
		default:
			if (m_pSquad->NumMembers() > 4) {
				DispatchParticleEffect("houndeye_shockwave_3", GetAbsOrigin(), GetAbsAngles(), NULL);
			}
			else {
				DispatchParticleEffect("houndeye_shockwave_3", GetAbsOrigin(), GetAbsAngles(), NULL);
			}
			break;
		}
	}
	else
	{
		DispatchParticleEffect("houndeye_shockwave_1", GetAbsOrigin(), GetAbsAngles(), NULL);
	}

	CBaseEntity* pEntity = NULL;
	// iterate on all entities in the vicinity.
	while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), HOUNDEYE_MAX_ATTACK_RADIUS)) != NULL)
	{
		if (pEntity->m_takedamage != DAMAGE_NO)
		{
			if (!FClassnameIs(pEntity, "npc_houndeye"))
			{// houndeyes don't hurt other houndeyes with their attack

				// houndeyes do FULL damage if the ent in question is visible. Half damage otherwise.
				// This means that you must get out of the houndeye's attack range entirely to avoid damage.
				// Calculate full damage first

				if (m_pSquad && m_pSquad->NumMembers() > 1)
				{
					// squad gets attack bonus.
					flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat() + sk_houndeye_dmg_blast.GetFloat() * (HOUNDEYE_SQUAD_BONUS * (m_pSquad->NumMembers() - 1));
				}
				else
				{
					// solo
					flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat();
				}

				flDist = (pEntity->WorldSpaceCenter() - GetAbsOrigin()).Length();

				flAdjustedDamage -= (flDist / HOUNDEYE_MAX_ATTACK_RADIUS) * flAdjustedDamage;

				if (!FVisible(pEntity))
				{
					if (pEntity->IsPlayer())
					{
						// if this entity is a client, and is not in full view, inflict half damage. We do this so that players still 
						// take the residual damage if they don't totally leave the houndeye's effective radius. We restrict it to clients
						// so that monsters in other parts of the level don't take the damage and get pissed.
						flAdjustedDamage *= 0.5;
					}
					else if (!FClassnameIs(pEntity, "func_breakable") && !FClassnameIs(pEntity, "func_pushable"))
					{
						// do not hurt nonclients through walls, but allow damage to be done to breakables
						flAdjustedDamage = 0;
					}
				}

				//ALERT ( at_aiconsole, "Damage: %f\n", flAdjustedDamage );

				if (flAdjustedDamage > 0)
				{
					CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SONIC | DMG_ALWAYSGIB);
					CalculateExplosiveDamageForce(&info, (pEntity->GetAbsOrigin() - GetAbsOrigin()), pEntity->GetAbsOrigin());

					pEntity->TakeDamage(info);

					if ((pEntity->GetAbsOrigin() - GetAbsOrigin()).Length2D() <= HOUNDEYE_MAX_ATTACK_RADIUS)
					{
						if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS || (pEntity->VPhysicsGetObject() && !pEntity->IsPlayer()))
						{
							IPhysicsObject* pPhysObject = pEntity->VPhysicsGetObject();

							if (pPhysObject)
							{
								float flMass = pPhysObject->GetMass();

								if (flMass <= HOUNDEYE_TOP_MASS)
								{
									// Increase the vertical lift of the force
									Vector vecForce = info.GetDamageForce();
									vecForce.z *= 2.0f;
									info.SetDamageForce(vecForce);

									pEntity->VPhysicsTakeDamage(info);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool CNPC_Houndeye::ShouldGoToIdleState(void)
{
	if (m_pSquad)
	{
		AISquadIter_t iter;
		for (CAI_BaseNPC* pMember = m_pSquad->GetFirstMember(&iter); pMember; pMember = m_pSquad->GetNextMember(&iter))
		{
			if (pMember != this && pMember->GetHintNode()->HintType() != NO_NODE)
				return true;
		}

		return true;
	}

	return true;
}

bool CNPC_Houndeye::FValidateHintType(CAI_Hint* pHint)
{
	switch (pHint->HintType())
	{
	case HINT_HL1_WORLD_MACHINERY:
		return true;
		break;
	case HINT_HL1_WORLD_BLINKING_LIGHT:
		return true;
		break;
	case HINT_HL1_WORLD_HUMAN_BLOOD:
		return true;
		break;
	case HINT_HL1_WORLD_ALIEN_BLOOD:
		return true;
		break;
	}

	Msg("Couldn't validate hint type");

	return false;
}

//=========================================================
// SetActivity 
//=========================================================
void CNPC_Houndeye::SetActivity(Activity NewActivity)
{
	int	iSequence;

	if (NewActivity == GetActivity())
		return;

	if (m_NPCState == NPC_STATE_COMBAT && NewActivity == ACT_IDLE && random->RandomInt(0, 1))
	{
		// play pissed idle.
		iSequence = LookupSequence("madidle");

		SetActivity(NewActivity); // Go ahead and set this so it doesn't keep trying when the anim is not present

		// In case someone calls this with something other than the ideal activity
		SetIdealActivity(GetActivity());

		// Set to the desired anim, or default anim if the desired is not present
		if (iSequence > ACTIVITY_NOT_AVAILABLE)
		{
			SetSequence(iSequence);	// Set to the reset anim (if it's there)
			SetCycle(0);				// FIX: frame counter shouldn't be reset when its the same activity as before
			ResetSequenceInfo();
		}
	}
	else
	{
		BaseClass::SetActivity(NewActivity);
	}
}

//=========================================================
// start task
//=========================================================
void CNPC_Houndeye::StartTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HOUND_FALL_ASLEEP:
	{
		m_fAsleep = TRUE; // signal that hound is lying down (must stand again before doing anything else!)
		TaskComplete();
		break;
	}
	case TASK_HOUND_WAKE_UP:
	{
		m_fAsleep = FALSE; // signal that hound is standing again
		TaskComplete();
		break;
	}
	case TASK_HOUND_OPEN_EYE:
	{
		m_fDontBlink = FALSE; // turn blinking back on and that code will automatically open the eye
		TaskComplete();
		break;
	}
	case TASK_HOUND_CLOSE_EYE:
	{
		m_nSkin = 0;
		m_fDontBlink = TRUE; // tell blink code to leave the eye alone.
		break;
	}
	case TASK_HOUND_THREAT_DISPLAY:
	{
		SetIdealActivity(ACT_IDLE_ANGRY);
		break;
	}
	case TASK_HOUND_HOP_BACK:
	{
		SetIdealActivity(ACT_LEAP);
		break;
	}
	case TASK_RANGE_ATTACK1:
	{
		SetIdealActivity(ACT_RANGE_ATTACK1);
		break;
	}
	case TASK_SPECIAL_ATTACK1:
	{
		SetIdealActivity(ACT_SPECIAL_ATTACK1);
		break;
	}
	default:
	{
		BaseClass::StartTask(pTask);
		break;
	}
	}
}

//=========================================================
// RunTask 
//=========================================================
void CNPC_Houndeye::RunTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HOUND_THREAT_DISPLAY:
	{
		if (GetEnemy())
		{
			float idealYaw;
			idealYaw = UTIL_VecToYaw(GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			GetMotor()->SetIdealYawAndUpdate(idealYaw);
		}

		if (IsSequenceFinished())
		{
			TaskComplete();
		}

		break;
	}
	case TASK_HOUND_CLOSE_EYE:
	{
		if (m_nSkin < HOUNDEYE_EYE_FRAMES - 1)
			m_nSkin++;
		break;
	}
	case TASK_HOUND_HOP_BACK:
	{
		if (IsSequenceFinished())
		{
			TaskComplete();
		}
		break;
	}
	case TASK_SPECIAL_ATTACK1:
	{
		m_nSkin = random->RandomInt(0, HOUNDEYE_EYE_FRAMES - 1);

		float idealYaw;
		idealYaw = UTIL_VecToYaw(GetNavigator()->GetGoalPos());
		GetMotor()->SetIdealYawAndUpdate(idealYaw);

		float life;
		life = ((255 - GetCycle()) / (m_flPlaybackRate * m_flPlaybackRate));
		if (life < 0.1)
		{
			life = 0.1;
		}

		/*	MessageBegin( MSG_PAS, SVC_TEMPENTITY, GetAbsOrigin() );
				WRITE_BYTE(  TE_IMPLOSION);
				WRITE_COORD( GetAbsOrigin().x);
				WRITE_COORD( GetAbsOrigin().y);
				WRITE_COORD( GetAbsOrigin().z + 16);
				WRITE_BYTE( 50 * life + 100);  // radius
				WRITE_BYTE( pev->frame / 25.0 ); // count
				WRITE_BYTE( life * 10 ); // life
			MessageEnd();*/

		if (IsSequenceFinished())
		{
			SonicAttack();
			TaskComplete();
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

//=========================================================
// PrescheduleThink
//=========================================================
void CNPC_Houndeye::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	// if the hound is mad and is running, make hunt noises.
	if (m_NPCState == NPC_STATE_COMBAT && GetActivity() == ACT_RUN && random->RandomFloat(0, 1) < 0.2)
	{
		WarnSound();
	}

	// at random, initiate a blink if not already blinking or sleeping
	if (!m_fDontBlink)
	{
		if ((m_nSkin == 0) && random->RandomInt(0, 0x7F) == 0)
		{// start blinking!
			m_nSkin = HOUNDEYE_EYE_FRAMES - 1;
		}
		else if (m_nSkin != 0)
		{// already blinking
			m_nSkin--;
		}
	}

	// if you are the leader, average the origins of each pack member to get an approximate center.
	if (m_pSquad && m_pSquad->IsLeader(this))
	{
		int iSquadCount = 0;

		AISquadIter_t iter;
		for (CAI_BaseNPC* pSquadMember = m_pSquad->GetFirstMember(&iter); pSquadMember; pSquadMember = m_pSquad->GetNextMember(&iter))
		{
			iSquadCount++;
			m_vecPackCenter = m_vecPackCenter + pSquadMember->GetAbsOrigin();
		}

		m_vecPackCenter = m_vecPackCenter / iSquadCount;
	}
}

//=========================================================
// GetScheduleOfType 
//=========================================================
int CNPC_Houndeye::TranslateSchedule(int scheduleType)
{
	if (m_fAsleep)
	{
		// if the hound is sleeping, must wake and stand!
		if (HasCondition(COND_HEAR_DANGER) || HasCondition(COND_HEAR_THUMPER) || HasCondition(COND_HEAR_COMBAT) ||
			HasCondition(COND_HEAR_WORLD) || HasCondition(COND_HEAR_PLAYER) || HasCondition(COND_HEAR_BULLET_IMPACT))
		{
			CSound* pWakeSound;

			pWakeSound = GetBestSound();
			ASSERT(pWakeSound != NULL);
			if (pWakeSound)
			{
				GetMotor()->SetIdealYawToTarget(pWakeSound->GetSoundOrigin());

				if (FLSoundVolume(pWakeSound) >= HOUNDEYE_SOUND_STARTLE_VOLUME)
				{
					// awakened by a loud sound
					return SCHED_HOUND_WAKE_URGENT;
				}
			}
			// sound was not loud enough to scare the bejesus out of houndeye
			return SCHED_HOUND_WAKE_LAZY;
		}
		else if (HasCondition(COND_NEW_ENEMY))
		{
			// get up fast, to fight.
			return SCHED_HOUND_WAKE_URGENT;
		}

		else
		{
			// hound is waking up on its own
			return SCHED_HOUND_SLEEP;
		}
	}
	switch (scheduleType)
	{
	case SCHED_CHASE_ENEMY:
	{
		return SCHED_HOUND_CHASE_ENEMY;
		break;
	}
	case SCHED_IDLE_STAND:
	{
		// we may want to sleep instead of stand!
		if (m_pSquad && !m_pSquad->IsLeader(this) && !m_fAsleep && random->RandomInt(0, 29) < 1)
		{
			return SCHED_HOUND_SLEEP;
		}
		else
		{
			return BaseClass::TranslateSchedule(scheduleType);
		}
	}

	case SCHED_RANGE_ATTACK1:
		return SCHED_HOUND_RANGEATTACK;
	case SCHED_SPECIAL_ATTACK1:
		return SCHED_HOUND_SPECIALATTACK;

	case SCHED_FAIL:
	{
		if (m_NPCState == NPC_STATE_COMBAT)
		{
			if (!FNullEnt(UTIL_FindClientInPVS(edict())))
			{
				// client in PVS
				return SCHED_HOUND_COMBAT_FAIL_PVS;
			}
			else
			{
				// client has taken off! 
				return SCHED_HOUND_COMBAT_FAIL_NOPVS;
			}
		}
		else
		{
			return BaseClass::TranslateSchedule(scheduleType);
		}
	}
	default:
	{
		return BaseClass::TranslateSchedule(scheduleType);
	}
	}
}

int CNPC_Houndeye::SelectSchedule(void)
{
	if (HasCondition(COND_HOUNDEYE_FLIPPED))
	{
		ClearCondition(COND_HOUNDEYE_FLIPPED);
		return SCHED_HOUND_FLIP;
	}

	switch (m_NPCState)
	{
	case NPC_STATE_IDLE:
	{
		if (m_bShouldPatrol)
			return SCHED_PATROL_WALK;
		else
			break;
	}
	case NPC_STATE_COMBAT:
	{
		// dead enemy
		if (HasCondition(COND_ENEMY_DEAD))
		{
			// call base class, all code to handle dead enemies is centralized there.
			return BaseClass::SelectSchedule();
		}

		if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE))
		{
			if (random->RandomFloat(0, 1) <= 0.4)
			{
				trace_t trace;
				Vector v_forward;
				GetVectors(&v_forward, NULL, NULL);
				UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin() + v_forward * -128, MASK_SOLID, &trace);

				if (trace.fraction == 1.0)
				{
					// it's clear behind, so the hound will jump
					return SCHED_HOUND_HOP_RETREAT;
				}
			}

			return SCHED_TAKE_COVER_FROM_ENEMY;
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			if (OccupyStrategySlot(SQUAD_SLOTS_HOUND_ATTACK) || OccupyStrategySlot(0) || OccupyStrategySlot(GetSquad()->NumMembers()/2))
			{
				return SCHED_RANGE_ATTACK1;
			}

			return SCHED_HOUND_AGITATED;

		}
		return SCHED_CHASE_ENEMY;

		break;
	}
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Houndeye::InputStartPatrolling(inputdata_t& inputdata)
{
	m_bShouldPatrol = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Houndeye::InputStopPatrolling(inputdata_t& inputdata)
{
	m_bShouldPatrol = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Houndeye::InputStartSleep(inputdata_t& inputdata)
{
	m_fAsleep = true;
	SetSchedule(SCHED_HOUND_SLEEP);
}


//=========================================================
// FLSoundVolume - subtracts the volume of the given sound
// from the distance the sound source is from the caller, 
// and returns that value, which is considered to be the 'local' 
// volume of the sound. 
//=========================================================
float CNPC_Houndeye::FLSoundVolume(CSound* pSound)
{
	return (pSound->Volume() - ((pSound->GetSoundOrigin() - GetAbsOrigin()).Length()));
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC(npc_houndeye, CNPC_Houndeye)

DECLARE_TASK(TASK_HOUND_CLOSE_EYE)
DECLARE_TASK(TASK_HOUND_OPEN_EYE)
DECLARE_TASK(TASK_HOUND_THREAT_DISPLAY)
DECLARE_TASK(TASK_HOUND_FALL_ASLEEP)
DECLARE_TASK(TASK_HOUND_WAKE_UP)
DECLARE_TASK(TASK_HOUND_HOP_BACK)

DECLARE_CONDITION(COND_HOUNDEYE_FLIPPED)

DECLARE_ACTIVITY(ACT_HOUNDEYE_FLIP)

//=========================================================
// > SCHED_HOUND_AGITATED
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_AGITATED,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_HOUND_THREAT_DISPLAY	0"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HEAVY_DAMAGE"
)

//=========================================================
// > SCHED_HOUND_HOP_RETREAT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_HOP_RETREAT,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_HOUND_HOP_BACK			0"
	"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_HOUND_YELL1
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_YELL1,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_IDEAL				0"
	"		TASK_RANGE_ATTACK1			0"
	"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_HOUND_AGITATED"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_HOUND_YELL2
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_YELL2,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_IDEAL				0"
	"		TASK_RANGE_ATTACK1			0"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_HOUND_RANGEATTACK
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_RANGEATTACK,

	"	Tasks"
	"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_HOUND_YELL1"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
)

//=========================================================
// > SCHED_HOUND_SLEEP
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_SLEEP,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
	"		TASK_WAIT_RANDOM			5"
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_CROUCH"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_CROUCHIDLE"
	"		TASK_HOUND_FALL_ASLEEP		0"
	"		TASK_WAIT_RANDOM			25"
	"	TASK_HOUND_CLOSE_EYE		0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NEW_ENEMY"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_PLAYER"
	"		COND_HEAR_WORLD"
)

//=========================================================
// > SCHED_HOUND_WAKE_LAZY
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_WAKE_LAZY,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_HOUND_OPEN_EYE			0"
	"		TASK_WAIT_RANDOM			2.5"
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_STAND"
	"		TASK_HOUND_WAKE_UP			0"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_HOUND_WAKE_URGENT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_WAKE_URGENT,

	"	Tasks"
	"		TASK_HOUND_OPEN_EYE			0"
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_HOP"
	"		TASK_FACE_IDEAL				0"
	"		TASK_HOUND_WAKE_UP			0"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_HOUND_SPECIALATTACK
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_SPECIALATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_IDEAL				0"
	"		TASK_SPECIAL_ATTACK1		0"
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_IDLE_ANGRY"
	"	"
	"	Interrupts"
	"		"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_OCCLUDED"
)

//=========================================================
// > SCHED_HOUND_COMBAT_FAIL_PVS
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_COMBAT_FAIL_PVS,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_HOUND_THREAT_DISPLAY	0"
	"		TASK_WAIT_FACE_ENEMY		1"
	"	"
	"	Interrupts"
	"		"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
)

//=========================================================
// > SCHED_HOUND_COMBAT_FAIL_NOPVS
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_COMBAT_FAIL_NOPVS,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_HOUND_THREAT_DISPLAY	0"
	"		TASK_WAIT_FACE_ENEMY		1"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
	"		TASK_WAIT_PVS				0"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
)
//=========================================================
// > SCHED_HOUND_CHASE_ENEMY
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HOUND_CHASE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_RANGE_ATTACK1"
	"		TASK_GET_PATH_TO_ENEMY			0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_SMELL"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_FLIP,

	"	Tasks"
	"		TASK_STOP_MOVING	0"
	"		TASK_RESET_ACTIVITY		0"
	"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_HOUNDEYE_FLIP"

	"	Interrupts"
	"		COND_TASK_FAILED"
)

AI_END_CUSTOM_NPC()
