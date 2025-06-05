//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npc_android.h"
#include "android_spawner.h"
#include "saverestore_utlvector.h"
#include "mapentities.h"
#include "decals.h"
#include "iservervehicle.h"
#include "antlion_dust.h"
#include "smoke_trail.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CAndroidMakerManager g_AndroidnMakerManager("CAndroidMakerManager");

static const char* s_pPoolThinkContext = "PoolThinkContext";
static const char* s_pBlockedEffectsThinkContext = "BlockedEffectsThinkContext";
static const char* s_pBlockedCheckContext = "BlockedCheckContext";


#define ANDROID_MAKER_PLAYER_DETECT_RADIUS	512
#define ANDROID_MAKER_BLOCKED_MASS			250.0f		// half the weight of a car

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vFightGoal - 
//-----------------------------------------------------------------------------
void CAndroidMakerManager::BroadcastFightGoal(const Vector& vFightGoal)
{
	CAndroidTemplateMaker* pMaker;

	for (int i = 0; i < m_Makers.Count(); i++)
	{
		pMaker = m_Makers[i];

		if (pMaker)
		{
			pMaker->SetFightTarget(vFightGoal);
			pMaker->UpdateChildren();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFightGoal - 
//-----------------------------------------------------------------------------
void CAndroidMakerManager::BroadcastFightGoal(CBaseEntity* pFightGoal)
{
	CAndroidTemplateMaker* pMaker;

	for (int i = 0; i < m_Makers.Count(); i++)
	{
		pMaker = m_Makers[i];

		if (pMaker)
		{
			pMaker->SetFightTarget(pFightGoal);
			pMaker->UpdateChildren();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFightGoal - 
//-----------------------------------------------------------------------------
void CAndroidMakerManager::BroadcastFollowGoal(CBaseEntity* pFollowGoal)
{
	CAndroidTemplateMaker* pMaker;

	for (int i = 0; i < m_Makers.Count(); i++)
	{
		pMaker = m_Makers[i];

		if (pMaker)
		{
			pMaker->SetFollowTarget(pFollowGoal);
			pMaker->UpdateChildren();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidMakerManager::GatherMakers(void)
{
	CBaseEntity* pSearch = NULL;
	CAndroidTemplateMaker* pMaker;

	m_Makers.Purge();

	// Find these all once
	while ((pSearch = gEntList.FindEntityByClassname(pSearch, "npc_android_template_maker")) != NULL)
	{
		pMaker = static_cast<CAndroidTemplateMaker*>(pSearch);

		m_Makers.AddToTail(pMaker);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidMakerManager::LevelInitPostEntity(void)
{
	//Find all makers
	GatherMakers();
}

LINK_ENTITY_TO_CLASS(npc_android_template_maker, CAndroidTemplateMaker);

//DT Definition
BEGIN_DATADESC(CAndroidTemplateMaker)

DEFINE_KEYFIELD(m_strSpawnGroup, FIELD_STRING, "spawngroup"),
DEFINE_KEYFIELD(m_strSpawnTarget, FIELD_STRING, "spawntarget"),
DEFINE_KEYFIELD(m_flSpawnRadius, FIELD_FLOAT, "spawnradius"),
DEFINE_KEYFIELD(m_strFightTarget, FIELD_STRING, "fighttarget"),
DEFINE_KEYFIELD(m_strFollowTarget, FIELD_STRING, "followtarget"),
DEFINE_KEYFIELD(m_flVehicleSpawnDistance, FIELD_FLOAT, "vehicledistance"),

DEFINE_FIELD(m_hFightTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_hProxyTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_hFollowTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_iSkinCount, FIELD_INTEGER),
DEFINE_FIELD(m_flBlockedBumpTime, FIELD_TIME),
DEFINE_FIELD(m_bBlocked, FIELD_BOOLEAN),

DEFINE_UTLVECTOR(m_Children, FIELD_EHANDLE),

DEFINE_KEYFIELD(m_iPool, FIELD_INTEGER, "pool_start"),
DEFINE_KEYFIELD(m_iMaxPool, FIELD_INTEGER, "pool_max"),
DEFINE_KEYFIELD(m_iPoolRegenAmount, FIELD_INTEGER, "pool_regen_amount"),
DEFINE_KEYFIELD(m_flPoolRegenTime, FIELD_FLOAT, "pool_regen_time"),

DEFINE_INPUTFUNC(FIELD_STRING, "SetFightTarget", InputSetFightTarget),
DEFINE_INPUTFUNC(FIELD_STRING, "SetFollowTarget", InputSetFollowTarget),
DEFINE_INPUTFUNC(FIELD_VOID, "ClearFollowTarget", InputClearFollowTarget),
DEFINE_INPUTFUNC(FIELD_VOID, "ClearFightTarget", InputClearFightTarget),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetSpawnRadius", InputSetSpawnRadius),
DEFINE_INPUTFUNC(FIELD_INTEGER, "AddToPool", InputAddToPool),
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetMaxPool", InputSetMaxPool),
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetPoolRegenAmount", InputSetPoolRegenAmount),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetPoolRegenTime", InputSetPoolRegenTime),
DEFINE_INPUTFUNC(FIELD_STRING, "ChangeDestinationGroup", InputChangeDestinationGroup),
DEFINE_OUTPUT(m_OnAllBlocked, "OnAllBlocked"),

DEFINE_THINKFUNC(PoolRegenThink),
DEFINE_THINKFUNC(FindNodesCloseToPlayer),
DEFINE_THINKFUNC(BlockedCheckFunc),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAndroidTemplateMaker::CAndroidTemplateMaker(void)
{
	m_hFightTarget = NULL;
	m_hProxyTarget = NULL;
	m_hFollowTarget = NULL;
	m_iSkinCount = 0;
	m_flBlockedBumpTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAndroidTemplateMaker::~CAndroidTemplateMaker(void)
{
	DestroyProxyTarget();
	m_Children.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pAnt - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::AddChild(CNPC_Android* pAnt)
{
	m_Children.AddToTail(pAnt);
	m_nLiveChildren = m_Children.Count();

	pAnt->SetOwnerEntity(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pAnt - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::RemoveChild(CNPC_Android* pAnt)
{
	m_Children.FindAndRemove(pAnt);
	m_nLiveChildren = m_Children.Count();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::PrecacheTemplateEntity(CBaseEntity* pEntity)
{
	BaseClass::PrecacheTemplateEntity(pEntity);

	pEntity->Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::Activate(void)
{
	BaseClass::Activate();

	// Are we using the pool behavior for coast?
	if (m_iMaxPool)
	{
		if (!m_flPoolRegenTime)
		{
			Msg("%s using pool behavior without a specified pool regen time.\n", GetClassname());
			m_flPoolRegenTime = 0.1;
		}

		// Start up our think cycle unless we're reloading this map (which would reset it)
		if (m_bDisabled == false && gpGlobals->eLoadType != MapLoad_LoadGame)
		{
			// Start our pool regeneration cycle
			SetContextThink(&CAndroidTemplateMaker::PoolRegenThink, gpGlobals->curtime + m_flPoolRegenTime, s_pPoolThinkContext);
			SetContextThink(&CAndroidTemplateMaker::FindNodesCloseToPlayer, gpGlobals->curtime + 1.0f, s_pBlockedEffectsThinkContext);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity* CAndroidTemplateMaker::GetFightTarget(void)
{
	if (m_hFightTarget != NULL)
		return m_hFightTarget;

	return m_hProxyTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity* CAndroidTemplateMaker::GetFollowTarget(void)
{
	return m_hFollowTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::UpdateChildren(void)
{
	//Update all children
	CNPC_Android* pAndroid = NULL;

	// Move through our child list
	int i = 0;
	for (; i < m_Children.Count(); i++)
	{
		pAndroid = m_Children[i];

		//HACKHACK
		//Let's just fix this up.
		//This guy might have been killed in another level and we just came back.
		if (pAndroid == NULL)
		{
			m_Children.Remove(i);
			i--;
			continue;
		}

		if (pAndroid->m_lifeState != LIFE_ALIVE)
			continue;
	}

	m_nLiveChildren = i;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : strTarget - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::SetFightTarget(string_t strTarget, CBaseEntity* pActivator, CBaseEntity* pCaller)
{
	SetFightTarget(gEntList.FindEntityByName(NULL, strTarget, this, pActivator, pCaller));
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::SetFightTarget(CBaseEntity* pEntity)
{
	m_hFightTarget = pEntity;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &position - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::SetFightTarget(const Vector& position)
{
	CreateProxyTarget(position);

	m_hFightTarget = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::SetFollowTarget(CBaseEntity* pTarget)
{
	m_hFollowTarget = pTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::SetFollowTarget(string_t strTarget, CBaseEntity* pActivator, CBaseEntity* pCaller)
{
	CBaseEntity* pSearch = gEntList.FindEntityByName(NULL, strTarget, NULL, pActivator, pCaller);

	if (pSearch != NULL)
	{
		SetFollowTarget(pSearch);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &position - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::CreateProxyTarget(const Vector& position)
{
	// Create if we don't have one
	if (m_hProxyTarget == NULL)
	{
		m_hProxyTarget = CreateEntityByName("info_target");
	}

	// Update if we do
	if (m_hProxyTarget != NULL)
	{
		m_hProxyTarget->SetAbsOrigin(position);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::DestroyProxyTarget(void)
{
	if (m_hProxyTarget)
	{
		UTIL_Remove(m_hProxyTarget);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bIgnoreSolidEntities - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAndroidTemplateMaker::CanMakeNPC(bool bIgnoreSolidEntities)
{
	if (m_nMaxLiveChildren == 0)
		return false;

	if (m_strSpawnGroup == NULL_STRING)
		return BaseClass::CanMakeNPC(bIgnoreSolidEntities);

	if (m_nMaxLiveChildren > 0 && m_nLiveChildren >= m_nMaxLiveChildren)
		return false;

	if (m_iMaxPool && !m_iPool)
		return false;

	if ((CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI) == bits_debugDisableAI)
		return false;

	return true;
}

void CAndroidTemplateMaker::Enable(void)
{
	BaseClass::Enable();

	if (m_iMaxPool)
	{
		SetContextThink(&CAndroidTemplateMaker::PoolRegenThink, gpGlobals->curtime + m_flPoolRegenTime, s_pPoolThinkContext);
		SetContextThink(&CAndroidTemplateMaker::FindNodesCloseToPlayer, gpGlobals->curtime + 1.0f, s_pBlockedEffectsThinkContext);
	}
}

void CAndroidTemplateMaker::Disable(void)
{
	BaseClass::Disable();

	SetContextThink(NULL, gpGlobals->curtime, s_pPoolThinkContext);
	SetContextThink(NULL, gpGlobals->curtime, s_pBlockedEffectsThinkContext);
}

void CAndroidTemplateMaker::ChildPreSpawn(CAI_BaseNPC* pChild)
{
	BaseClass::ChildPreSpawn(pChild);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::MakeNPC(void)
{
	if (m_strSpawnGroup == NULL_STRING)
	{
		BaseClass::MakeNPC();
		return;
	}

	if (CanMakeNPC(true) == false)
		return;

	// Set our defaults
	Vector	targetOrigin = GetAbsOrigin();
	QAngle	targetAngles = GetAbsAngles();

	// Look for our target entity
	CBaseEntity* pTarget = gEntList.FindEntityByName(NULL, m_strSpawnTarget, this);

	// Take its position if it exists
	if (pTarget != NULL)
	{
		UTIL_PredictedPosition(pTarget, 1.5f, &targetOrigin);
	}

	Vector	spawnOrigin = vec3_origin;

	CAI_Hint* pNode = NULL;

	if (FindNearTargetSpawnPosition(spawnOrigin, m_flSpawnRadius, pTarget) == false)
		return;
	else
	{
		// If we can't find a spawn position, then we can't spawn this time
		if (FindHintSpawnPosition(targetOrigin, m_flSpawnRadius, m_strSpawnGroup, &pNode, true) == false)
			return;

		pNode->GetPosition(HULL_MEDIUM, &spawnOrigin);
	}

	// Point at the current position of the enemy
	if (pTarget != NULL)
	{
		targetOrigin = pTarget->GetAbsOrigin();
	}

	// Create the entity via a template
	CAI_BaseNPC* pent = NULL;
	CBaseEntity* pEntity = NULL;
	MapEntity_ParseEntity(pEntity, STRING(m_iszTemplateData), NULL);

	if (pEntity != NULL)
	{
		pent = (CAI_BaseNPC*)pEntity;
	}

	if (pent == NULL)
	{
		Warning("NULL Ent in NPCMaker!\n");
		return;
	}

	// Lock this hint node
	pNode->Lock(pEntity);

	// reject this point as a spawn point to spread them out a bit
	pNode->Unlock(2.0f);

	m_OnSpawnNPC.Set(pEntity, pEntity, this);

	pent->AddSpawnFlags(SF_NPC_FALL_TO_GROUND);

	ChildPreSpawn(pent);

	// Put us at the desired location
	pent->SetLocalOrigin(spawnOrigin);

	QAngle	spawnAngles;

	if (pTarget)
	{
		// Face our spawning direction
		Vector	spawnDir = (targetOrigin - spawnOrigin);
		VectorNormalize(spawnDir);

		VectorAngles(spawnDir, spawnAngles);
		spawnAngles[PITCH] = 0.0f;
		spawnAngles[ROLL] = 0.0f;
	}
	else if (pNode)
	{
		spawnAngles = QAngle(0, pNode->Yaw(), 0);
	}

	pent->SetLocalAngles(spawnAngles);
	DispatchSpawn(pent);

	pent->Activate();

	pent->m_nSkin = m_iSkinCount;

	ChildPostSpawn(pent);

	// Hold onto the child
	CNPC_Android* pAndroid = dynamic_cast<CNPC_Android*>(pent);

	AddChild(pAndroid);

	m_bBlocked = false;
	SetContextThink(NULL, -1, s_pBlockedCheckContext);

	if (!(m_spawnflags & SF_NPCMAKER_INF_CHILD))
	{
		if (m_iMaxPool)
		{
			m_iPool--;
		}
		else
		{
			m_nMaxNumNPCs--;
		}

		if (IsDepleted())
		{
			m_OnAllSpawned.FireOutput(this, this);

			// Disable this forever.  Don't kill it because it still gets death notices
			SetThink(NULL);
			SetUse(NULL);
		}
	}
}

bool CAndroidTemplateMaker::FindPositionOnFoot(Vector& origin, float radius, CBaseEntity* pTarget)
{
	int iMaxTries = 10;
	Vector vSpawnOrigin = pTarget->GetAbsOrigin();

	while (iMaxTries > 0)
	{
		vSpawnOrigin.x += random->RandomFloat(-radius, radius);
		vSpawnOrigin.y += random->RandomFloat(-radius, radius);
		vSpawnOrigin.z += 96;

		if (ValidateSpawnPosition(vSpawnOrigin, pTarget) == false)
		{
			iMaxTries--;
			continue;
		}

		origin = vSpawnOrigin;
		return true;
	}

	return false;
}

bool CAndroidTemplateMaker::ValidateSpawnPosition(Vector& vOrigin, CBaseEntity* pTarget)
{
	trace_t	tr;
	UTIL_TraceLine(vOrigin, vOrigin - Vector(0, 0, 1024), MASK_BLOCKLOS | CONTENTS_WATER, NULL, COLLISION_GROUP_NONE, &tr);

	// Make sure this point is clear 
	if (tr.fraction != 1.0)
	{
		if (tr.contents & (CONTENTS_WATER))
			return false;

		const surfacedata_t* psurf = physprops->GetSurfaceData(tr.surface.surfaceProps);

		if (psurf)
		{
			if (psurf->game.material != CHAR_TEX_SAND)
				return false;
		}

		trace_t trCheck;
		UTIL_TraceHull(tr.endpos, tr.endpos + Vector(0, 0, 5), NAI_Hull::Mins(HULL_MEDIUM), NAI_Hull::Maxs(HULL_MEDIUM), MASK_NPCSOLID, NULL, COLLISION_GROUP_NONE, &trCheck);

		if (trCheck.DidHit() == false)
		{
			if (pTarget)
			{
				if (pTarget->IsPlayer())
				{
					CBaseEntity* pVehicle = NULL;
					CBasePlayer* pPlayer = dynamic_cast <CBasePlayer*> (pTarget);

					if (pPlayer && pPlayer->GetVehicle())
						pVehicle = ((CBasePlayer*)pTarget)->GetVehicle()->GetVehicleEnt();

					CTraceFilterSkipTwoEntities traceFilter(pPlayer, pVehicle, COLLISION_GROUP_NONE);

					trace_t trVerify;

					Vector vVerifyOrigin = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
					float flZOffset = NAI_Hull::Maxs(HULL_MEDIUM).z;
					UTIL_TraceLine(vVerifyOrigin, tr.endpos + Vector(0, 0, flZOffset), MASK_BLOCKLOS | CONTENTS_WATER, &traceFilter, &trVerify);

					if (trVerify.fraction != 1.0f)
					{
						const surfacedata_t* psurf = physprops->GetSurfaceData(trVerify.surface.surfaceProps);

						if (psurf)
						{
							if (psurf->game.material == CHAR_TEX_DIRT)
							{

								return false;
							}
						}
					}
				}
			}


			vOrigin = trCheck.endpos + Vector(0, 0, 5);
			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}

bool CAndroidTemplateMaker::FindNearTargetSpawnPosition(Vector& origin, float radius, CBaseEntity* pTarget)
{
	if (pTarget)
	{
		CBaseEntity* pVehicle = NULL;

		if (pTarget->IsPlayer())
		{
			CBasePlayer* pPlayer = ((CBasePlayer*)pTarget);

			if (pPlayer->GetVehicle())
				pVehicle = ((CBasePlayer*)pTarget)->GetVehicle()->GetVehicleEnt();
		}

		return FindPositionOnFoot(origin, radius, pTarget);
	}

	return false;
}

bool CAndroidTemplateMaker::FindHintSpawnPosition(const Vector& origin, float radius, string_t hintGroupName, CAI_Hint** pHint, bool bRandom)
{
	CAI_Hint* pChosenHint = NULL;

	CHintCriteria	hintCriteria;

	hintCriteria.SetGroup(hintGroupName);

	if (bRandom)
	{
		hintCriteria.SetFlag(bits_HINT_NODE_RANDOM);
	}
	else
	{
		hintCriteria.SetFlag(bits_HINT_NODE_NEAREST);
	}

	// If requested, deny nodes that can be seen by the player
	if (m_spawnflags & SF_NPCMAKER_HIDEFROMPLAYER)
	{
		hintCriteria.SetFlag(bits_HINT_NODE_NOT_VISIBLE_TO_PLAYER);
	}

	hintCriteria.AddIncludePosition(origin, radius);

	if (bRandom == true)
	{
		pChosenHint = CAI_HintManager::FindHintRandom(NULL, origin, hintCriteria);
	}
	else
	{
		pChosenHint = CAI_HintManager::FindHint(origin, hintCriteria);
	}

	if (pChosenHint != NULL)
	{
		bool bChosenHintBlocked = false;

		if (AllHintsFromClusterBlocked(pChosenHint, bChosenHintBlocked))
		{
			if ((GetIndexForThinkContext(s_pBlockedCheckContext) == NO_THINK_CONTEXT) ||
				(GetNextThinkTick(s_pBlockedCheckContext) == TICK_NEVER_THINK))
			{
				SetContextThink(&CAndroidTemplateMaker::BlockedCheckFunc, gpGlobals->curtime + 2.0f, s_pBlockedCheckContext);
			}

			return false;
		}

		if (bChosenHintBlocked == true)
		{
			return false;
		}

		*pHint = pChosenHint;
		return true;
	}

	return false;
}

void CAndroidTemplateMaker::DoBlockedEffects(CBaseEntity* pBlocker, Vector vOrigin)
{
	// If the object blocking the hole is a physics object, wobble it a bit.
	if (pBlocker)
	{
		IPhysicsObject* pPhysObj = pBlocker->VPhysicsGetObject();

		if (pPhysObj && pPhysObj->IsAsleep())
		{
			// Don't bonk the object unless it is at rest.
			float x = RandomFloat(-5000, 5000);
			float y = RandomFloat(-5000, 5000);

			Vector vecForce = Vector(x, y, RandomFloat(10000, 15000));
			pPhysObj->ApplyForceCenter(vecForce);

			UTIL_CreateAntlionDust(vOrigin, vec3_angle, true);
			pBlocker->EmitSound("NPC_Antlion.MeleeAttackSingle_Muffled");
			pBlocker->EmitSound("NPC_Antlion.TrappedMetal");


			m_flBlockedBumpTime = gpGlobals->curtime + random->RandomFloat(1.75, 2.75);
		}
	}
}

CBaseEntity* CAndroidTemplateMaker::AllHintsFromClusterBlocked(CAI_Hint* pNode, bool& bChosenHintBlocked)
{
	// Only do this for episodic content!
	if (hl2_episodic.GetBool() == false)
		return NULL;

	CBaseEntity* pBlocker = NULL;

	if (pNode != NULL)
	{
		int iNumBlocked = 0;
		int iNumNodes = 0;

		CHintCriteria	hintCriteria;

		hintCriteria.SetGroup(m_strSpawnGroup);

		CUtlVector<CAI_Hint*> hintList;
		CAI_HintManager::FindAllHints(vec3_origin, hintCriteria, &hintList);

		for (int i = 0; i < hintList.Count(); i++)
		{
			CAI_Hint* pTestHint = hintList[i];

			if (pTestHint)
			{
				if (pTestHint->NameMatches(pNode->GetEntityName()))
				{
					bool bBlocked;

					iNumNodes++;

					Vector spawnOrigin;
					pTestHint->GetPosition(HULL_MEDIUM, &spawnOrigin);

					bBlocked = false;

					CBaseEntity* pList[20];

					int count = UTIL_EntitiesInBox(pList, 20, spawnOrigin + NAI_Hull::Mins(HULL_MEDIUM), spawnOrigin + NAI_Hull::Maxs(HULL_MEDIUM), 0);

					//Iterate over all the possible targets
					for (int i = 0; i < count; i++)
					{
						if (pList[i]->GetMoveType() != MOVETYPE_VPHYSICS)
							continue;

						if (PhysGetEntityMass(pList[i]) > ANDROID_MAKER_BLOCKED_MASS)
						{
							bBlocked = true;
							iNumBlocked++;
							pBlocker = pList[i];

							if (pTestHint == pNode)
							{
								bChosenHintBlocked = true;
							}

							break;
						}
					}
				}
			}
		}

		//All the nodes from this cluster are blocked so start playing the effects.
		if (iNumNodes > 0 && iNumBlocked == iNumNodes)
		{
			return pBlocker;
		}
	}

	return NULL;
}

void CAndroidTemplateMaker::FindNodesCloseToPlayer(void)
{
	SetContextThink(&CAndroidTemplateMaker::FindNodesCloseToPlayer, gpGlobals->curtime + random->RandomFloat(0.75, 1.75), s_pBlockedEffectsThinkContext);

	CBasePlayer* pPlayer = AI_GetSinglePlayer();

	if (pPlayer == NULL)
		return;

	CHintCriteria hintCriteria;

	float flRadius = ANDROID_MAKER_PLAYER_DETECT_RADIUS;

	hintCriteria.SetGroup(m_strSpawnGroup);
	hintCriteria.SetHintType(HINT_ANTLION_BURROW_POINT);
	hintCriteria.AddIncludePosition(pPlayer->GetAbsOrigin(), ANDROID_MAKER_PLAYER_DETECT_RADIUS);

	CUtlVector<CAI_Hint*> hintList;

	if (CAI_HintManager::FindAllHints(vec3_origin, hintCriteria, &hintList) <= 0)
		return;

	CUtlVector<string_t> m_BlockedNames;

	//----
	//What we do here is find all hints of the same name (cluster name) and figure out if all of them are blocked.
	//If they are then we only need to play the blocked effects once
	//---
	for (int i = 0; i < hintList.Count(); i++)
	{
		CAI_Hint* pNode = hintList[i];

		if (pNode && pNode->HintMatchesCriteria(NULL, hintCriteria, pPlayer->GetAbsOrigin(), &flRadius))
		{
			bool bClusterAlreadyBlocked = false;

			//Have one of the nodes from this cluster been checked for blockage? If so then there's no need to do block checks again for this cluster.
			for (int iStringCount = 0; iStringCount < m_BlockedNames.Count(); iStringCount++)
			{
				if (pNode->NameMatches(m_BlockedNames[iStringCount]))
				{
					bClusterAlreadyBlocked = true;
					break;
				}
			}

			if (bClusterAlreadyBlocked == true)
				continue;

			Vector vHintPos;
			pNode->GetPosition(HULL_MEDIUM, &vHintPos);

			bool bBlank;
			if (CBaseEntity* pBlocker = AllHintsFromClusterBlocked(pNode, bBlank))
			{
				DoBlockedEffects(pBlocker, vHintPos);
				m_BlockedNames.AddToTail(pNode->GetEntityName());
			}
		}
	}
}

void CAndroidTemplateMaker::BlockedCheckFunc(void)
{
	SetContextThink(&CAndroidTemplateMaker::BlockedCheckFunc, -1, s_pBlockedCheckContext);

	if (m_bBlocked == true)
		return;

	CUtlVector<CAI_Hint*> hintList;
	int iBlocked = 0;

	CHintCriteria	hintCriteria;

	hintCriteria.SetGroup(m_strSpawnGroup);

	if (CAI_HintManager::FindAllHints(vec3_origin, hintCriteria, &hintList) > 0)
	{
		for (int i = 0; i < hintList.Count(); i++)
		{
			CAI_Hint* pNode = hintList[i];

			if (pNode)
			{
				Vector vHintPos;
				pNode->GetPosition(AI_GetSinglePlayer(), &vHintPos);

				CBaseEntity* pList[20];
				int count = UTIL_EntitiesInBox(pList, 20, vHintPos + NAI_Hull::Mins(HULL_MEDIUM), vHintPos + NAI_Hull::Maxs(HULL_MEDIUM), 0);

				//Iterate over all the possible targets
				for (int i = 0; i < count; i++)
				{
					if (pList[i]->GetMoveType() != MOVETYPE_VPHYSICS)
						continue;

					if (PhysGetEntityMass(pList[i]) > ANDROID_MAKER_BLOCKED_MASS)
					{
						iBlocked++;
						break;
					}
				}
			}
		}
	}

	if (iBlocked > 0 && hintList.Count() == iBlocked)
	{
		m_bBlocked = true;
		m_OnAllBlocked.FireOutput(this, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputSetFightTarget(inputdata_t& inputdata)
{
	// Set our new goal
	m_strFightTarget = MAKE_STRING(inputdata.value.String());

	SetFightTarget(m_strFightTarget, inputdata.pActivator, inputdata.pCaller);

	UpdateChildren();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputSetFollowTarget(inputdata_t& inputdata)
{
	// Set our new goal
	m_strFollowTarget = MAKE_STRING(inputdata.value.String());

	SetFollowTarget(m_strFollowTarget, inputdata.pActivator, inputdata.pCaller);

	UpdateChildren();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputClearFightTarget(inputdata_t& inputdata)
{
	SetFightTarget(NULL);

	UpdateChildren();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputClearFollowTarget(inputdata_t& inputdata)
{
	SetFollowTarget(NULL);

	UpdateChildren();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputSetSpawnRadius(inputdata_t& inputdata)
{
	m_flSpawnRadius = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputAddToPool(inputdata_t& inputdata)
{
	PoolAdd(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputSetMaxPool(inputdata_t& inputdata)
{
	m_iMaxPool = inputdata.value.Int();
	if (m_iPool > m_iMaxPool)
	{
		m_iPool = m_iMaxPool;
	}

	// Stop regenerating if we're supposed to stop using the pool
	if (!m_iMaxPool)
	{
		SetContextThink(NULL, gpGlobals->curtime, s_pPoolThinkContext);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputSetPoolRegenAmount(inputdata_t& inputdata)
{
	m_iPoolRegenAmount = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputSetPoolRegenTime(inputdata_t& inputdata)
{
	m_flPoolRegenTime = inputdata.value.Float();

	if (m_flPoolRegenTime != 0.0f)
	{
		SetContextThink(&CAndroidTemplateMaker::PoolRegenThink, gpGlobals->curtime + m_flPoolRegenTime, s_pPoolThinkContext);
	}
	else
	{
		SetContextThink(NULL, gpGlobals->curtime, s_pPoolThinkContext);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pool behavior for coast
// Input  : iNumToAdd - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::PoolAdd(int iNumToAdd)
{
	m_iPool = clamp(m_iPool + iNumToAdd, 0, m_iMaxPool);
}

//-----------------------------------------------------------------------------
// Purpose: Regenerate the pool
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::PoolRegenThink(void)
{
	if (m_iPool < m_iMaxPool)
	{
		m_iPool = clamp(m_iPool + m_iPoolRegenAmount, 0, m_iMaxPool);
	}

	SetContextThink(&CAndroidTemplateMaker::PoolRegenThink, gpGlobals->curtime + m_flPoolRegenTime, s_pPoolThinkContext);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::DeathNotice(CBaseEntity* pVictim)
{
	CNPC_Android* pAnt = dynamic_cast<CNPC_Android*>(pVictim);
	if (pAnt == NULL)
		return;

	// Take it out of our list
	RemoveChild(pAnt);

	// Check if all live children are now dead
	if (m_nLiveChildren <= 0)
	{
		// Fire the output for this case
		m_OnAllLiveChildrenDead.FireOutput(this, this);

		bool bPoolDepleted = (m_iMaxPool != 0 && m_iPool == 0);
		if (bPoolDepleted || IsDepleted())
		{
			// Signal that all our children have been spawned and are now dead
			m_OnAllSpawnedDead.FireOutput(this, this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: If this had a finite number of children, return true if they've all
//			been created.
//-----------------------------------------------------------------------------
bool CAndroidTemplateMaker::IsDepleted(void)
{
	// If we're running pool behavior, we're never depleted
	if (m_iMaxPool)
		return false;

	return BaseClass::IsDepleted();
}

//-----------------------------------------------------------------------------
// Purpose: Change the spawn group the maker is using
//-----------------------------------------------------------------------------
void CAndroidTemplateMaker::InputChangeDestinationGroup(inputdata_t& inputdata)
{
	// FIXME: This function is redundant to the base class version, remove the m_strSpawnGroup
	m_strSpawnGroup = inputdata.value.StringID();
}