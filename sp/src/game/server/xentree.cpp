//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "AI_BaseNPC.h"
#include "trains.h"
#include "ndebugoverlay.h"
#include "EntityList.h"
#include "engine/IEngineSound.h"
#include "doors.h"
#include "soundent.h"
#include "shake.h"
#include "globalstate.h"
#include "soundscape.h"
#include "buttons.h"
#include "sprite.h"
#include "actanimating.h"
#include "npcevent.h"
#include "func_break.h"
#include "eventqueue.h"
#include "ai_baseactor.h"
#include "ai_behavior.h"
#include "baseentity.h"
#include "util.h"
#include <time.h> 


class CXenTreeTrigger : public CBaseEntity
{
	DECLARE_CLASS(CXenTreeTrigger, CBaseEntity);
public:
	void		Touch(CBaseEntity* pOther);
	static CXenTreeTrigger* TriggerCreate(CBaseEntity* pOwner, const Vector& position);
};
LINK_ENTITY_TO_CLASS(xen_ttrigger, CXenTreeTrigger);

CXenTreeTrigger* CXenTreeTrigger::TriggerCreate(CBaseEntity* pOwner, const Vector& position)
{
	CXenTreeTrigger* pTrigger = CREATE_ENTITY(CXenTreeTrigger, "xen_ttrigger");
	pTrigger->SetAbsOrigin(position);

	pTrigger->SetSolid(SOLID_BBOX);
	pTrigger->AddSolidFlags(FSOLID_TRIGGER | FSOLID_NOT_SOLID);
	pTrigger->SetMoveType(MOVETYPE_NONE);
	pTrigger->SetOwnerEntity(pOwner);

	return pTrigger;
}


void CXenTreeTrigger::Touch(CBaseEntity* pOther)
{
	if (GetOwnerEntity())
	{
		GetOwnerEntity()->Touch(pOther);
	}
}

#define TREE_AE_ATTACK		1

class CXenTree : public CAI_BaseActor
{
	DECLARE_CLASS(CXenTree, CAI_BaseActor);
public:
	void		Spawn(void);
	void		Precache(void);
	void		Touch(CBaseEntity* pOther);
	void		Think(void);
	void		Whack(void);
	void SetActivity(Activity NewActivity);
	int			OnTakeDamage(const CTakeDamageInfo& info) { Attack(); return 0; }
	float lastSwing = 0.0f;
	void		Attack(void);
	Class_T			Classify(void) { return CLASS_EARTH_FAUNA; }

	DECLARE_DATADESC();

private:
	CXenTreeTrigger* m_pTrigger;
};

LINK_ENTITY_TO_CLASS(xen_tree, CXenTree);

BEGIN_DATADESC(CXenTree)
DEFINE_FIELD(m_pTrigger, FIELD_CLASSPTR),
END_DATADESC()

void CXenTree::Spawn(void)
{
	Precache();

	SetModel("models/model_xen/xen_tree.mdl");
	SetMoveType(MOVETYPE_NONE);
	SetSolid(SOLID_BBOX);
	//SetBloodColor(BLOOD_COLOR_ANTLION);

	m_takedamage = DAMAGE_YES;

	UTIL_SetSize(this, Vector(-30, -30, 0), Vector(30, 30, 188));
	SetActivity(ACT_IDLE);
	SetNextThink(gpGlobals->curtime + 0.1);
	SetCycle(random->RandomFloat(0, 1));
	m_flPlaybackRate = random->RandomFloat(0.7, 1.4);

	Vector triggerPosition, vForward;

	AngleVectors(GetAbsAngles(), &vForward);
	triggerPosition = GetAbsOrigin() + (vForward * 64);

	// Create the trigger
	m_pTrigger = CXenTreeTrigger::TriggerCreate(this, triggerPosition);
	UTIL_SetSize(m_pTrigger, Vector(-60, -60, 0), Vector(60, 60, 128));
}

void CXenTree::Precache(void)
{
	PrecacheModel("models/model_xen/xen_tree.mdl");

	PrecacheScriptSound("XenTree.AttackMiss");
	PrecacheScriptSound("XenTree.AttackHit");

	SetActivity(ACT_IDLE);
}


void CXenTree::Touch(CBaseEntity* pOther)
{
	if (!pOther->IsAlive())
		return;

	Attack();
}


void CXenTree::Attack(void)
{
	if (GetActivity() == ACT_IDLE)
	{
		lastSwing = gpGlobals->curtime+0.7;
		int num = RandomInt(4, 4);
		switch (num)
		{
			case 1:
				SetActivity(ACT_MELEE_ATTACK1);
				lastSwing = gpGlobals->curtime + 0.7;
				break;
			case 2:
				SetActivity(ACT_MELEE_ATTACK2);
				lastSwing = gpGlobals->curtime + 0.9;
				break;
			case 3:
				SetActivity(ACT_MELEE_ATTACK_MISS1);
				lastSwing = gpGlobals->curtime + 0.7;
				break;
			case 4:
				SetActivity(ACT_MELEE_ATTACK_MISS2);
				lastSwing = gpGlobals->curtime + 0.7;
				break;
		}
		m_flPlaybackRate = random->RandomFloat(0.9, 1.1);

		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "XenTree.AttackMiss");
	}
}

void CXenTree::Think(void)
{
	StudioFrameAdvance();
	SetNextThink(gpGlobals->curtime + 0.1);
	DispatchAnimEvents(this);

	//Hurt Targets
	if (gpGlobals->curtime > lastSwing && lastSwing != 0.0f)
	{
		Msg("%fLastSwing\n", lastSwing);
		lastSwing = 0.0f;
		Whack();
	}

	//Return to idle
	switch (GetActivity())
	{
	case ACT_MELEE_ATTACK1:
		if (IsSequenceFinished())
		{
			SetActivity(ACT_IDLE);
			m_flPlaybackRate = random->RandomFloat(0.6f, 1.4f);
		}
		break;
	case ACT_MELEE_ATTACK2:
		if (IsSequenceFinished())
		{
			SetActivity(ACT_IDLE);
			m_flPlaybackRate = random->RandomFloat(0.6f, 1.4f);
		}
		break;
	case ACT_MELEE_ATTACK_MISS1:
		if (IsSequenceFinished())
		{
			SetActivity(ACT_IDLE);
			m_flPlaybackRate = random->RandomFloat(0.6f, 1.4f);
		}
		break;
	case ACT_MELEE_ATTACK_MISS2:
		if (IsSequenceFinished())
		{
			SetActivity(ACT_IDLE);
			m_flPlaybackRate = random->RandomFloat(0.6f, 1.4f);
		}
		break;

	default:
	case ACT_IDLE:
		break;

	}
}

void CXenTree::SetActivity(Activity NewActivity)
{
	BaseClass::SetActivity(NewActivity);
}

void CXenTree::Whack()
{
	CBaseEntity* pList[8];
	BOOL sound = FALSE;
	int count = UTIL_EntitiesInBox(pList, 8, m_pTrigger->GetAbsOrigin() + m_pTrigger->WorldAlignMins(), m_pTrigger->GetAbsOrigin() + m_pTrigger->WorldAlignMaxs(), FL_NPC | FL_CLIENT);

	Vector forward;
	AngleVectors(GetAbsAngles(), &forward);

	for (int i = 0; i < count; i++)
	{
		Msg("%s\n", pList[i]->GetEntityName());
		if (pList[i] != this)
		{
			if (pList[i]->GetOwnerEntity() != this)
			{
				sound = true;
				pList[i]->TakeDamage(CTakeDamageInfo(this, this, 25, DMG_CRUSH | DMG_SLASH));
				pList[i]->ViewPunch(QAngle(15, 0, 18));

				pList[i]->SetAbsVelocity(pList[i]->GetAbsVelocity() + forward * 100);
			}
		}
	}

	if (sound)
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "XenTree.AttackHit");
	}
}