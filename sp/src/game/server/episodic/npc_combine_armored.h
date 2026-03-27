#ifndef NPC_COMBINES_ARMOURED_H
#define NPC_COMBINES_ARMOURED_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "npc_combines.h"
#include "props.h"
#include "grenade_teleport.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


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

    int OnTakeDamage( const CTakeDamageInfo& info );

    void OnPieceBreak( void );
};

class CNPC_Combine_Armored : public CNPC_CombineS
{
    DECLARE_CLASS( CNPC_Combine_Armored, CNPC_CombineS );
    //DECLARE_DATADESC();
public:
    void		Spawn( void );
    void		Precache( void );

    void		SpawnArmorPieces( void );
    void        Event_Killed( const CTakeDamageInfo& info );

    int         OnTakeDamage_Alive( const CTakeDamageInfo& info );

    float       GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo& info );

    bool        CanSpawnJumpers( void );

    void        SpawnJumper( void );

    void        HandleAnimEvent( animevent_t* pEvent );

    CUtlVector<CArmorPiece*> m_aArmorPieces;

    int         m_iSpawnedJumpers;
    int         m_iActiveJumpers;
};

//BEGIN_DATADESC( CNPC_Combine_Armored )
    //DEFINE_FIELD( m_iSpawnedJumpers, FIELD_INTEGER ),
    //DEFINE_FIELD( m_iActiveJumpers, FIELD_INTEGER ),
//END_DATADESC()

#endif