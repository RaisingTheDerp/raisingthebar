//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements an explosion entity and a support spark shower entity.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "decals.h"
#include "explode.h"
#include "ai_basenpc.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "vstdlib/strtools.h"

//-----------------------------------------------------------------------------
// Purpose: Spark shower, created by the explosion entity.
//-----------------------------------------------------------------------------
class CShower : public CPointEntity
{
public:
	DECLARE_CLASS( CShower, CPointEntity );

	void Spawn( void );
	void Think( void );
	void Touch( CBaseEntity *pOther );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( spark_shower, CShower );


void CShower::Spawn( void )
{
	Vector vecForward;
	AngleVectors( GetLocalAngles(), &vecForward );

	Vector vecNewVelocity;
	vecNewVelocity = random->RandomFloat( 200, 300 ) * vecForward;
	vecNewVelocity.x += random->RandomFloat(-100.f,100.f);
	vecNewVelocity.y += random->RandomFloat(-100.f,100.f);
	if ( vecNewVelocity.z >= 0 )
		vecNewVelocity.z += 200;
	else
		vecNewVelocity.z -= 200;
	SetAbsVelocity( vecNewVelocity );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetGravity(0.5);
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetSolid( SOLID_NONE );
	UTIL_SetSize(this, vec3_origin, vec3_origin );
	m_fEffects |= EF_NODRAW;
	m_flSpeed = random->RandomFloat( 0.5, 1.5 );

	SetLocalAngles( vec3_angle );
}


void CShower::Think( void )
{
	g_pEffects->Sparks( GetAbsOrigin() );

	m_flSpeed -= 0.1;
	if ( m_flSpeed > 0 )
		SetNextThink( gpGlobals->curtime + 0.1f );
	else
		UTIL_Remove( this );
	RemoveFlag( FL_ONGROUND );
}


void CShower::Touch( CBaseEntity *pOther )
{
	Vector vecNewVelocity = GetAbsVelocity();

	if ( GetFlags() & FL_ONGROUND )
		vecNewVelocity *= 0.1;
	else
		vecNewVelocity *= 0.6;

	if ( (vecNewVelocity.x*vecNewVelocity.x+vecNewVelocity.y*vecNewVelocity.y) < 10.0 )
		m_flSpeed = 0;

	SetAbsVelocity( vecNewVelocity );
}


class CEnvExplosion : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvExplosion, CPointEntity );

	CEnvExplosion( void )
	{
		// Default to invalid.
		m_sFireballSprite = -1;
	};

	void Precache( void );
	void Spawn( );
	void Smoke ( void );
	bool KeyValue( const char *szKeyName, const char *szValue );

	// Input handlers
	void InputExplode( inputdata_t &inputdata );

	DECLARE_DATADESC();

	int m_iMagnitude;// how large is the fireball? how much damage?
	int m_iRadiusOverride;// For use when m_iMagnitude results in larger radius than designer desires.
	int m_spriteScale; // what's the exact fireball sprite scale? 
	string_t m_iszFireballSprite;
	short m_sFireballSprite;
};

LINK_ENTITY_TO_CLASS( env_explosion, CEnvExplosion );

BEGIN_DATADESC( CEnvExplosion )

	DEFINE_KEYFIELD( CEnvExplosion, m_iMagnitude, FIELD_INTEGER, "iMagnitude" ),
	DEFINE_KEYFIELD( CEnvExplosion, m_iRadiusOverride, FIELD_INTEGER, "iRadiusOverride" ),
	DEFINE_FIELD( CEnvExplosion, m_spriteScale, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvExplosion, m_iszFireballSprite, FIELD_STRING ),
	DEFINE_FIELD( CEnvExplosion, m_sFireballSprite, FIELD_SHORT ),

	// Function Pointers
	DEFINE_THINKFUNC( CEnvExplosion, Smoke ),

	// Inputs
	DEFINE_INPUTFUNC(CEnvExplosion, FIELD_VOID, "Explode", InputExplode),

END_DATADESC()


bool CEnvExplosion::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "fireballsprite"))
	{
		m_iszFireballSprite = AllocPooledString( szValue );
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

void CEnvExplosion::Precache( void )
{
	if ( m_iszFireballSprite != NULL_STRING )
	{
		m_sFireballSprite = engine->PrecacheModel( STRING( m_iszFireballSprite ) );
	}
}

void CEnvExplosion::Spawn( void )
{ 
	Precache();

	SetSolid( SOLID_NONE );
	m_fEffects |= EF_NODRAW;

	SetMoveType( MOVETYPE_NONE );
	/*
	if ( m_iMagnitude > 250 )
	{
		m_iMagnitude = 250;
	}
	*/

	float flSpriteScale;
	flSpriteScale = ( m_iMagnitude - 50) * 0.6;

	// Control the clamping of the fireball sprite
	if( m_spawnflags & SF_ENVEXPLOSION_NOCLAMPMIN )
	{
		// Don't inhibit clamping altogether. Just relax it a bit.
		if ( flSpriteScale < 1 )
		{
			flSpriteScale = 1;
		}
	}
	else
	{
		if ( flSpriteScale < 10 )
		{
			flSpriteScale = 10;
		}
	}

	if( m_spawnflags & SF_ENVEXPLOSION_NOCLAMPMAX )
	{
		// We may need to adjust this to suit designers' needs.
		if ( flSpriteScale > 200 )
		{
			flSpriteScale = 200;
		}
	}
	else
	{
		if ( flSpriteScale > 50 )
		{
			flSpriteScale = 50;
		}
	}

	m_spriteScale = (int)flSpriteScale;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for making the explosion explode.
//-----------------------------------------------------------------------------
void CEnvExplosion::InputExplode( inputdata_t &inputdata )
{ 
	trace_t tr;

	SetModelName( NULL_STRING );//invisible
	SetSolid( SOLID_NONE );// intangible

	// recalc absorigin
	Relink();

	Vector vecSpot = GetAbsOrigin() + Vector( 0 , 0 , 8 );
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	
	// Pull out of the wall a bit. We used to move the explosion origin itself, but that seems unnecessary, not to mention a
	// little weird when you consider that it might be in hierarchy. Instead we just calculate a new virtual position at
	// which to place the explosion. We don't use that new position to calculate radius damage because according to Steve's
	// comment, that adversely affects the force vector imparted on explosion victims when they ragdoll.
	Vector vecExplodeOrigin = GetAbsOrigin();
	if ( tr.fraction != 1.0 )
	{
		vecExplodeOrigin = tr.endpos + (tr.plane.normal * 24 );
	}

	// draw decal
	if (! ( m_spawnflags & SF_ENVEXPLOSION_NODECAL))
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	// It's stupid that this entity's spawnflags and the flags for the
	// explosion temp ent don't match up. But because they don't, we
	// have to reinterpret some of the spawnflags to determine which
	// flags to pass to the temp ent.
	int nFlags = TE_EXPLFLAG_NONE;

	if( m_spawnflags & SF_ENVEXPLOSION_NOFIREBALL )
	{
		nFlags |= TE_EXPLFLAG_NOFIREBALL;
	}
	
	if( m_spawnflags & SF_ENVEXPLOSION_NOSOUND )
	{
		nFlags |= TE_EXPLFLAG_NOSOUND;
	}
	
	if ( m_spawnflags & SF_ENVEXPLOSION_RND_ORIENT )
	{
		nFlags |= TE_EXPLFLAG_ROTATE;
	}

	if ( m_nRenderMode == kRenderTransAlpha )
	{
		nFlags |= TE_EXPLFLAG_DRAWALPHA;
	}
	else if ( m_nRenderMode != kRenderTransAdd )
	{
		nFlags |= TE_EXPLFLAG_NOADDITIVE;
	}

	if( m_spawnflags & SF_ENVEXPLOSION_NOPARTICLES )
	{
		nFlags |= TE_EXPLFLAG_NOPARTICLES;
	}

	if( m_spawnflags & SF_ENVEXPLOSION_NODLIGHTS )
	{
		nFlags |= TE_EXPLFLAG_NODLIGHTS;
	}

	//Get the damage override if specified
	int	iRadius = ( m_iRadiusOverride > 0 ) ? m_iRadiusOverride : ( m_iMagnitude * 2.5f );

	CPASFilter filter( vecExplodeOrigin );
	te->Explosion( filter, 0.0,
		&vecExplodeOrigin, 
		( m_sFireballSprite < 1 ) ? g_sModelIndexFireball : m_sFireballSprite,
		!( m_spawnflags & SF_ENVEXPLOSION_NOFIREBALL ) ? ( m_spriteScale / 10.0 ) : 0.0,
		15,
		nFlags,
		iRadius,
		m_iMagnitude );

	// do damage
	if ( !( m_spawnflags & SF_ENVEXPLOSION_NODAMAGE ) )
	{
		CBaseEntity *pAttacker = GetOwnerEntity() ? GetOwnerEntity() : this;
		RadiusDamage( CTakeDamageInfo( this, pAttacker, m_iMagnitude, DMG_BLAST ), GetAbsOrigin(), iRadius, CLASS_NONE );
	}

	SetThink( &CEnvExplosion::Smoke );
	SetNextThink( gpGlobals->curtime + 0.3 );

	// draw sparks
	if ( !( m_spawnflags & SF_ENVEXPLOSION_NOSPARKS ) )
	{
		int sparkCount = random->RandomInt(0,3);

		for ( int i = 0; i < sparkCount; i++ )
		{
			QAngle angles;
			VectorAngles( tr.plane.normal, angles );
			Create( "spark_shower", vecExplodeOrigin, angles, NULL );
		}
	}
}


void CEnvExplosion::Smoke( void )
{
	if ( !(m_spawnflags & SF_ENVEXPLOSION_REPEATABLE) )
	{
		UTIL_Remove( this );
	}
}


// HACKHACK -- create one of these and fake a keyvalue to get the right explosion setup
void ExplosionCreate( const Vector &center, const QAngle &angles, CBaseEntity *pOwner, int magnitude, int radius, bool doDamage )
{
	char			buf[128];

	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, angles, pOwner );
	Q_snprintf( buf,sizeof(buf), "%3d", magnitude );
	char *szKeyName = "iMagnitude";
	char *szValue = buf;
	pExplosion->KeyValue( szKeyName, szValue );
	if ( !doDamage )
	{
		pExplosion->AddSpawnFlags( SF_ENVEXPLOSION_NODAMAGE );
	}

	//For E3, no sparks
	pExplosion->AddSpawnFlags( SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE );

	if ( radius )
	{
		Q_snprintf( buf,sizeof(buf), "%d", radius );
		pExplosion->KeyValue( "iRadiusOverride", buf );
	}
	variant_t emptyVariant;
	pExplosion->m_nRenderMode = kRenderTransAdd;
	pExplosion->SetOwnerEntity( pOwner );
	pExplosion->Spawn();
	pExplosion->AcceptInput( "Explode", NULL, NULL, emptyVariant, 0 );
}
