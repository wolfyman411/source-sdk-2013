//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Heavily armored combine infantry
//
//=============================================================================

#include "cbase.h"
#include "npc_combines.h"
#include "weapon_physcannon.h"
#include "hl2_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_combine_armored_health( "sk_combine_armored_health", "0" );
ConVar	sk_combine_armored_kick( "sk_combine_armored_kick", "0" );
ConVar	sk_combine_armored_armor_chest_health( "sk_combine_armored_armor_chest_health", "0" );
ConVar	sk_combine_armored_armor_health( "sk_combine_armored_armor_health", "0" );

//-----------------------------------------------------------------------------
// Purpose: Heavily armored combine infantry
//-----------------------------------------------------------------------------
class CArmorPiece : public CDynamicProp
{
    DECLARE_CLASS( CArmorPiece, CDynamicProp );
    public:
    CBaseEntity* m_pCombineUnit;

    void Spawn( void )
    {
        Precache();

        BaseClass::Spawn();
        SetModel( STRING( GetModelName() ) );

        CreateVPhysics();
    }

    void Precache( void )
    {
        PrecacheModel( STRING( GetModelName() ) );
    }

    bool CreateVPhysics( void )
    {
        SetSolid( SOLID_VPHYSICS );
        IPhysicsObject* pPhysicsObject = VPhysicsInitShadow( false, false );

        if ( !pPhysicsObject )
        {
            SetSolid( SOLID_NONE );
            SetMoveType( MOVETYPE_NONE );
            Warning( "ERROR!: Can't create physics object for %s\n", STRING( GetModelName() ) );
        }

        return true;
    }

    int OnTakeDamage( const CTakeDamageInfo& info )
    {
        if ( this->GetHealth() <= 0 )
        {
            this->OnPieceBreak();
        }

        DevLog( "Armor piece took damage: %f\n", info.GetDamage() );
        DevLog( "Current armor piece health: %d\n", this->GetHealth() );
        DevLog( "Damage is fit: %s\n", ( info.GetDamageType() & ( DMG_CLUB | DMG_BULLET | DMG_BLAST ) ) ? "true" : "false" );

        if ( ( info.GetDamageType() & ( DMG_CLUB | DMG_BULLET | DMG_BLAST ) ) )
        {
            this->SetHealth( this->GetHealth() - info.GetDamage() );
        }
        
        return 0;
    }
   
    void OnPieceBreak( void );
};

LINK_ENTITY_TO_CLASS( combine_armor_piece, CArmorPiece );

//-----------------------------------------------------------------------------
// Purpose: Heavily armored combine infantry
//-----------------------------------------------------------------------------
class CNPC_Combine_Armored : public CNPC_CombineS
{
	DECLARE_CLASS( CNPC_Combine_Armored, CNPC_CombineS );
public: 
	void		Spawn( void );
	void		Precache( void );

	void		SpawnArmorPieces( void );
    void        Event_Killed( const CTakeDamageInfo &info );

    int         OnTakeDamage_Alive( const CTakeDamageInfo& info );

    CUtlVector<CArmorPiece*> m_ArmorPieces;
};

LINK_ENTITY_TO_CLASS( npc_combine_armored, CNPC_Combine_Armored );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Combine_Armored::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

    m_nSkin = 1;

	SetHealth( sk_combine_armored_health.GetFloat() );
	SetMaxHealth( sk_combine_armored_health.GetFloat() );
	SetKickDamage( sk_combine_armored_kick.GetFloat() );

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	BaseClass::Spawn();
    CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
    CapabilitiesAdd( bits_CAP_DOORS_GROUP );

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
		{ Vector(2, -8, 60),       QAngle(0, 0, 0),     20.0f,		"models/combine_scanner.mdl"},
		{ Vector(2, 8, 60),		   QAngle(0, 0, 0),     20.0f,		"models/Gibs/HGIBS.mdl"},
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
            pArmor->SetHealth( i == 0 ? sk_combine_armored_armor_chest_health.GetInt() : sk_combine_armored_armor_health.GetInt() );

            DevLog( "Spawning armor piece with health: %d\n", pArmor->GetHealth() );

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

int CNPC_Combine_Armored::OnTakeDamage_Alive( const CTakeDamageInfo& info )
{
    if ( info.GetDamageType() & DMG_CLUB )
    {
        CArmorPiece* pClosestArmour = NULL;
        Vector vecDamage = info.GetDamagePosition();
        for ( int i = 0; i < m_ArmorPieces.Count(); i++ )
        {
            if ( m_ArmorPieces[ i ] && !pClosestArmour || ( vecDamage - m_ArmorPieces[ i ]->GetAbsOrigin() ).LengthSqr() < ( vecDamage - pClosestArmour->GetAbsOrigin() ).LengthSqr() )
            {
                pClosestArmour = m_ArmorPieces[ i ];
            }
        }

        if ( pClosestArmour )
        {
            float flDot = ( vecDamage - GetAbsOrigin() ).Normalized().Dot( ( pClosestArmour->GetAbsOrigin() - GetAbsOrigin() ).Normalized() );
            if ( flDot >= 0.975 )
            {
                pClosestArmour->TakeDamage( info );
            }
        }
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