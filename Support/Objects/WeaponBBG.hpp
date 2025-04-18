//=========================================================================
// WEAPON CANNON
//=========================================================================
#ifndef _WEAPONBBG_HPP
#define _WEAPONBBG_HPP

//=========================================================================
// INCLUDES
//=========================================================================
#include "NewWeapon.hpp"
#include "x_bitmap.hpp"

//=========================================================================
class weapon_bbg : public new_weapon
{
public:
	CREATE_RTTI( weapon_bbg , new_weapon , object )

								weapon_bbg		        ( void );
	virtual		  			   ~weapon_bbg		        ( void );

    // Object description
    virtual const object_desc&  GetTypeDesc             ( void ) const;
    static  const object_desc&  GetObjectType           ( void );
    
    virtual void                ProcessSfx				( void );    
    virtual s32                 GetTotalSecondaryAmmo   ( void );

    virtual	void	            OnEnumProp		        ( prop_enum& list );
	virtual	xbool	            OnProperty		        ( prop_query& rPropQuery );

    virtual void                OnAdvanceLogic          ( f32 DeltaTime );
    virtual void                InitWeapon              ( const vector3& rInitPos, render_state rRenderState, guid OwnerGuid );

    virtual	void				InitWeapon			    (   const char* pSkinFileName , 
                                                            const char* pAnimFileName , 
                                                            const vector3& rInitPos , 
                                                            const render_state& rRenderState = RENDER_STATE_PLAYER,
                                                            const guid& rParentGuid = NULL 
                                                        );
    
    virtual void                BeginAltRampUp          ( void );
    virtual void                EndAltRampUp            ( xbool bGoingIntoHold, xbool bSwitchingWeapon );
    virtual void                EndAltHold              ( xbool bSwitchingWeapon );    
    virtual void                BeginSwitchFrom         ( void );
    virtual void                BeginSwitchTo           ( void );
    virtual void                BeginIdle               ( xbool bNormalIdle = TRUE );
    virtual void                EndIdle                 ( void );
    virtual xbool               CanSwitchIdleAnim       ( void );


    virtual void                BeginAltFire            ( void );
    virtual void                EndAltFire              ( void );
    virtual void                BeginPrimaryFire        ( void );
    virtual void                EndPrimaryFire          ( void );
    virtual void                ReleaseAudio            ( void );

    virtual ammo_priority       GetPrimaryAmmoPriority  ( void ){ return AMMO_PRIMARY; }
    virtual ammo_priority       GetSecondaryAmmoPriority( void ){ return AMMO_PRIMARY; }

    virtual xbool               CanAltChainFire         ( void ) {return FALSE;}

    virtual xbool				CanReload			    ( const ammo_priority& Priority );
    virtual xbool               CanFire                 ( xbool bIsAltFire );
    virtual void                SetupRenderInformation   ( void );

    virtual vector3             GetLaserEmitPosition    ( void );
    virtual xbool               GetFiringStartPosition  ( vector3 &Pos );
            void                SuspendReloadFX         ( void );
    virtual const char*         GetLogicalName          ( void ) {return "BBG";}

    virtual void                ClearAmmo               ( const ammo_priority& rAmmoPriority = AMMO_PRIMARY );
    virtual void                DecrementAmmo           ( const ammo_priority& rAmmoPriority = AMMO_PRIMARY, const s32& nAmt = 1);
    virtual void                FillAmmo                ( void );

    virtual xbool               FireGhostPrimary            ( s32 iFirePoint, xbool bUseFireAt, vector3& FireAt );
    virtual xbool               FireGhostSecondary          ( s32 iFirePoint );    
    virtual void                OnRenderTransparent         ( void );
    virtual vector3             GetLaserReflectVector       ( player *pPlayer, const vector3& StartPos, const vector3& Direction, const collision_mgr::collision& Coll );
    virtual xbool               CheckLaserHitObject         (object* pObject);

    virtual xbool               HasFlashlight               ( void )        { return FALSE; }
    virtual xbool               ShouldDrawReticle           ( void );
    virtual s32                 IncrementZoom               ( void );

protected:
       
    virtual	xbool				FireWeaponProtected		    ( const vector3& InitPos , const vector3& BaseVelocity, const f32& Power , const radian3& InitRot , const guid& Owner, s32 iFirePoint );
    virtual	xbool				FireSecondaryProtected	    ( const vector3& InitPos , const vector3& BaseVelocity, const f32& Power , const radian3& InitRot , const guid& Owner, s32 iFirePoint );
    virtual	xbool				FireNPCWeaponProtected      ( const vector3& BaseVelocity , const vector3& Target , const guid& Owner, f32 fDegradeMultiplier, const xbool isHit  );
    virtual	xbool				FireNPCSecondaryProtected	( const vector3& BaseVelocity , const vector3& Target , const guid& Owner, f32 fDegradeMultiplier, const xbool isHit  );
   
    virtual void                RenderReloadFX              ( void );

    void                InitReloadFx                ( void );
    void                AdvanceReloadFx             ( f32 DeltaTime );
    void                DestroyReloadFx             ( void );
    
    void                KillLaserSound              ( void );
    void                PlayLaserSound              ( void );

    virtual void        UpdateReticle               ( f32 DeltaTime );
    vector3             GetWeaponCollisionOffset    ( void );

    s32                         m_LockonLoopId;
    s32                         m_LoopVoiceId;
    s32                         m_LaserOnLoopId;

    f32                         m_LastLockonTime;
    f32                         m_CurrentWaitTime;
    f32                         m_ReloadWaitTime;
    xbool                       m_bStartReloadFx;
    s32                         m_ReloadBoneIndex;

    fx_handle                   m_ReloadFxHandle;
    fx_handle                   m_NPCReloadFxHandle;
    rhandle<char>               m_ReloadFx;                     // The reload particle effect.
    rhandle<char>               m_PrimaryProjectileFx;          // The primary fire energy projectile particle effect.
    rhandle<char>               m_SecondaryProjectileFx;        // The secondary fire energy projectile particle effect.    
    rhandle<char>               m_WallBounceFx;                 // The effect when a projectile hits a wall.

    f32                         m_SecondaryFireBaseDamage;
    f32                         m_SecondaryFireMaxDamage;
    f32                         m_SecondaryFireSpeed;
    f32                         m_SecondaryFireBaseForce;
    f32                         m_SecondaryFireMaxForce;
    f32                         m_LastUpdateTime;       // since we don't get OnAdvanceLogic calls if weapon isn't equipped... store off time.
    f32                         m_bIsAltFiring;    
    f32                         m_LastAmmoBurnTime;
    s32                         m_AmmoBurned;
    guid                        m_SecondaryFireProjectileGuid;  // store off the guid to the projectile so we can release it.

    rhandle<char>               m_hBulletAudioPackage;  // Audio package for the weapon's bullets.

    rhandle<xbitmap>            m_LaserBitmap;
    rhandle<xbitmap>            m_LaserFixupBitmap;
};

//===========================================================================

inline
s32 weapon_bbg::GetTotalSecondaryAmmo( void )
{
    // SMP uses primary ammo for both shots.
    return m_WeaponAmmo[ AMMO_PRIMARY ].m_AmmoAmount;
}

//=========================================================================
// END
//=========================================================================    
#endif