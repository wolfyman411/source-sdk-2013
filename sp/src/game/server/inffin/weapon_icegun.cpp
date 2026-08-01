// This is the code we will need to use in order to freeze NPCs. Could be PrimaryAttack or SecondaryAttack. -TheMaster974

/*
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
		return;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(0);
	trace_t tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 128, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr);

	CAI_BaseNPC* pHitNPC = dynamic_cast<CAI_BaseNPC*>(tr.m_pEnt);
	if (pHitNPC)
	{
		pHitNPC->m_bHasFrozen = true; // This makes it so the NPC can be slowed down.
		float playbackrate = pHitNPC->GetPlaybackRate();
		playbackrate -= 0.04f; // This needs to be investigated.

    // This is the freezing effect.
		int colour = (1 + playbackrate) * 140;
		if (colour > 255)
			colour = 255;

		pHitNPC->SetRenderColorR(colour);

    // If we fall below a certain threshold, effectively freeze the NPC.
		if (playbackrate < 0.02f)
			playbackrate = FLT_EPSILON;

		pHitNPC->SetPlaybackRate(playbackrate);
	}
	
	SendWeaponAnim(ACT_VM_SECONDARYATTACK);
//	pPlayer->RemoveAmmo(1, m_iSecondaryAmmoType);

	// Primary fire delay time.
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;

	// Secondary fire delay time.
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.05f;
*/
