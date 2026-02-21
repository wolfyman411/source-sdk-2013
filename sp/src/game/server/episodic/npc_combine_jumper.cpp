#include "cbase.h"
#include "npc_combine_jumper.h"
#include "npc_combine_armored.h"
#include "props.h"

#include "tier0/memdbgon.h"

ConVar sk_combine_jumper_health( "sk_combine_jumper_health", "0" );
ConVar sk_combine_jumper_kick( "sk_combine_jumper_kick", "0" );

void CNPC_Combine_Jumper::Spawn( void )
{
    Precache();
    SetModel( STRING( GetModelName() ) );

    BaseClass::Spawn();

    CapabilitiesAdd( bits_CAP_MOVE_JUMP );
}

ConVar sk_combine_jumper_max_jump_rise( "sk_combine_jumper_max_jump_rise", "1024.0" );
ConVar sk_combine_jumper_max_jump_distance( "sk_combine_jumper_max_jump_distance", "1024.0" );
ConVar sk_combine_jumper_max_jump_drop( "sk_combine_jumper_max_jump_drop", "8192.0" );

bool CNPC_Combine_Jumper::IsJumpLegal( const Vector& startPos, const Vector& apex, const Vector& endPos ) const
{
    return BaseClass::IsJumpLegal( startPos, apex, endPos, sk_combine_jumper_max_jump_rise.GetFloat(), sk_combine_jumper_max_jump_drop.GetFloat(), sk_combine_jumper_max_jump_distance.GetFloat() );
}

void CNPC_Combine_Jumper::Precache( void )
{
    if ( !GetModelName() )
    {
        SetModelName( MAKE_STRING( "models/combine_soldier.mdl" ) );
    }

    PrecacheModel( STRING( GetModelName() ) );

    BaseClass::Precache();
}