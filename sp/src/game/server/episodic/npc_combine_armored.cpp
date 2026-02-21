//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Heavily armored combine infantry
//
//=============================================================================

#include "cbase.h"
#include "npc_combine_armored.h"
#include "props.h"

#include "tier0/memdbgon.h"

ConVar	sk_combine_armored_health( "sk_combine_armored_health", "0" );
ConVar	sk_combine_armored_kick( "sk_combine_armored_kick", "0" );
ConVar	sk_combine_armored_armor_chest_health( "sk_combine_armored_armor_chest_health", "0" );
ConVar	sk_combine_armored_armor_health( "sk_combine_armored_armor_health", "0" );

//-----------------------------------------------------------------------------
// Purpose: Heavily armored combine infantry
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine_Armored::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	BaseClass::Spawn();

    m_nSkin = 1;

    SetHealth( sk_combine_armored_health.GetFloat() );
    SetMaxHealth( sk_combine_armored_health.GetFloat() );
    SetKickDamage( sk_combine_armored_kick.GetFloat() );

    CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
    CapabilitiesRemove( bits_CAP_MOVE_SHOOT );
    CapabilitiesAdd( bits_CAP_DOORS_GROUP );

    AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );

	SpawnArmorPieces();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Combine_Armored::Precache()
{
	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/combine_soldier.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "combine_armor_piece" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

// TODO: Rework this when we have a bloody modeller do attachments
struct ArmourPieceHitgroupStruct
{
    int iHitGroup;
    int iArmourPieceIndex;
};

static const ArmourPieceHitgroupStruct g_ArmorHitgroups[] = 
{
    { HITGROUP_RIGHTARM, 0 },
    { HITGROUP_LEFTARM, 1 },
    { HITGROUP_CHEST, 2 },
    { HITGROUP_RIGHTLEG, 3 },
    { HITGROUP_LEFTLEG, 4 },
};

inline CArmorPiece* GetArmourPieceHitgroup( CNPC_Combine_Armored* m_pCombine, int iHitGroup )
{
    for ( int i = 0; i < ARRAYSIZE( g_ArmorHitgroups ); i++ )
    {
        if ( g_ArmorHitgroups[ i ].iHitGroup == iHitGroup )
        {
            int iIndex = g_ArmorHitgroups[ i ].iArmourPieceIndex;
            if ( m_pCombine->m_ArmorPieces.IsValidIndex( iIndex ) )
            {
                return m_pCombine->m_ArmorPieces[ iIndex ];
            }
        }
    }

    return NULL;
}

void CNPC_Combine_Armored::SpawnArmorPieces( void )
{
	struct ArmorPieceStruct
	{
		Vector	vecOffset;
        QAngle  angOffset;

        int     iHealth;

		char	*pszModel;
	};

    ArmorPieceStruct ArmorPiecesData[] =
	{
		{ Vector(2, -8, 60),       QAngle(0, 0, 0),     20.0f,		"models/combine_scanner.mdl"},  // Right Arm ( 0 ) m_ArmorPieces.Element(0)
		{ Vector(2, 8, 60),		   QAngle(0, 0, 0),     20.0f,		"models/Gibs/HGIBS.mdl"},       // Left Arm    ( 1 ) m_ArmorPieces.Element(1)

        { Vector(15, 0, 50),	   QAngle( 0, 0, 0 ),   30.0f,		"models/props_lab/partsbin01.mdl" }, // Chest       ( 2 ) m_ArmorPieces.Element(2)

        { Vector( 8, 5, 25 ),	   QAngle( 0, 90, 0 ),   30.0f,		"models/props_junk/cinderblock01a.mdl" }, // Right Leg       ( 3 ) m_ArmorPieces.Element(3)
        { Vector( 8, -5, 25 ),	   QAngle( 0, 90, 0 ),   30.0f,		"models/props_junk/cinderblock01a.mdl" }, // Left Leg       ( 4 ) m_ArmorPieces.Element(4)
	};

	for ( int i = 0; i < ARRAYSIZE(ArmorPiecesData); i++ )
	{
		CArmorPiece* pArmor = (CArmorPiece*) CBaseEntity::CreateNoSpawn( "combine_armor_piece", GetAbsOrigin(), GetAbsAngles(), this );
        if ( pArmor )
        {
            pArmor->SetModelName( MAKE_STRING( ArmorPiecesData[i].pszModel ) );
            pArmor->SetParent( this );
            pArmor->SetLocalOrigin( ArmorPiecesData[i].vecOffset );
            pArmor->SetLocalAngles( ArmorPiecesData[ i ].angOffset );

            DispatchSpawn( pArmor );

            pArmor->Activate();
            pArmor->SetHealth( i == 2 ? sk_combine_armored_armor_chest_health.GetInt() : sk_combine_armored_armor_health.GetInt() );

            DevLog( "Spawning armor piece with health: %d\n", pArmor->GetHealth() );

            pArmor->ScriptSetColor( 50 * i, 50 * i, 50 * i );

            pArmor->m_pCombineUnit = this;
            m_ArmorPieces.AddToHead( pArmor );
        }	
	}
}

void CNPC_Combine_Armored::Event_Killed( const CTakeDamageInfo& info )
{
    for ( int i = 0; i < m_ArmorPieces.Count(); i++ )
    {
        if ( m_ArmorPieces[i] )
        {
            m_ArmorPieces[ i ]->RemoveDeferred();
        }
    }

    m_ArmorPieces.Purge();

    BaseClass::Event_Killed( info );
}

// TODO: Probably end up using this
ConVar sk_combine_armored_armour_tolerance_dist( "sk_combine_armored_armour_tolerance_dist", "5.0");

inline CArmorPiece* GetClosestArmourPlate( CNPC_Combine_Armored* m_hCombine, Vector vecPoint )
{
    CArmorPiece* pClosestArmour = NULL;
    float flClosestDistSqr = FLT_MAX;

    for ( int i = 0; i < m_hCombine->m_ArmorPieces.Count(); i++ )
    {
        if ( m_hCombine->m_ArmorPieces[ i ] )
        {
            if ( m_hCombine->m_ArmorPieces[ i ]->CollisionProp()->IsPointInBounds( vecPoint ) )
            {
                return m_hCombine->m_ArmorPieces[ i ];
            }
            else
            {
                float flDistSqr = m_hCombine->m_ArmorPieces[ i ]->GetAbsOrigin().DistToSqr( vecPoint );
                if ( flDistSqr < flClosestDistSqr )
                {
                    flClosestDistSqr = flDistSqr;
                    pClosestArmour = m_hCombine->m_ArmorPieces[ i ];
                }
            }
        }
    }

    if ( pClosestArmour )
    {
        return pClosestArmour;
    }

    return NULL;
}

ConVar sk_combine_armored_head_dmg_multiplier( "sk_combine_armored_head_dmg_multiplier", "0.3" );
float CNPC_Combine_Armored::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
    if ( iHitGroup == HITGROUP_HEAD && info.GetDamageType() & DMG_BULLET )
    {
        return sk_combine_armored_head_dmg_multiplier.GetFloat();
    }

    return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}


int CNPC_Combine_Armored::OnTakeDamage_Alive( const CTakeDamageInfo& info )
{
    int iArmourCount = m_ArmorPieces.Count();
    if ( iArmourCount <= 0 ) { return BaseClass::OnTakeDamage_Alive( info ); }

    if ( info.GetDamageType() & DMG_CLUB )
    {
        CArmorPiece* m_pRandomArmourPiece = m_ArmorPieces.Element( RandomInt( 0, iArmourCount - 1 ) );
        if ( m_pRandomArmourPiece )
        {
            m_pRandomArmourPiece->OnPieceBreak();
        }

        CTakeDamageInfo newInfo = info;
        newInfo.SetDamage( (float) info.GetDamage() / ( iArmourCount > 0 ? iArmourCount : 1 ) );

        return BaseClass::OnTakeDamage_Alive( newInfo );
    }
    else if ( info.GetDamageType() & DMG_BLAST )
    {
        CTakeDamageInfo newInfo = info;
        newInfo.SetDamage( 0 );

        return BaseClass::OnTakeDamage_Alive( newInfo );
    }

    return BaseClass::OnTakeDamage_Alive( info );
}

// NOTE: Keep this here, cus otherwise 'CArmorPiece' isn't even aware of 'CNPC_Combine_Armored' existing, and thus can't call 'pCombine->m_ArmorPieces.FindAndRemove( this )' in the case of an armor piece breaking
void CArmorPiece::OnPieceBreak( void ) {
    if ( m_pCombineUnit )
    {
        CNPC_Combine_Armored* pCombine = ( CNPC_Combine_Armored* ) m_pCombineUnit;
        if ( pCombine )
        {
            DevLog( "Valid combine unit found, removing self from its armor pieces list\n" );
            pCombine->m_ArmorPieces.FindAndRemove( this );
        }
    }

    CPhysicsProp* pProp = ( CPhysicsProp* ) CreateEntityByName( "prop_physics" );
    if ( pProp )
    {
        pProp->SetModelName( this->GetModelName() );
        pProp->SetAbsOrigin( this->GetAbsOrigin() );
        pProp->SetAbsAngles( this->GetAbsAngles() );

        DispatchSpawn( pProp );
        pProp->Activate();
    }

    UTIL_Remove( this );
}

int CArmorPiece::OnTakeDamage( const CTakeDamageInfo& info )
{
    DevLog( "Armor piece took damage: %f\n", info.GetDamage() );
    DevLog( "Current armor piece health: %d\n", this->GetHealth() );

    if ( info.GetDamageType() & DMG_BULLET )
    {
        this->SetHealth( this->GetHealth() - info.GetDamage() );

        if ( this->GetHealth() <= 0 )
        {
            this->OnPieceBreak();
        }
    }
    else if ( info.GetDamageType() & DMG_BLAST )
    {
        CNPC_Combine_Armored* pCombine = ( CNPC_Combine_Armored* ) this->m_pCombineUnit;
        if ( pCombine )
        {
            // Explosive damage is split between pieces of armor, and can only deal damage to the combine if no armor is left. For example if a grenade did 65 damage, and the total armor HP was 50, the combine would take 15 damage.
            float flTotalArmourHealth = 0.0f;
            int iArmourCount = pCombine->m_ArmorPieces.Count();
            for ( int i = 0; i < iArmourCount; i++ )
            {
                flTotalArmourHealth += pCombine->m_ArmorPieces[ i ] ? pCombine->m_ArmorPieces[ i ]->GetHealth() : 0.0f;
            }

            DevLog( "Total armor health: %f\n", flTotalArmourHealth );

            if ( flTotalArmourHealth > 0.0f )
            {
                float flDamageToArmor = MIN( info.GetDamage(), flTotalArmourHealth );
                float flDamageToCombine = info.GetDamage() - flDamageToArmor;
                float flDamagePerArmour = flDamageToArmor / iArmourCount;

                CTakeDamageInfo newInfo = info;
                newInfo.SetDamage( flDamagePerArmour );

                this->SetHealth( this->GetHealth() - newInfo.GetDamage() );

                if ( this->GetHealth() <= 0 )
                {
                    this->OnPieceBreak();
                }

                CTakeDamageInfo newInfoCombine = info;
                newInfoCombine.SetDamage( flDamageToCombine );
                pCombine->TakeDamage( newInfoCombine );

                return BaseClass::OnTakeDamage( newInfo );
            }
        }
    }
    else if (info.GetDamageType() & DMG_CLUB)
    {
        CNPC_Combine_Armored* pCombine = (CNPC_Combine_Armored*)this->m_pCombineUnit;
        if (pCombine)
        {
            int iArmourCount = pCombine->m_ArmorPieces.Count();
            this->OnPieceBreak();
            CTakeDamageInfo newInfoCombine = info;
            newInfoCombine.SetDamage((float)info.GetDamage() / (iArmourCount > 0 ? iArmourCount : 1));
            pCombine->TakeDamage(newInfoCombine);
        }
    }

    return BaseClass::OnTakeDamage( info );
}