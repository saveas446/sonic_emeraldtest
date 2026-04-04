// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_inter.c
/// \brief Handling interactions (i.e., collisions)

#include "doomdef.h"
#include "i_system.h"
#include "am_map.h"
#include "g_battle.h"
#include "g_game.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "r_main.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "lua_hook.h"
#include "m_cond.h" // unlockables, emblems, etc
#include "m_cheat.h" // objectplace
#include "m_misc.h"
#include "v_video.h" // video flags for CEchos

// CTF player names
#define CTFTEAMCODE(pl) pl->ctfteam ? (pl->ctfteam == 1 ? "\x85" : "\x84") : ""
#define CTFTEAMENDCODE(pl) pl->ctfteam ? "\x80" : ""

void P_ForceFeed(const player_t *player, INT32 attack, INT32 fade, tic_t duration, INT32 period)
{
	BasicFF_t Basicfeed;
	if (!player)
		return;
	Basicfeed.Duration = (UINT32)(duration * (100L/TICRATE));
	Basicfeed.ForceX = Basicfeed.ForceY = 1;
	Basicfeed.Gain = 25000;
	Basicfeed.Magnitude = period*10;
	Basicfeed.player = player;
	/// \todo test FFB
	P_RampConstant(&Basicfeed, attack, fade);
}

void P_ForceConstant(const BasicFF_t *FFInfo)
{
	JoyFF_t ConstantQuake;
	if (!FFInfo || !FFInfo->player)
		return;
	ConstantQuake.ForceX    = FFInfo->ForceX;
	ConstantQuake.ForceY    = FFInfo->ForceY;
	ConstantQuake.Duration  = FFInfo->Duration;
	ConstantQuake.Gain      = FFInfo->Gain;
	ConstantQuake.Magnitude = FFInfo->Magnitude;
	if (FFInfo->player == &players[consoleplayer])
		I_Tactile(ConstantForce, &ConstantQuake);
	else if (splitscreen && FFInfo->player == &players[secondarydisplayplayer])
		I_Tactile2(ConstantForce, &ConstantQuake);
}
void P_RampConstant(const BasicFF_t *FFInfo, INT32 Start, INT32 End)
{
	JoyFF_t RampQuake;
	if (!FFInfo || !FFInfo->player)
		return;
	RampQuake.ForceX    = FFInfo->ForceX;
	RampQuake.ForceY    = FFInfo->ForceY;
	RampQuake.Duration  = FFInfo->Duration;
	RampQuake.Gain      = FFInfo->Gain;
	RampQuake.Magnitude = FFInfo->Magnitude;
	RampQuake.Start     = Start;
	RampQuake.End       = End;
	if (FFInfo->player == &players[consoleplayer])
		I_Tactile(ConstantForce, &RampQuake);
	else if (splitscreen && FFInfo->player == &players[secondarydisplayplayer])
		I_Tactile2(ConstantForce, &RampQuake);
}


//
// GET STUFF
//

/** Makes sure all previous starposts are cleared.
  * For instance, hitting starpost 5 will clear starposts 1 through 4, even if
  * you didn't touch them. This is how the classic games work, although it can
  * lead to bizarre situations on levels that allow you to make a circuit.
  *
  * \param postnum The number of the starpost just touched.
  */
void P_ClearStarPost(INT32 postnum)
{
	thinker_t *th;
	mobj_t *mo2;

	// scan the thinkers
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2->type == MT_STARPOST && mo2->health <= postnum)
			P_SetMobjState(mo2, mo2->info->seestate);
	}
	return;
}

//
// P_ResetStarposts
//
// Resets all starposts back to their spawn state, used on A_Mixup and some other things.
//
void P_ResetStarposts(void)
{
	// Search through all the thinkers.
	thinker_t *th;
	mobj_t *post;

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		post = (mobj_t *)th;

		if (post->type == MT_STARPOST)
			P_SetMobjState(post, post->info->spawnstate);
	}
}

//
// P_CanPickupItem
//
// Returns true if the player is in a state where they can pick up items.
//
boolean P_CanPickupItem(player_t *player, boolean weapon)
{
	if (player->bot && weapon)
		return false;

	if (player->powers[pw_flashing] > (flashingtics/4)*3 && player->powers[pw_flashing] <= flashingtics)
		return false;

	return true;
}

//
// P_DoNightsScore
//
// When you pick up some items in nights, it displays
// a score sign, and awards you some drill time.
//
void P_DoNightsScore(player_t *player)
{
	mobj_t *dummymo;

	if (player->exiting)
		return; // Don't do any fancy shit for failures.

	dummymo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z+player->mo->height/2, MT_NIGHTSCORE);
	if (player->bot)
		player = &players[consoleplayer];

	if (G_IsSpecialStage(gamemap)) // Global link count? Maybe not a good idea...
	{
		INT32 i;
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i])
			{
				if (++players[i].linkcount > players[i].maxlink)
					players[i].maxlink = players[i].linkcount;
				players[i].linktimer = 2*TICRATE;
			}
	}
	else // Individual link counts
	{
		if (++player->linkcount > player->maxlink)
			player->maxlink = player->linkcount;
		player->linktimer = 2*TICRATE;
	}

	if (player->linkcount < 10)
	{
		if (player->bonustime)
		{
			P_AddPlayerScore(player, player->linkcount*20);
			P_SetMobjState(dummymo, dummymo->info->xdeathstate+player->linkcount-1);
		}
		else
		{
			P_AddPlayerScore(player, player->linkcount*10);
			P_SetMobjState(dummymo, dummymo->info->spawnstate+player->linkcount-1);
		}
	}
	else
	{
		if (player->bonustime)
		{
			P_AddPlayerScore(player, 200);
			P_SetMobjState(dummymo, dummymo->info->xdeathstate+9);
		}
		else
		{
			P_AddPlayerScore(player, 100);
			P_SetMobjState(dummymo, dummymo->info->spawnstate+9);
		}
	}

	// Hoops are the only things that should add to your drill meter
	//player->drillmeter += TICRATE;
	dummymo->momz = FRACUNIT;
	dummymo->fuse = 3*TICRATE;

	// What?! NO, don't use the camera! Scale up instead!
	//P_InstaThrust(dummymo, R_PointToAngle2(dummymo->x, dummymo->y, camera.x, camera.y), 3*FRACUNIT);
	dummymo->scalespeed = FRACUNIT/25;
	dummymo->destscale = 2*FRACUNIT;
}

/** Takes action based on a ::MF_SPECIAL thing touched by a player.
  * Actually, this just checks a few things (heights, toucher->player, no
  * objectplace, no dead or disappearing things)
  *
  * The special thing may be collected and disappear, or a sound may play, or
  * both.
  *
  * \param special     The special thing.
  * \param toucher     The player's mobj.
  * \param heightcheck Whether or not to make sure the player and the object
  *                    are actually touching.
  */
void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher, boolean heightcheck)
{
	player_t *player;
	//INT32 i;

	if (objectplacing)
		return;

	I_Assert(special != NULL);
	I_Assert(toucher != NULL);

	// Dead thing touching.
	// Can happen with a sliding player corpse.
	if (toucher->health <= 0)
		return;

	if (heightcheck)
	{
		if (special->type == MT_FLINGEMERALD) // little hack here...
		{ // flingemerald sprites are low to the ground, so extend collision radius down some.
			if (toucher->z > (special->z + special->height))
				return;
			if (special->z - special->height > (toucher->z + toucher->height))
				return;
		}
		else
		{
			if (toucher->momz < 0) {
				if (toucher->z + toucher->momz > special->z + special->height)
					return;
			} else if (toucher->z > special->z + special->height)
				return;
			if (toucher->momz > 0) {
				if (toucher->z + toucher->height + toucher->momz < special->z)
					return;
			} else if (toucher->z + toucher->height < special->z)
				return;
		}
	}

	if (special->health <= 0)
		return;

	// If we're touching the battletarget during the stroll animation, move on to the next step
	if (moveanim_step == 1 && special == moveanim_target && moveanim == MOVEANIM_STROLL && toucher == moveanim_source)
	{
		int dmg_min, dmg_max;

		// Damage enemy, if we're a player deplete the battle gauge
		if (tmthing->player)
			tmthing->player->battlegauge = max(0, tmthing->player->battlegauge - 360); // Don't go below 0

		// Set damage amount based on mobj type
		switch (moveanim_source->type) {
			case MT_PLAYER:
			dmg_min = 15;
			dmg_max = 25;
			break;
			case MT_BLUECRAWLA:
			default:
			dmg_min = 6;
			dmg_max = 12;
			break;
		}

		P_DamageMobj(moveanim_target, NULL, tmthing, P_RandomRange(dmg_min, dmg_max));

		moveanim_step = 2;
		return;
	}

	// Non-players can touch stuff now, but they can't get past this point

	if (!toucher->player)
		return;
	else
		player = toucher->player;

	if (player->spectator)
		return;

	// Ignore eggman in "ouchie" mode
	if (special->flags & MF_BOSS && special->flags2 & MF2_FRET)
		return;

#ifdef HAVE_BLUA
	if (LUAh_TouchSpecial(special, toucher) || P_MobjWasRemoved(special))
		return;
#endif

	if (special->flags & MF_SPECIAL) {
		// Pick up rings
		switch (special->type)
		{
			// Enemy. Skip this part.
			case MT_BLUECRAWLA:
				return;
			case MT_RING:
			//case MT_FLINGRING:
				if (!(P_CanPickupItem(player, false)))
					return;

				special->momx = special->momy = special->momz = 0;
				P_GivePlayerRings(player, 1);

				if ((maptol & TOL_NIGHTS) && special->type != MT_FLINGRING)
					P_DoNightsScore(player);
				break;
			/*default: // SOC or script pickup
				if (player->bot)
					return;
				P_SetTarget(&special->target, toucher);
				break;
			}*/
		}

		S_StartSound(toucher, special->info->deathsound); // was NULL, but changed to player so you could hear others pick up rings
		P_KillMobj(special, NULL, toucher);
	}
}

/** Prints death messages relating to a dying or hit player.
  *
  * \param player    Affected player.
  * \param inflictor The attack weapon used, can be NULL.
  * \param source    The attacker, can be NULL.
  */
static void P_HitDeathMessages(player_t *player, mobj_t *inflictor, mobj_t *source)
{
	const char *str = NULL;
	boolean deathonly = false;
	boolean deadsource = false;
	boolean deadtarget = false;
	// player names complete with control codes
	char targetname[MAXPLAYERNAME+4];
	char sourcename[MAXPLAYERNAME+4];

	if (G_PlatformGametype())
		return; // Not in coop, etc.

	if (!player)
		return; // Impossible!

	if (player->spectator)
		return; // No messages for dying (crushed) spectators.

	if (!netgame)
		return; // Presumably it's obvious what's happening in splitscreen.

#ifdef HAVE_BLUA
	if (LUAh_HurtMsg(player, inflictor, source))
		return;
#endif

	deadtarget = (player->health <= 0);

	// Target's name
	snprintf(targetname, sizeof(targetname), "%s%s%s",
	         CTFTEAMCODE(player),
	         player_names[player - players],
	         CTFTEAMENDCODE(player));

	if (source)
	{
		// inflictor shouldn't be NULL if source isn't
		I_Assert(inflictor != NULL);

		if (source->player)
		{
			// Source's name (now that we know there is one)
			snprintf(sourcename, sizeof(sourcename), "%s%s%s",
					 CTFTEAMCODE(source->player),
					 player_names[source->player - players],
					 CTFTEAMENDCODE(source->player));

			// We don't care if it's us.
			// "Player 1's [redacted] killed Player 1."
			if (source->player->playerstate == PST_DEAD && source->player != player &&
			 (inflictor->flags2 & MF2_BEYONDTHEGRAVE))
				deadsource = true;

			if (inflictor->flags & MF_PUSHABLE)
			{
				str = M_GetText("%s%s's playtime with heavy objects %s %s.\n");
			}
			else switch (inflictor->type)
			{
				case MT_PLAYER:
					if ((inflictor->player->powers[pw_shield] & SH_NOSTACK) == SH_BOMB)
						str = M_GetText("%s%s's armageddon blast %s %s.\n");
					else if (inflictor->player->powers[pw_invulnerability])
						str = M_GetText("%s%s's invincibility aura %s %s.\n");
					else if (inflictor->player->powers[pw_super])
						str = M_GetText("%s%s's super aura %s %s.\n");
					else
						str = M_GetText("%s%s's tagging hand %s %s.\n");
					break;
				case MT_SPINFIRE:
					str = M_GetText("%s%s's elemental fire trail %s %s.\n");
					break;
				case MT_THROWNBOUNCE:
					str = M_GetText("%s%s's bounce ring %s %s.\n");
					break;
				case MT_THROWNINFINITY:
					str = M_GetText("%s%s's infinity ring %s %s.\n");
					break;
				case MT_THROWNAUTOMATIC:
					str = M_GetText("%s%s's automatic ring %s %s.\n");
					break;
				case MT_THROWNSCATTER:
					str = M_GetText("%s%s's scatter ring %s %s.\n");
					break;
				// TODO: For next two, figure out how to determine if it was a direct hit or splash damage. -SH
				case MT_THROWNEXPLOSION:
					str = M_GetText("%s%s's explosion ring %s %s.\n");
					break;
				case MT_THROWNGRENADE:
					str = M_GetText("%s%s's grenade ring %s %s.\n");
					break;
				case MT_REDRING:
					if (inflictor->flags2 & MF2_RAILRING)
						str = M_GetText("%s%s's rail ring %s %s.\n");
					else
						str = M_GetText("%s%s's thrown ring %s %s.\n");
					break;
				default:
					str = M_GetText("%s%s %s %s.\n");
					break;
			}

			CONS_Printf(str,
				deadsource ? M_GetText("The late ") : "",
				sourcename,
				deadtarget ? M_GetText("killed") : M_GetText("hit"),
				targetname);
			return;
		}
		else switch (source->type)
		{
			case MT_NULL:
				switch(source->threshold)
				{
				case 42:
					deathonly = true;
					str = M_GetText("%s drowned.\n");
					break;
				case 43:
					str = M_GetText("%s was %s by spikes.\n");
					break;
				case 44:
					deathonly = true;
					str = M_GetText("%s was crushed.\n");
					break;
				}
				break;
			case MT_EGGMANICO:
			case MT_EGGMANBOX:
				str = M_GetText("%s was %s by Eggman's nefarious TV magic.\n");
				break;
			case MT_SPIKE:
				str = M_GetText("%s was %s by spikes.\n");
				break;
			default:
				str = M_GetText("%s was %s by an environmental hazard.\n");
				break;
		}
	}
	else
	{
		// null source, environment kills
		// TERRIBLE HACK for hit damage because P_DoPlayerPain moves the player...
		// I'll put it back, I promise!
		player->mo->z -= player->mo->momz+1;
		if (P_PlayerTouchingSectorSpecial(player, 1, 2))
			str = M_GetText("%s was %s by chemical water.\n");
		else if (P_PlayerTouchingSectorSpecial(player, 1, 3))
			str = M_GetText("%s was %s by molten lava.\n");
		else if (P_PlayerTouchingSectorSpecial(player, 1, 4))
			str = M_GetText("%s was %s by electricity.\n");
		else if (deadtarget)
		{
			deathonly = true;
			if (P_PlayerTouchingSectorSpecial(player, 1, 6)
			 || P_PlayerTouchingSectorSpecial(player, 1, 7))
				str = M_GetText("%s fell into a bottomless pit.\n");
			else if (P_PlayerTouchingSectorSpecial(player, 1, 12))
				str = M_GetText("%s asphyxiated in space.\n");
			else
				str = M_GetText("%s died.\n");
		}
		if (!str)
			str = M_GetText("%s was %s by an environmental hazard.\n");

		player->mo->z += player->mo->momz+1;
	}

	if (!str) // Should not happen! Unless we missed catching something above.
		return;

	// Don't log every hazard hit if they don't want us to.
	if (!deadtarget && !cv_hazardlog.value)
		return;

	if (deathonly)
	{
		if (!deadtarget)
			return;
		CONS_Printf(str, targetname);
	}
	else
		CONS_Printf(str, targetname, deadtarget ? M_GetText("killed") : M_GetText("hit"));
}

/** Checks if the level timer is over the timelimit and the round should end,
  * unless you are in overtime. In which case leveltime may stretch out beyond
  * timelimitintics and overtime's status will be checked here each tick.
  * Verify that the value of ::cv_timelimit is greater than zero before
  * calling this function.
  *
  * \sa cv_timelimit, P_CheckPointLimit, P_UpdateSpecials
  */
void P_CheckTimeLimit(void)
{
	INT32 i, k;

	if (!cv_timelimit.value)
		return;

	if (!(multiplayer || netgame))
		return;

	if (G_PlatformGametype())
		return;

	if (leveltime < timelimitintics)
		return;

	if (gameaction == ga_completed)
		return;

	//Tagmode round end but only on the tic before the
	//XD_EXITLEVEL packet is received by all players.
	if (G_TagGametype())
	{
		if (leveltime == (timelimitintics + 1))
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i] || players[i].spectator
				 || (players[i].pflags & PF_TAGGED) || (players[i].pflags & PF_TAGIT))
					continue;

				CONS_Printf(M_GetText("%s received double points for surviving the round.\n"), player_names[i]);
				P_AddPlayerScore(&players[i], players[i].score);
			}
		}

		if (server)
			SendNetXCmd(XD_EXITLEVEL, NULL, 0);
	}

	//Optional tie-breaker for Match/CTF
	else if (cv_overtime.value)
	{
		INT32 playerarray[MAXPLAYERS];
		INT32 tempplayer = 0;
		INT32 spectators = 0;
		INT32 playercount = 0;

		//Figure out if we have enough participating players to care.
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && players[i].spectator)
				spectators++;
		}

		if ((D_NumPlayers() - spectators) > 1)
		{
			// Play the starpost sfx after the first second of overtime.
			if (gamestate == GS_LEVEL && (leveltime == (timelimitintics + TICRATE)))
				S_StartSound(NULL, sfx_strpst);

			// Normal Match
			if (!G_GametypeHasTeams())
			{
				//Store the nodes of participating players in an array.
				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (playeringame[i] && !players[i].spectator)
					{
						playerarray[playercount] = i;
						playercount++;
					}
				}

				//Sort 'em.
				for (i = 1; i < playercount; i++)
				{
					for (k = i; k < playercount; k++)
					{
						if (players[playerarray[i-1]].score < players[playerarray[k]].score)
						{
							tempplayer = playerarray[i-1];
							playerarray[i-1] = playerarray[k];
							playerarray[k] = tempplayer;
						}
					}
				}

				//End the round if the top players aren't tied.
				if (players[playerarray[0]].score == players[playerarray[1]].score)
					return;
			}
			else
			{
				//In team match and CTF, determining a tie is much simpler. =P
				if (redscore == bluescore)
					return;
			}
		}
		if (server)
			SendNetXCmd(XD_EXITLEVEL, NULL, 0);
	}

	if (server)
		SendNetXCmd(XD_EXITLEVEL, NULL, 0);
}

/** Checks if a player's score is over the pointlimit and the round should end.
  * Verify that the value of ::cv_pointlimit is greater than zero before
  * calling this function.
  *
  * \sa cv_pointlimit, P_CheckTimeLimit, P_UpdateSpecials
  */
void P_CheckPointLimit(void)
{
	INT32 i;

	if (!cv_pointlimit.value)
		return;

	if (!(multiplayer || netgame))
		return;

	if (G_PlatformGametype())
		return;

	// pointlimit is nonzero, check if it's been reached by this player
	if (G_GametypeHasTeams())
	{
		// Just check both teams
		if ((UINT32)cv_pointlimit.value <= redscore || (UINT32)cv_pointlimit.value <= bluescore)
		{
			if (server)
				SendNetXCmd(XD_EXITLEVEL, NULL, 0);
		}
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;

			if ((UINT32)cv_pointlimit.value <= players[i].score)
			{
				if (server)
					SendNetXCmd(XD_EXITLEVEL, NULL, 0);
				return;
			}
		}
	}
}

/*Checks for untagged remaining players in both tag derivitave modes.
 *If no untagged players remain, end the round.
 *Also serves as error checking if the only IT player leaves.*/
void P_CheckSurvivors(void)
{
	INT32 i;
	INT32 survivors = 0;
	INT32 taggers = 0;
	INT32 spectators = 0;
	INT32 survivorarray[MAXPLAYERS];

	if (!D_NumPlayers()) //no players in the game, no check performed.
		return;

	for (i=0; i < MAXPLAYERS; i++) //figure out counts of taggers, survivors and spectators.
	{
		if (playeringame[i])
		{
			if (players[i].spectator)
				spectators++;
			else if (players[i].pflags & PF_TAGIT)
				taggers++;
			else if (!(players[i].pflags & PF_TAGGED))
			{
				survivorarray[survivors] = i;
				survivors++;
			}
		}
	}

	if (!taggers) //If there are no taggers, pick a survivor at random to be it.
	{
		// Exception for hide and seek. If a round has started and the IT player leaves, end the round.
		if (gametype == GT_HIDEANDSEEK && (leveltime >= (hidetime * TICRATE)))
		{
			CONS_Printf(M_GetText("The IT player has left the game.\n"));
			if (server)
				SendNetXCmd(XD_EXITLEVEL, NULL, 0);

			return;
		}

		if (survivors)
		{
			INT32 newtagger = survivorarray[P_RandomKey(survivors)];

			CONS_Printf(M_GetText("%s is now IT!\n"), player_names[newtagger]); // Tell everyone who is it!
			players[newtagger].pflags |= PF_TAGIT;

			survivors--; //Get rid of the guy we just made IT.

			//Yeah, we have an eligible tagger, but we may not have anybody for him to tag!
			//If there is only one guy waiting on the game to fill or spectators to enter game, don't bother.
			if (!survivors && (D_NumPlayers() - spectators) > 1)
			{
				CONS_Printf(M_GetText("All players have been tagged!\n"));
				if (server)
					SendNetXCmd(XD_EXITLEVEL, NULL, 0);
			}

			return;
		}

		//If we reach this point, no player can replace the one that was IT.
		//Unless it is one player waiting on a game, end the round.
		if ((D_NumPlayers() - spectators) > 1)
		{
			CONS_Printf(M_GetText("There are no players able to become IT.\n"));
			if (server)
				SendNetXCmd(XD_EXITLEVEL, NULL, 0);
		}

		return;
	}

	//If there are taggers, but no survivors, end the round.
	//Except when the tagger is by himself and the rest of the game are spectators.
	if (!survivors && (D_NumPlayers() - spectators) > 1)
	{
		CONS_Printf(M_GetText("All players have been tagged!\n"));
		if (server)
			SendNetXCmd(XD_EXITLEVEL, NULL, 0);
	}
}

// Checks whether or not to end a race netgame.
boolean P_CheckRacers(void)
{
	INT32 i;

	// Check if all the players in the race have finished. If so, end the level.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].exiting && players[i].lives > 0)
			break;
	}

	if (i == MAXPLAYERS) // finished
	{
		countdown = 0;
		countdown2 = 0;
		return true;
	}

	return false;
}

/** Kills an object.
  *
  * \param target    The victim.
  * \param inflictor The attack weapon. May be NULL (environmental damage).
  * \param source    The attacker. May be NULL.
  * \todo Cleanup, refactor, split up.
  * \sa P_DamageMobj
  */
void P_KillMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source)
{
	mobjtype_t item;
	mobj_t *mo;

	if (inflictor && (inflictor->type == MT_SHELL || inflictor->type == MT_FIREBALL))
		P_SetTarget(&target->tracer, inflictor);

	if (!useNightsSS && G_IsSpecialStage(gamemap) && target->player && sstimer > 6)
		sstimer = 6; // Just let P_Ticker take care of the rest.

	if (target->flags & (MF_ENEMY|MF_BOSS))
		target->momx = target->momy = target->momz = 0;

	if (target->type != MT_PLAYER && !(target->flags & MF_MONITOR))
		target->flags |= MF_NOGRAVITY|MF_NOCLIP|MF_NOCLIPHEIGHT; // Don't drop Tails 03-08-2000

	if (target->flags2 & MF2_NIGHTSPULL)
		P_SetTarget(&target->tracer, NULL);

	// dead target is no more shootable
	target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SPECIAL);
	target->flags2 &= ~(MF2_SKULLFLY|MF2_NIGHTSPULL);
	target->health = 0; // This makes it easy to check if something's dead elsewhere.

#ifdef HAVE_BLUA
	if (LUAh_MobjDeath(target, inflictor, source) || P_MobjWasRemoved(target))
		return;
#endif

	// Let EVERYONE know what happened to a player! 01-29-2002 Tails
	if (target->player && !target->player->spectator)
	{
		if (metalrecording) // Ack! Metal Sonic shouldn't die! Cut the tape, end recording!
			G_StopMetalRecording();
		if (gametype == GT_MATCH && cv_match_scoring.value == 0 // note, no team match suicide penalty
			&& ((target == source) || (source == NULL && inflictor == NULL) || (source && !source->player)))
		{ // Suicide penalty
			if (target->player->score >= 50)
				target->player->score -= 50;
			else
				target->player->score = 0;
		}

		target->flags2 &= ~MF2_DONTDRAW;
	}

	// if killed by a player
	if (source && source->player)
	{
		if (target->flags & MF_MONITOR)
		{
			P_SetTarget(&target->target, source);
			source->player->numboxes++;
			if ((cv_itemrespawn.value && gametype != GT_COOP && (modifiedgame || netgame || multiplayer)))
				target->fuse = cv_itemrespawntime.value*TICRATE + 2; // Random box generation
		}

		// Award Score Tails
		{
			INT32 score = 0;

			if (maptol & TOL_NIGHTS) // Enemies always worth 200, bosses don't do anything.
			{
				if ((target->flags & MF_ENEMY) && !(target->flags & (MF_MISSILE|MF_BOSS)))
				{
					score = 200;

					if (source->player->bonustime)
						score *= 2;

					// Also, add to the link.
					// I don't know if NiGHTS did this, but
					// Sonic Time Attacked did and it seems like a good enough incentive
					// to make people want to actually dash towards/paraloop enemies
					if (++source->player->linkcount > source->player->maxlink)
						source->player->maxlink = source->player->linkcount;
					source->player->linktimer = 2*TICRATE;
				}
			}
			else
			{
				if (target->flags & MF_BOSS)
					score = 1000;
				else if ((target->flags & MF_ENEMY) && !(target->flags & MF_MISSILE))
				{
					mobj_t *scoremobj;
					UINT32 scorestate = mobjinfo[MT_SCORE].spawnstate;

					scoremobj = P_SpawnMobj(target->x, target->y, target->z + (target->height / 2), MT_SCORE);

					// On ground? No chain starts.
					if (!source->player->powers[pw_invulnerability] && P_IsObjectOnGround(source))
					{
						source->player->scoreadd = 0;
						score = 100;
					}
					// Mario Mode has Mario-like chain point values
					else if (mariomode) switch (++source->player->scoreadd)
					{
						case 1: score = 100;  break;
						case 2: score = 200;  scorestate += 1; break;
						case 3: score = 400;  scorestate += 5; break;
						case 4: score = 800;  scorestate += 6; break;
						case 5: score = 1000; scorestate += 3; break;
						case 6: score = 2000; scorestate += 7; break;
						case 7: score = 4000; scorestate += 8; break;
						case 8: score = 8000; scorestate += 9; break;
						default: // 1up for a chain this long
							if (modeattacking) // but 1ups don't exist in record attack!
							{ // So we just go back to 10k points.
								score = 10000; scorestate += 4; break;
							}
							P_GivePlayerLives(source->player, 1);
							P_PlayLivesJingle(source->player);
							scorestate += 10;
							break;
					}
					// More Sonic-like point system
					else switch (++source->player->scoreadd)
					{
						case 1:  score = 100;   break;
						case 2:  score = 200;   scorestate += 1; break;
						case 3:  score = 500;   scorestate += 2; break;
						case 4: case 5: case 6: case 7: case 8: case 9:
						case 10: case 11: case 12: case 13: case 14:
						         score = 1000;  scorestate += 3; break;
						default: score = 10000; scorestate += 4; break;
					}

					P_SetMobjState(scoremobj, scorestate);
				}
			}

			P_AddPlayerScore(source->player, score);
		}
	}

	// if a player avatar dies...
	if (target->player)
	{
		target->flags &= ~(MF_SOLID|MF_SHOOTABLE); // does not block
		P_UnsetThingPosition(target);
		target->flags |= MF_NOBLOCKMAP;
		P_SetThingPosition(target);

		if (!target->player->bot && !G_IsSpecialStage(gamemap)
		 && G_GametypeUsesLives())
		{
			target->player->lives -= 1; // Lose a life Tails 03-11-2000

			if (target->player->lives <= 0) // Tails 03-14-2000
			{
				if (P_IsLocalPlayer(target->player)/* && target->player == &players[consoleplayer] */)
				{
					S_StopMusic(); // Stop the Music! Tails 03-14-2000
					S_ChangeMusicInternal("gmover", false); // Yousa dead now, Okieday? Tails 03-14-2000
				}
			}
		}
		target->player->playerstate = PST_DEAD;

		if (target->player == &players[consoleplayer])
		{
			// don't die in auto map,
			// switch view prior to dying
			if (automapactive)
				AM_Stop();

			//added : 22-02-98: recenter view for next life...
			localaiming = 0;
		}
		if (target->player == &players[secondarydisplayplayer])
		{
			// added : 22-02-98: recenter view for next life...
			localaiming2 = 0;
		}

		//tag deaths handled differently in suicide cases. Don't count spectators!
		if (G_TagGametype()
		 && !(target->player->pflags & PF_TAGIT) && (!source || !source->player) && !(target->player->spectator))
		{
			// if you accidentally die before you run out of time to hide, ignore it.
			// allow them to try again, rather than sitting the whole thing out.
			if (leveltime >= hidetime * TICRATE)
			{
				if (gametype == GT_TAG)//suiciding in survivor makes you IT.
				{
					target->player->pflags |= PF_TAGIT;
					CONS_Printf(M_GetText("%s is now IT!\n"), player_names[target->player-players]); // Tell everyone who is it!
					P_CheckSurvivors();
				}
				else
				{
					if (!(target->player->pflags & PF_TAGGED))
					{
						//otherwise, increment the tagger's score.
						//in hide and seek, suiciding players are counted as found.
						INT32 w;

						for (w=0; w < MAXPLAYERS; w++)
						{
							if (players[w].pflags & PF_TAGIT)
								P_AddPlayerScore(&players[w], 100);
						}

						target->player->pflags |= PF_TAGGED;
						CONS_Printf(M_GetText("%s was found!\n"), player_names[target->player-players]);
						P_CheckSurvivors();
					}
				}
			}
		}
	}

	if (source && target && target->player && source->player)
		P_PlayVictorySound(source); // Killer laughs at you. LAUGHS! BWAHAHAHA!

	// Drop stuff.
	// This determines the kind of object spawned
	// during the death frame of a thing.
	if (!mariomode // Don't show birds, etc. in Mario Mode Tails 12-23-2001
	&& target->flags & MF_ENEMY)
	{
		if (cv_soniccd.value)
			item = MT_SEED;
		else
		{
			INT32 prandom;

			switch (target->type)
			{
				case MT_REDCRAWLA:
				case MT_GOLDBUZZ:
				case MT_SKIM:
				case MT_UNIDUS:
					item = MT_BUNNY;
					break;

				case MT_BLUECRAWLA:
				case MT_JETTBOMBER:
				case MT_GFZFISH:
					item = MT_BIRD;
					break;

				case MT_JETTGUNNER:
				case MT_CRAWLACOMMANDER:
				case MT_REDBUZZ:
				case MT_DETON:
					item = MT_MOUSE;
					break;

				case MT_GSNAPPER:
				case MT_EGGGUARD:
				case MT_SPRINGSHELL:
					item = MT_COW;
					break;

				case MT_MINUS:
				case MT_VULTURE:
				case MT_POINTY:
				case MT_YELLOWSHELL:
					item = MT_CHICKEN;
					break;

				case MT_AQUABUZZ:
					item = MT_REDBIRD;
					break;

				default:
					if (target->info->doomednum)
						prandom = target->info->doomednum%5; // "Random" animal for new enemies.
					else
						prandom = P_RandomKey(5); // No placable object, just use a random number.

					switch(prandom)
					{
						default: item = MT_BUNNY; break;
						case 1: item = MT_BIRD; break;
						case 2: item = MT_MOUSE; break;
						case 3: item = MT_COW; break;
						case 4: item = MT_CHICKEN; break;
					}
					break;
			}
		}

		mo = P_SpawnMobj(target->x, target->y, target->z + (target->height / 2) - FixedMul(mobjinfo[item].height / 2, target->scale), item);
		mo->destscale = target->scale;
		P_SetScale(mo, mo->destscale);
	}
	// Other death animation effects
	else switch(target->type)
	{
		case MT_BOUNCEPICKUP:
		case MT_RAILPICKUP:
		case MT_AUTOPICKUP:
		case MT_EXPLODEPICKUP:
		case MT_SCATTERPICKUP:
		case MT_GRENADEPICKUP:
			P_SetObjectMomZ(target, FRACUNIT, false);
			target->fuse = target->info->damage;
			break;

		case MT_EGGTRAP:
			// Time for birdies! Yaaaaaaaay!
			target->fuse = TICRATE*2;
			break;

		case MT_PLAYER:
			target->fuse = TICRATE*3; // timer before mobj disappears from view (even if not an actual player)
			target->momx = target->momy = target->momz = 0;
			if (!(source && source->type == MT_NULL && source->threshold == 42)) // Don't jump up when drowning
				P_SetObjectMomZ(target, 14*FRACUNIT, false);

			if (source && source->type == MT_NULL && source->threshold == 42) // drowned
				S_StartSound(target, sfx_drown);
			else if (source && (source->type == MT_SPIKE || (source->type == MT_NULL && source->threshold == 43))) // Spikes
				S_StartSound(target, sfx_spkdth);
			else
				P_PlayDeathSound(target);
			break;
		default:
			break;
	}

	// Enemy drops that ALWAYS occur regardless of mode
	if (target->type == MT_AQUABUZZ) // Additionally spawns breathable bubble for players to get
	{
		if (inflictor && inflictor->player // did a player kill you? Spawn relative to the player so he's bound to get it
		&& P_AproxDistance(inflictor->x - target->x, inflictor->y - target->y) <= inflictor->radius + target->radius + FixedMul(8*FRACUNIT, inflictor->scale) // close enough?
		&& inflictor->z <= target->z + target->height + FixedMul(8*FRACUNIT, inflictor->scale)
		&& inflictor->z + inflictor->height >= target->z - FixedMul(8*FRACUNIT, inflictor->scale))
			mo = P_SpawnMobj(inflictor->x + inflictor->momx, inflictor->y + inflictor->momy, inflictor->z + (inflictor->height / 2) + inflictor->momz, MT_EXTRALARGEBUBBLE);
		else
			mo = P_SpawnMobj(target->x, target->y, target->z, MT_EXTRALARGEBUBBLE);
		mo->destscale = target->scale;
		P_SetScale(mo, mo->destscale);
	}
	else if (target->type == MT_YELLOWSHELL) // Spawns a spring that falls to the ground
	{
		mobjtype_t spawnspring = MT_YELLOWSPRING;
		fixed_t spawnheight = target->z;
		if (!(target->eflags & MFE_VERTICALFLIP))
			spawnheight += target->height;

		mo = P_SpawnMobj(target->x, target->y, spawnheight, spawnspring);
		mo->destscale = target->scale;
		P_SetScale(mo, mo->destscale);

		if (target->flags2 & MF2_OBJECTFLIP)
			mo->flags2 |= MF2_OBJECTFLIP;
	}

	if (target->type == MT_EGGMOBILE3)
	{
		thinker_t *th;
		UINT32 i = 0; // to check how many clones we've removed

		// scan the thinkers to make sure all the old pinch dummies are gone on death
		// this can happen if the boss was hurt earlier than expected
		for (th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if (th->function.acp1 != (actionf_p1)P_MobjThinker)
				continue;

			mo = (mobj_t *)th;
			if (mo->type == (mobjtype_t)target->info->mass && mo->tracer == target)
			{
				P_RemoveMobj(mo);
				i++;
			}
			if (i == 2) // we've already removed 2 of these, let's stop now
				break;
		}
	}

	if (target->type == MT_SPIKE && inflictor && target->info->deathstate != S_NULL)
	{
		const fixed_t x=target->x,y=target->y,z=target->z;
		const fixed_t scale=target->scale;
		const boolean flip=(target->eflags & MFE_VERTICALFLIP) == MFE_VERTICALFLIP;
		S_StartSound(target,target->info->deathsound);

		P_SetMobjState(target, target->info->deathstate);
		target->health = 0;
		target->angle = inflictor->angle + ANGLE_90;
		P_UnsetThingPosition(target);
		target->flags = MF_NOCLIP;
		target->x += P_ReturnThrustX(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
		target->y += P_ReturnThrustY(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
		if (flip)
			target->z -= FixedMul(12*FRACUNIT, target->scale);
		else
			target->z += FixedMul(12*FRACUNIT, target->scale);
		P_SetThingPosition(target);
		P_InstaThrust(target,target->angle,FixedMul(2*FRACUNIT, target->scale));
		target->momz = FixedMul(7*FRACUNIT, target->scale);
		if (flip)
			target->momz = -target->momz;

		if (flip)
		{
			target = P_SpawnMobj(x,y,z-FixedMul(12*FRACUNIT, target->scale),MT_SPIKE);
			target->eflags |= MFE_VERTICALFLIP;
		}
		else
			target = P_SpawnMobj(x,y,z+FixedMul(12*FRACUNIT, target->scale),MT_SPIKE);
		P_SetMobjState(target, target->info->deathstate);
		target->health = 0;
		target->angle = inflictor->angle - ANGLE_90;
		target->destscale = scale;
		P_SetScale(target, scale);
		P_UnsetThingPosition(target);
		target->flags = MF_NOCLIP;
		target->x += P_ReturnThrustX(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
		target->y += P_ReturnThrustY(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
		P_SetThingPosition(target);
		P_InstaThrust(target,target->angle,FixedMul(2*FRACUNIT, target->scale));
		target->momz = FixedMul(7*FRACUNIT, target->scale);
		if (flip)
			target->momz = -target->momz;

		if (target->info->xdeathstate != S_NULL)
		{
			target = P_SpawnMobj(x,y,z,MT_SPIKE);
			if (flip)
				target->eflags |= MFE_VERTICALFLIP;
			P_SetMobjState(target, target->info->xdeathstate);
			target->health = 0;
			target->angle = inflictor->angle + ANGLE_90;
			target->destscale = scale;
			P_SetScale(target, scale);
			P_UnsetThingPosition(target);
			target->flags = MF_NOCLIP;
			target->x += P_ReturnThrustX(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
			target->y += P_ReturnThrustY(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
			P_SetThingPosition(target);
			P_InstaThrust(target,target->angle,FixedMul(4*FRACUNIT, target->scale));
			target->momz = FixedMul(6*FRACUNIT, target->scale);
			if (flip)
				target->momz = -target->momz;

			target = P_SpawnMobj(x,y,z,MT_SPIKE);
			if (flip)
				target->eflags |= MFE_VERTICALFLIP;
			P_SetMobjState(target, target->info->xdeathstate);
			target->health = 0;
			target->angle = inflictor->angle - ANGLE_90;
			target->destscale = scale;
			P_SetScale(target, scale);
			P_UnsetThingPosition(target);
			target->flags = MF_NOCLIP;
			target->x += P_ReturnThrustX(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
			target->y += P_ReturnThrustY(target, target->angle, FixedMul(8*FRACUNIT, target->scale));
			P_SetThingPosition(target);
			P_InstaThrust(target,target->angle,FixedMul(4*FRACUNIT, target->scale));
			target->momz = FixedMul(6*FRACUNIT, target->scale);
			if (flip)
				target->momz = -target->momz;
		}
	}
	else if (target->player)
		P_SetPlayerMobjState(target, target->info->deathstate);
	else
#ifdef DEBUG_NULL_DEATHSTATE
		P_SetMobjState(target, S_NULL);
#else
		P_SetMobjState(target, target->info->deathstate);
#endif

	/** \note For player, the above is redundant because of P_SetMobjState (target, S_PLAY_DIE1)
	   in P_DamageMobj()
	   Graue 12-22-2003 */
}

static inline void P_NiGHTSDamage(mobj_t *target, mobj_t *source)
{
	player_t *player = target->player;
	tic_t oldnightstime = player->nightstime;

	if (!player->powers[pw_flashing]
		&& !(player->pflags & PF_GODMODE))
	{
		angle_t fa;

		player->angle_pos = player->old_angle_pos;
		player->speed /= 5;
		player->flyangle += 180; // Shuffle's BETTERNIGHTSMOVEMENT?
		player->flyangle %= 360;

		if (gametype == GT_RACE || gametype == GT_COMPETITION)
			player->drillmeter -= 5*20;
		else
		{
			if (source && source->player)
			{
				if (player->nightstime > 20*TICRATE)
					player->nightstime -= 20*TICRATE;
				else
					player->nightstime = 1;
			}
			else
			{
				if (player->nightstime > 5*TICRATE)
					player->nightstime -= 5*TICRATE;
				else
					player->nightstime = 1;
			}
		}

		if (player->pflags & PF_TRANSFERTOCLOSEST)
		{
			target->momx = -target->momx;
			target->momy = -target->momy;
		}
		else
		{
			fa = player->old_angle_pos>>ANGLETOFINESHIFT;

			target->momx = FixedMul(FINECOSINE(fa),target->target->radius);
			target->momy = FixedMul(FINESINE(fa),target->target->radius);
		}

		player->powers[pw_flashing] = flashingtics;
		P_SetMobjState(target->tracer, S_NIGHTSHURT1);
		S_StartSound(target, sfx_nghurt);

		if (oldnightstime > 10*TICRATE
			&& player->nightstime < 10*TICRATE)
		{
			//S_StartSound(NULL, sfx_timeup); // that creepy "out of time" music from NiGHTS. Dummied out, as some on the dev team thought it wasn't Sonic-y enough (Mystic, notably). Uncomment to restore. -SH
			S_ChangeMusicInternal("drown",false);
		}
	}
}

static inline boolean P_TagDamage(mobj_t *target, mobj_t *inflictor, mobj_t *source, INT32 damage)
{
	player_t *player = target->player;
	(void)damage; //unused parm

	// If flashing or invulnerable, ignore the tag,
	if (player->powers[pw_flashing] || player->powers[pw_invulnerability])
		return false;

	// Ignore IT players shooting each other, unless friendlyfire is on.
	if ((player->pflags & PF_TAGIT && !(cv_friendlyfire.value &&
		source && source->player && source->player->pflags & PF_TAGIT)))
		return false;

	// Don't allow any damage before the round starts.
	if (leveltime <= hidetime * TICRATE)
		return false;

	// Don't allow players on the same team to hurt one another,
	// unless cv_friendlyfire is on.
	if (!cv_friendlyfire.value && (player->pflags & PF_TAGIT) == (source->player->pflags & PF_TAGIT))
	{
		if (!(inflictor->flags & MF_FIRE))
			P_GivePlayerRings(player, 1);
		if (inflictor->flags2 & MF2_BOUNCERING)
			inflictor->fuse = 0; // bounce ring disappears at -1 not 0
		return false;
	}

	// The tag occurs so long as you aren't shooting another tagger with friendlyfire on.
	if (source->player->pflags & PF_TAGIT && !(player->pflags & PF_TAGIT))
	{
		P_AddPlayerScore(source->player, 100); //award points to tagger.
		P_HitDeathMessages(player, inflictor, source);

		if (gametype == GT_TAG) //survivor
		{
			player->pflags |= PF_TAGIT; //in survivor, the player becomes IT and helps hunt down the survivors.
			CONS_Printf(M_GetText("%s is now IT!\n"), player_names[player-players]); // Tell everyone who is it!
		}
		else
		{
			player->pflags |= PF_TAGGED; //in hide and seek, the player is tagged and stays stationary.
			CONS_Printf(M_GetText("%s was found!\n"), player_names[player-players]); // Tell everyone who is it!
		}

		//checks if tagger has tagged all players, if so, end round early.
		P_CheckSurvivors();
	}

	P_DoPlayerPain(player, source, inflictor);

	// Check for a shield
	if (player->powers[pw_shield])
	{
		P_RemoveShield(player);
		S_StartSound(target, sfx_shldls);
		return true;
	}

	if (target->health <= 1) // Death
	{
		P_PlayDeathSound(target);
		P_PlayVictorySound(source); // Killer laughs at you! LAUGHS! BWAHAHAHHAHAA!!
	}
	else if (target->health > 1) // Ring loss
	{
		P_PlayRinglossSound(target);
		P_PlayerRingBurst(player, player->mo->health - 1);
	}

	if (inflictor && ((inflictor->flags & MF_MISSILE) || inflictor->player) && player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds]))
	{
		player->health -= 10;
		if (player->health < 2)
			player->health = 2;
		target->health = player->health;
	}
	else
		player->health = target->health = 1;

	return true;
}

static inline boolean P_PlayerHitsPlayer(mobj_t *target, mobj_t *inflictor, mobj_t *source, INT32 damage)
{
	player_t *player = target->player;

	// You can't kill yourself, idiot...
	if (source == target)
		return false;

	// In COOP/RACE/CHAOS, you can't hurt other players unless cv_friendlyfire is on
	if (!cv_friendlyfire.value && (G_PlatformGametype()))
		return false;

	// Tag handling
	if (G_TagGametype())
		return P_TagDamage(target, inflictor, source, damage);
	else if (G_GametypeHasTeams()) // CTF + Team Match
	{
		// Don't allow players on the same team to hurt one another,
		// unless cv_friendlyfire is on.
		if (!cv_friendlyfire.value && target->player->ctfteam == source->player->ctfteam)
		{
			if (!(inflictor->flags & MF_FIRE))
				P_GivePlayerRings(target->player, 1);
			if (inflictor->flags2 & MF2_BOUNCERING)
				inflictor->fuse = 0; // bounce ring disappears at -1 not 0

			return false;
		}
	}

	// Add pity.
	if (!player->powers[pw_flashing] && !player->powers[pw_invulnerability] && !player->powers[pw_super]
	&& source->player->score > player->score)
		player->pity++;

	return true;
}

static void P_KillPlayer(player_t *player, mobj_t *source, INT32 damage)
{
	player->pflags &= ~(PF_CARRIED|PF_SLIDING|PF_ITEMHANG|PF_MACESPIN|PF_ROPEHANG|PF_NIGHTSMODE);

	// Burst weapons and emeralds in Match/CTF only
	if (source && (gametype == GT_MATCH || gametype == GT_TEAMMATCH || gametype == GT_CTF))
	{
		P_PlayerRingBurst(player, player->health - 1);
		P_PlayerEmeraldBurst(player, false);
	}

	// Get rid of shield
	player->powers[pw_shield] = SH_NONE;
	player->mo->color = player->skincolor;

	// Get rid of emeralds
	player->powers[pw_emeralds] = 0;

	P_ForceFeed(player, 40, 10, TICRATE, 40 + min(damage, 100)*2);

	P_ResetPlayer(player);

	P_SetPlayerMobjState(player->mo, player->mo->info->deathstate);
	if (gametype == GT_CTF && (player->gotflag & (GF_REDFLAG|GF_BLUEFLAG)))
	{
		P_PlayerFlagBurst(player, false);
		if (source && source->player)
		{
			// Award no points when players shoot each other when cv_friendlyfire is on.
			if (!G_GametypeHasTeams() || !(source->player->ctfteam == player->ctfteam && source != player->mo))
				P_AddPlayerScore(source->player, 25);
		}
	}
	if (source && source->player && !player->powers[pw_super]) //don't score points against super players
	{
		// Award no points when players shoot each other when cv_friendlyfire is on.
		if (!G_GametypeHasTeams() || !(source->player->ctfteam == player->ctfteam && source != player->mo))
			P_AddPlayerScore(source->player, 100);
	}

	// If the player was super, tell them he/she ain't so super nomore.
	if (gametype != GT_COOP && player->powers[pw_super])
	{
		S_StartSound(NULL, sfx_s3k66); //let all players hear it.
		HU_SetCEchoFlags(0);
		HU_SetCEchoDuration(5);
		HU_DoCEcho(va("%s\\is no longer super.\\\\\\\\", player_names[player-players]));
	}
}

static void P_SuperDamage(player_t *player, mobj_t *inflictor, mobj_t *source, INT32 damage)
{
	fixed_t fallbackspeed;
	angle_t ang;

	P_ForceFeed(player, 40, 10, TICRATE, 40 + min(damage, 100)*2);

	if (player->mo->eflags & MFE_VERTICALFLIP)
		player->mo->z--;
	else
		player->mo->z++;

	if (player->mo->eflags & MFE_UNDERWATER)
		P_SetObjectMomZ(player->mo, FixedDiv(10511*FRACUNIT,2600*FRACUNIT), false);
	else
		P_SetObjectMomZ(player->mo, FixedDiv(69*FRACUNIT,10*FRACUNIT), false);

	ang = R_PointToAngle2(inflictor->x,	inflictor->y, player->mo->x, player->mo->y);

	// explosion and rail rings send you farther back, making it more difficult
	// to recover
	if (inflictor->flags2 & MF2_SCATTER && source)
	{
		fixed_t dist = P_AproxDistance(P_AproxDistance(source->x-player->mo->x, source->y-player->mo->y), source->z-player->mo->z);

		dist = FixedMul(128*FRACUNIT, inflictor->scale) - dist/4;

		if (dist < FixedMul(4*FRACUNIT, inflictor->scale))
			dist = FixedMul(4*FRACUNIT, inflictor->scale);

		fallbackspeed = dist;
	}
	else if (inflictor->flags2 & MF2_EXPLOSION)
	{
		if (inflictor->flags2 & MF2_RAILRING)
			fallbackspeed = FixedMul(28*FRACUNIT, inflictor->scale); // 7x
		else
			fallbackspeed = FixedMul(20*FRACUNIT, inflictor->scale); // 5x
	}
	else if (inflictor->flags2 & MF2_RAILRING)
		fallbackspeed = FixedMul(16*FRACUNIT, inflictor->scale); // 4x
	else
		fallbackspeed = FixedMul(4*FRACUNIT, inflictor->scale); // the usual amount of force

	P_InstaThrust(player->mo, ang, fallbackspeed);

	if (player->charflags & SF_SUPERANIMS)
		P_SetPlayerMobjState(player->mo, S_PLAY_SUPERHIT);
	else
		P_SetPlayerMobjState(player->mo, player->mo->info->painstate);

	P_ResetPlayer(player);

	if (player->timeshit != UINT8_MAX)
		++player->timeshit;
}

void P_RemoveShield(player_t *player)
{
	if (player->powers[pw_shield] & SH_FORCE)
	{ // Multi-hit
		if ((player->powers[pw_shield] & 0xFF) == 0)
			player->powers[pw_shield] &= ~SH_FORCE;
		else
			player->powers[pw_shield]--;
	}
	else if ((player->powers[pw_shield] & SH_NOSTACK) == SH_NONE)
	{ // Second layer shields
		player->powers[pw_shield] = SH_NONE;
		// Reset fireflower
		if (!player->powers[pw_super])
		{
			player->mo->color = player->skincolor;
			G_GhostAddColor(GHC_NORMAL);
		}
	}
	else if ((player->powers[pw_shield] & SH_NOSTACK) == SH_BOMB) // Give them what's coming to them!
	{
		P_BlackOw(player); // BAM!
		player->pflags |= PF_JUMPDOWN;
	}
	else
		player->powers[pw_shield] = player->powers[pw_shield] & SH_STACK;
}

static void P_ShieldDamage(player_t *player, mobj_t *inflictor, mobj_t *source, INT32 damage)
{
	// Must do pain first to set flashing -- P_RemoveShield can cause damage
	P_DoPlayerPain(player, source, inflictor);

	P_RemoveShield(player);

	P_ForceFeed(player, 40, 10, TICRATE, 40 + min(damage, 100)*2);

	if (source && (source->type == MT_SPIKE || (source->type == MT_NULL && source->threshold == 43))) // spikes
		S_StartSound(player->mo, sfx_spkdth);
	else
		S_StartSound (player->mo, sfx_shldls); // Ba-Dum! Shield loss.

	if (gametype == GT_CTF && (player->gotflag & (GF_REDFLAG|GF_BLUEFLAG)))
	{
		P_PlayerFlagBurst(player, false);
		if (source && source->player)
		{
			// Award no points when players shoot each other when cv_friendlyfire is on.
			if (!G_GametypeHasTeams() || !(source->player->ctfteam == player->ctfteam && source != player->mo))
				P_AddPlayerScore(source->player, 25);
		}
	}
	if (source && source->player && !player->powers[pw_super]) //don't score points against super players
	{
		// Award no points when players shoot each other when cv_friendlyfire is on.
		if (!G_GametypeHasTeams() || !(source->player->ctfteam == player->ctfteam && source != player->mo))
			P_AddPlayerScore(source->player, cv_match_scoring.value == 1 ? 25 : 50);
	}
}

static void P_RingDamage(player_t *player, mobj_t *inflictor, mobj_t *source, INT32 damage)
{
	if (!(inflictor && ((inflictor->flags & MF_MISSILE) || inflictor->player) && player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds])))
	{
		P_DoPlayerPain(player, source, inflictor);

		P_ForceFeed(player, 40, 10, TICRATE, 40 + min(damage, 100)*2);

		if (source && (source->type == MT_SPIKE || (source->type == MT_NULL && source->threshold == 43))) // spikes
			S_StartSound(player->mo, sfx_spkdth);
	}

	if (source && source->player && !player->powers[pw_super]) //don't score points against super players
	{
		// Award no points when players shoot each other when cv_friendlyfire is on.
		if (!G_GametypeHasTeams() || !(source->player->ctfteam == player->ctfteam && source != player->mo))
			P_AddPlayerScore(source->player, 50);
	}

	if (gametype == GT_CTF && (player->gotflag & (GF_REDFLAG|GF_BLUEFLAG)))
	{
		P_PlayerFlagBurst(player, false);
		if (source && source->player)
		{
			// Award no points when players shoot each other when cv_friendlyfire is on.
			if (!G_GametypeHasTeams() || !(source->player->ctfteam == player->ctfteam && source != player->mo))
				P_AddPlayerScore(source->player, 25);
		}
	}

	// Ring loss sound plays despite hitting spikes
	P_PlayRinglossSound(player->mo); // Ringledingle!
}

/** Damages an object, which may or may not be a player.
  * For melee attacks, source and inflictor are the same.
  *
  * \param target    The object being damaged.
  * \param inflictor The thing that caused the damage: creature, missile,
  *                  gargoyle, and so forth. Can be NULL in the case of
  *                  environmental damage, such as slime or crushing.
  * \param source    The creature or person responsible. For example, if a
  *                  player is hit by a ring, the player who shot it. In some
  *                  cases, the target will go after this object after
  *                  receiving damage. This can be NULL.
  * \param damage    Amount of damage to be dealt. 10000 is instant death.
  * \return True if the target sustained damage, otherwise false.
  * \todo Clean up this mess, split into multiple functions.
  * \todo Get rid of the magic number 10000.
  * \sa P_KillMobj
  */
boolean P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source, INT32 damage)
{
	player_t *player;
#ifdef HAVE_BLUA
	boolean force = false;
#else
	static const boolean force = false;
#endif

	if (objectplacing)
		return false;

	if (target->health <= 0)
		return false;

	// Spectator handling
	if (netgame)
	{
		if (damage == 42000 && target->player && target->player->spectator)
			damage = 10000;
		else if (target->player && target->player->spectator)
			return false;

		if (source && source->player && source->player->spectator)
			return false;
	}

#ifdef HAVE_BLUA
	// Everything above here can't be forced.
	if (!metalrecording)
	{
		UINT8 shouldForce = LUAh_ShouldDamage(target, inflictor, source, damage);
		if (P_MobjWasRemoved(target))
			return (shouldForce == 1); // mobj was removed
		if (shouldForce == 1)
			force = true;
		else if (shouldForce == 2)
			return false;
	}
#endif

	if (!force)
	{
		if (!(target->flags & MF_SHOOTABLE))
			return false; // shouldn't happen...

		if (target->type == MT_BLACKEGGMAN)
			return false;

		// Make sure that boxes cannot be popped by enemies, red rings, etc.
		if (target->flags & MF_MONITOR && ((!source || !source->player || source->player->bot) || (inflictor && !inflictor->player)))
			return false;
	}

	if (target->flags2 & MF2_SKULLFLY)
		target->momx = target->momy = target->momz = 0;

	if (!force)
	{
		// Special case for team ring boxes
		if (target->type == MT_REDRINGBOX && !(source->player->ctfteam == 1))
			return false;

		if (target->type == MT_BLUERINGBOX && !(source->player->ctfteam == 2))
			return false;
	}

	// Special case for Crawla Commander
	if (target->type == MT_CRAWLACOMMANDER)
	{
		if (!force && target->fuse) // Invincible
			return false;

#ifdef HAVE_BLUA
		if (LUAh_MobjDamage(target, inflictor, source, damage) || P_MobjWasRemoved(target))
			return true;
#endif

		if (target->health > 1)
		{
			if (target->info->painsound)
				S_StartSound(target, target->info->painsound);

			target->fuse = TICRATE/2;
			target->flags2 |= MF2_FRET;
		}
		else
		{
			target->flags |= MF_NOGRAVITY;
			target->fuse = 0;
		}

		target->momx = target->momy = target->momz = 0;

		P_InstaThrust(target, target->angle-ANGLE_180, FixedMul(5*FRACUNIT, target->scale));
	}
	else if (target->flags & MF_BOSS)
	{
		if (!force && target->flags2 & MF2_FRET) // Currently flashing from being hit
			return false;

#ifdef HAVE_BLUA
		if (LUAh_MobjDamage(target, inflictor, source, damage) || P_MobjWasRemoved(target))
			return true;
#endif

		if (target->health > 1)
			target->flags2 |= MF2_FRET;
	}
#ifdef HAVE_BLUA
	else if (target->flags & MF_ENEMY)
	{
		if (LUAh_MobjDamage(target, inflictor, source, damage) || P_MobjWasRemoved(target))
			return true;
	}
#endif

	player = target->player;

	/*if (player) // Player is the target
	{
		if (!force)
		{
			if (player->exiting)
				return false;

			if (!(target->player->pflags & (PF_NIGHTSMODE|PF_NIGHTSFALL)) && (maptol & TOL_NIGHTS))
				return false;
		}

		if (player->pflags & PF_NIGHTSMODE) // NiGHTS damage handling
		{
			if (!force)
			{
				if (source == target)
					return false; // Don't hit yourself with your own paraloop, baka
				if (source && source->player && !cv_friendlyfire.value
				&& (gametype == GT_COOP
				|| (G_GametypeHasTeams() && target->player->ctfteam == source->player->ctfteam)))
					return false; // Don't run eachother over in special stages and team games and such
			}
#ifdef HAVE_BLUA
			if (LUAh_MobjDamage(target, inflictor, source, damage))
				return true;
#endif
			P_NiGHTSDamage(target, source); // -5s :(
			return true;
		}

		if (!force && inflictor && (inflictor->flags & MF_FIRE))
		{
			if ((player->powers[pw_shield] & SH_NOSTACK) == SH_ELEMENTAL)
				return false; // Invincible to fire objects

			if (G_PlatformGametype() && source && source->player)
				return false; // Don't get hurt by fire generated from friends.
		}

		// Sudden-Death mode
		if (source && source->type == MT_PLAYER)
		{
			if ((gametype == GT_MATCH || gametype == GT_TEAMMATCH || gametype == GT_CTF) && cv_suddendeath.value
				&& !player->powers[pw_flashing] && !player->powers[pw_invulnerability])
				damage = 10000;
		}

		// Player hits another player
		if (!force && source && source->player)
		{
			if (!P_PlayerHitsPlayer(target, inflictor, source, damage))
				return false;
		}

		if (!force && player->pflags & PF_GODMODE)
			return false;

		// Instant-Death
		if (damage == 10000)
			P_KillPlayer(player, source, damage);
		else if (metalrecording)
		{
			if (!inflictor)
				inflictor = source;
			if (inflictor && inflictor->flags & MF_ENEMY)
			{ // Metal Sonic destroy enemy !!
				P_KillMobj(inflictor, NULL, target);
				return false;
			}
			else if (inflictor && inflictor->flags & MF_MISSILE)
				return false; // Metal Sonic walk through flame !!
			else
			{ // Oh no! Metal Sonic is hit !!
				P_ShieldDamage(player, inflictor, source, damage);
				return true;
			}
		}
		else if (player->powers[pw_invulnerability] || player->powers[pw_flashing] // ignore bouncing & such in invulnerability
			|| (player->powers[pw_super] && !(ALL7EMERALDS(player->powers[pw_emeralds]) && inflictor && ((inflictor->flags & MF_MISSILE) || inflictor->player))))
		{
			if (force || (inflictor && (inflictor->flags & MF_MISSILE)
				&& (inflictor->flags2 & MF2_SUPERFIRE)
				&& player->powers[pw_super]))
			{
#ifdef HAVE_BLUA
				if (!LUAh_MobjDamage(target, inflictor, source, damage))
#endif
					P_SuperDamage(player, inflictor, source, damage);
				return true;
			}
			else
				return false;
		}
#ifdef HAVE_BLUA
		else if (LUAh_MobjDamage(target, inflictor, source, damage))
			return true;
#endif
		else if (!player->powers[pw_super] && (player->powers[pw_shield] || player->bot))  //If One-Hit Shield
		{
			P_ShieldDamage(player, inflictor, source, damage);
			damage = 0;
		}
		else if (player->mo->health > 1) // No shield but have rings.
		{
			//damage = player->mo->health - 1;
			P_RingDamage(player, inflictor, source, damage);
		}
		else // No shield, no rings, no invincibility.
		{
			// To reduce griefing potential, don't allow players to be killed
			// by friendly fire. Spilling their rings and other items is enough.
			if (force || !(G_GametypeHasTeams()
				&& source && source->player && (source->player->ctfteam == player->ctfteam)
				&& cv_friendlyfire.value))
			{
				damage = 1;
				P_KillPlayer(player, source, damage);
			}
			else
			{
				damage = 0;
				P_ShieldDamage(player, inflictor, source, damage);
			}
		}

		if (inflictor && ((inflictor->flags & MF_MISSILE) || inflictor->player) && player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds]))
		{
			if (player->powers[pw_shield])
			{
				P_RemoveShield(player);
				return true;
			}
			else
			{
				player->health -= (10 * (1 << (INT32)(player->powers[pw_super] / 10500)));
				if (player->health < 2)
					player->health = 2;
			}

			if (gametype == GT_CTF && (player->gotflag & (GF_REDFLAG|GF_BLUEFLAG)))
				P_PlayerFlagBurst(player, false);
		}
		else
		{
			player->health -= damage; // mirror mobj health here
			if (damage < 10000)
			{
				target->player->powers[pw_flashing] = flashingtics;
				if (damage > 0) // don't spill emeralds/ammo/panels for shield damage
					P_PlayerRingBurst(player, damage);
			}
		}

		if (player->health < 0)
			player->health = 0;

		P_HitDeathMessages(player, inflictor, source);

		P_ForceFeed(player, 40, 10, TICRATE, 40 + min(damage, 100)*2);
	}*/

	// Killing dead. Just for kicks.
	// Require source and inflictor be player.  Don't hurt for firing rings.
	if (cv_killingdead.value && (source && source->player) && (inflictor && inflictor->player) && P_RandomChance(5*FRACUNIT/16))
		P_DamageMobj(source, target, target, 1);

	// do the damage
	if (player && player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds]) && inflictor && ((inflictor->flags & MF_MISSILE) || inflictor->player))
	{
		target->health -= (10 * (1 << (INT32)(player->powers[pw_super] / 10500)));
		if (target->health < 2)
			target->health = 2;
	}
	else
		target->health -= damage;

	if (source && source->player && target)
		G_GhostAddHit(target);

	if (target->health <= 0)
	{
		P_KillMobj(target, inflictor, source);
		return true;
	}

	if (player)
	{
		if (!(player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds])))
			P_ResetPlayer(target->player);
	}
	else
		switch (target->type)
		{
		case MT_EGGMOBILE2: // egg slimer
			if (target->health < target->info->damage) // in pinch phase
			{
				P_SetMobjState(target, target->info->meleestate); // go to pinch pain state
				break;
			}
			/* FALLTHRU */
		default:
			P_SetMobjState(target, target->info->painstate);
			break;
		}

	target->reactiontime = 0; // we're awake now...

	if (source && source != target)
	{
		// if not intent on another player,
		// chase after this one
		P_SetTarget(&target->target, source);
		if (target->state == &states[target->info->spawnstate] && target->info->seestate != S_NULL)
		{
			if (player)
			{
				if (!(player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds])))
					P_SetPlayerMobjState(target, target->info->seestate);
			}
			else
				P_SetMobjState(target, target->info->seestate);
		}
	}

	return true;
}

/** Spills an injured player's rings.
  *
  * \param player    The player who is losing rings.
  * \param num_rings Number of rings lost. A maximum of 32 rings will be
  *                  spawned.
  * \sa P_PlayerFlagBurst
  */
void P_PlayerRingBurst(player_t *player, INT32 num_rings)
{
	INT32 i;
	mobj_t *mo;
	angle_t fa;
	fixed_t ns;
	fixed_t z;

	// Better safe than sorry.
	if (!player)
		return;

	// If no health, don't spawn ring!
	if (player->mo->health <= 1)
		num_rings = 0;

	if (num_rings > 32 && !(player->pflags & PF_NIGHTSFALL))
		num_rings = 32;

	if (player->powers[pw_emeralds])
		P_PlayerEmeraldBurst(player, false);

	// Spill weapons first
	if (player->ringweapons)
		P_PlayerWeaponPanelBurst(player);

	// Spill the ammo
	P_PlayerWeaponAmmoBurst(player);

	for (i = 0; i < num_rings; i++)
	{
		INT32 objType = mobjinfo[MT_RING].reactiontime;
		if (mariomode)
			objType = mobjinfo[MT_COIN].reactiontime;

		z = player->mo->z;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - mobjinfo[objType].height;

		mo = P_SpawnMobj(player->mo->x, player->mo->y, z, objType);

		mo->fuse = 8*TICRATE;
		P_SetTarget(&mo->target, player->mo);

		mo->destscale = player->mo->scale;
		P_SetScale(mo, player->mo->scale);

		// Angle offset by player angle, then slightly offset by amount of rings
		fa = ((i*FINEANGLES/16) + (player->mo->angle>>ANGLETOFINESHIFT) - ((num_rings-1)*FINEANGLES/32)) & FINEMASK;

		// Make rings spill out around the player in 16 directions like SA, but spill like Sonic 2.
		// Technically a non-SA way of spilling rings. They just so happen to be a little similar.
		if (player->pflags & PF_NIGHTSFALL)
		{
			ns = FixedMul(((i*FRACUNIT)/16)+2*FRACUNIT, mo->scale);
			mo->momx = FixedMul(FINECOSINE(fa),ns);

			if (!(twodlevel || (player->mo->flags2 & MF2_TWOD)))
				mo->momy = FixedMul(FINESINE(fa),ns);

			P_SetObjectMomZ(mo, 8*FRACUNIT, false);
			mo->fuse = 20*TICRATE; // Adjust fuse for NiGHTS
		}
		else
		{
			fixed_t momxy, momz; // base horizonal/vertical thrusts

			if (i > 15)
			{
				momxy = 3*FRACUNIT;
				momz = 4*FRACUNIT;
			}
			else
			{
				momxy = 2*FRACUNIT;
				momz = 3*FRACUNIT;
			}

			ns = FixedMul(FixedMul(momxy, FRACUNIT + FixedDiv(player->losstime<<FRACBITS, 10*TICRATE<<FRACBITS)), mo->scale);
			mo->momx = FixedMul(FINECOSINE(fa),ns);

			if (!(twodlevel || (player->mo->flags2 & MF2_TWOD)))
				mo->momy = FixedMul(FINESINE(fa),ns);

			ns = FixedMul(momz, FRACUNIT + FixedDiv(player->losstime<<FRACBITS, 10*TICRATE<<FRACBITS));
			P_SetObjectMomZ(mo, ns, false);

			if (i & 1)
				P_SetObjectMomZ(mo, ns, true);
		}
		if (player->mo->eflags & MFE_VERTICALFLIP)
			mo->momz *= -1;
	}

	player->losstime += 10*TICRATE;

	if (P_IsObjectOnGround(player->mo))
		player->pflags &= ~PF_NIGHTSFALL;

	return;
}

void P_PlayerWeaponPanelBurst(player_t *player)
{
	mobj_t *mo;
	angle_t fa;
	fixed_t ns;
	INT32 i;
	fixed_t z;

	INT32 num_weapons = M_CountBits((UINT32)player->ringweapons, NUM_WEAPONS-1);
	UINT16 ammoamt = 0;

	for (i = 0; i < num_weapons; i++)
	{
		mobjtype_t weptype = 0;
		powertype_t power = 0;

		if (player->ringweapons & RW_BOUNCE) // Bounce
		{
			weptype = MT_BOUNCEPICKUP;
			player->ringweapons &= ~RW_BOUNCE;
			power = pw_bouncering;
		}
		else if (player->ringweapons & RW_RAIL) // Rail
		{
			weptype = MT_RAILPICKUP;
			player->ringweapons &= ~RW_RAIL;
			power = pw_railring;
		}
		else if (player->ringweapons & RW_AUTO) // Auto
		{
			weptype = MT_AUTOPICKUP;
			player->ringweapons &= ~RW_AUTO;
			power = pw_automaticring;
		}
		else if (player->ringweapons & RW_EXPLODE) // Explode
		{
			weptype = MT_EXPLODEPICKUP;
			player->ringweapons &= ~RW_EXPLODE;
			power = pw_explosionring;
		}
		else if (player->ringweapons & RW_SCATTER) // Scatter
		{
			weptype = MT_SCATTERPICKUP;
			player->ringweapons &= ~RW_SCATTER;
			power = pw_scatterring;
		}
		else if (player->ringweapons & RW_GRENADE) // Grenade
		{
			weptype = MT_GRENADEPICKUP;
			player->ringweapons &= ~RW_GRENADE;
			power = pw_grenadering;
		}

		if (!weptype) // ???
			continue;

		if (player->powers[power] >= mobjinfo[weptype].reactiontime)
			ammoamt = (UINT16)mobjinfo[weptype].reactiontime;
		else
			ammoamt = player->powers[power];

		player->powers[power] -= ammoamt;

		z = player->mo->z;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - mobjinfo[weptype].height;

		mo = P_SpawnMobj(player->mo->x, player->mo->y, z, weptype);
		mo->reactiontime = ammoamt;
		mo->flags2 |= MF2_DONTRESPAWN;
		mo->flags &= ~(MF_NOGRAVITY|MF_NOCLIPHEIGHT);
		P_SetTarget(&mo->target, player->mo);
		mo->fuse = 12*TICRATE;
		mo->destscale = player->mo->scale;
		P_SetScale(mo, player->mo->scale);

		// Angle offset by player angle
		fa = ((i*FINEANGLES/16) + (player->mo->angle>>ANGLETOFINESHIFT)) & FINEMASK;

		// Make rings spill out around the player in 16 directions like SA, but spill like Sonic 2.
		// Technically a non-SA way of spilling rings. They just so happen to be a little similar.

		// >16 ring type spillout
		ns = FixedMul(3*FRACUNIT, mo->scale);
		mo->momx = FixedMul(FINECOSINE(fa),ns);

		if (!(twodlevel || (player->mo->flags2 & MF2_TWOD)))
			mo->momy = FixedMul(FINESINE(fa),ns);

		P_SetObjectMomZ(mo, 4*FRACUNIT, false);

		if (i & 1)
			P_SetObjectMomZ(mo, 4*FRACUNIT, true);
	}
}

void P_PlayerWeaponAmmoBurst(player_t *player)
{
	mobj_t *mo;
	angle_t fa;
	fixed_t ns;
	INT32 i = 0;
	fixed_t z;

	mobjtype_t weptype = 0;
	powertype_t power = 0;

	while (true)
	{
		if (player->powers[pw_bouncering])
		{
			weptype = MT_BOUNCERING;
			power = pw_bouncering;
		}
		else if (player->powers[pw_railring])
		{
			weptype = MT_RAILRING;
			power = pw_railring;
		}
		else if (player->powers[pw_infinityring])
		{
			weptype = MT_INFINITYRING;
			power = pw_infinityring;
		}
		else if (player->powers[pw_automaticring])
		{
			weptype = MT_AUTOMATICRING;
			power = pw_automaticring;
		}
		else if (player->powers[pw_explosionring])
		{
			weptype = MT_EXPLOSIONRING;
			power = pw_explosionring;
		}
		else if (player->powers[pw_scatterring])
		{
			weptype = MT_SCATTERRING;
			power = pw_scatterring;
		}
		else if (player->powers[pw_grenadering])
		{
			weptype = MT_GRENADERING;
			power = pw_grenadering;
		}
		else
			break; // All done!

		z = player->mo->z;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - mobjinfo[weptype].height;

		mo = P_SpawnMobj(player->mo->x, player->mo->y, z, weptype);
		mo->health = player->powers[power];
		mo->flags2 |= MF2_DONTRESPAWN;
		mo->flags &= ~(MF_NOGRAVITY|MF_NOCLIPHEIGHT);
		P_SetTarget(&mo->target, player->mo);

		player->powers[power] = 0;
		mo->fuse = 12*TICRATE;

		mo->destscale = player->mo->scale;
		P_SetScale(mo, player->mo->scale);

		// Angle offset by player angle
		fa = ((i*FINEANGLES/16) + (player->mo->angle>>ANGLETOFINESHIFT)) & FINEMASK;

		// Spill them!
		ns = FixedMul(2*FRACUNIT, mo->scale);
		mo->momx = FixedMul(FINECOSINE(fa), ns);

		if (!(twodlevel || (player->mo->flags2 & MF2_TWOD)))
			mo->momy = FixedMul(FINESINE(fa),ns);

		P_SetObjectMomZ(mo, 3*FRACUNIT, false);

		if (i & 1)
			P_SetObjectMomZ(mo, 3*FRACUNIT, true);

		i++;
	}
}

//
// P_PlayerEmeraldBurst
//
// Spills ONLY emeralds.
//
void P_PlayerEmeraldBurst(player_t *player, boolean toss)
{
	INT32 i;
	angle_t fa;
	fixed_t ns;
	fixed_t z = 0, momx = 0, momy = 0;

	// Better safe than sorry.
	if (!player)
		return;

	// Spill power stones
	if (player->powers[pw_emeralds])
	{
		INT32 num_stones = 0;

		if (player->powers[pw_emeralds] & EMERALD1)
			num_stones++;
		if (player->powers[pw_emeralds] & EMERALD2)
			num_stones++;
		if (player->powers[pw_emeralds] & EMERALD3)
			num_stones++;
		if (player->powers[pw_emeralds] & EMERALD4)
			num_stones++;
		if (player->powers[pw_emeralds] & EMERALD5)
			num_stones++;
		if (player->powers[pw_emeralds] & EMERALD6)
			num_stones++;
		if (player->powers[pw_emeralds] & EMERALD7)
			num_stones++;

		for (i = 0; i < num_stones; i++)
		{
			INT32 stoneflag = 0;
			statenum_t statenum = S_CEMG1;
			mobj_t *mo;

			if (player->powers[pw_emeralds] & EMERALD1)
			{
				stoneflag = EMERALD1;
				statenum = S_CEMG1;
			}
			else if (player->powers[pw_emeralds] & EMERALD2)
			{
				stoneflag = EMERALD2;
				statenum = S_CEMG2;
			}
			else if (player->powers[pw_emeralds] & EMERALD3)
			{
				stoneflag = EMERALD3;
				statenum = S_CEMG3;
			}
			else if (player->powers[pw_emeralds] & EMERALD4)
			{
				stoneflag = EMERALD4;
				statenum = S_CEMG4;
			}
			else if (player->powers[pw_emeralds] & EMERALD5)
			{
				stoneflag = EMERALD5;
				statenum = S_CEMG5;
			}
			else if (player->powers[pw_emeralds] & EMERALD6)
			{
				stoneflag = EMERALD6;
				statenum = S_CEMG6;
			}
			else if (player->powers[pw_emeralds] & EMERALD7)
			{
				stoneflag = EMERALD7;
				statenum = S_CEMG7;
			}

			if (!stoneflag) // ???
				continue;

			player->powers[pw_emeralds] &= ~stoneflag;

			if (toss)
			{
				fa = player->mo->angle>>ANGLETOFINESHIFT;

				z = player->mo->z + player->mo->height;
				if (player->mo->eflags & MFE_VERTICALFLIP)
					z -= mobjinfo[MT_FLINGEMERALD].height + player->mo->height;
				ns = FixedMul(8*FRACUNIT, player->mo->scale);
			}
			else
			{
				fa = ((255 / num_stones) * i) * FINEANGLES/256;

				z = player->mo->z + (player->mo->height / 2);
				if (player->mo->eflags & MFE_VERTICALFLIP)
					z -= mobjinfo[MT_FLINGEMERALD].height;
				ns = FixedMul(4*FRACUNIT, player->mo->scale);
			}

			momx = FixedMul(FINECOSINE(fa), ns);

			if (!(twodlevel || (player->mo->flags2 & MF2_TWOD)))
				momy = FixedMul(FINESINE(fa),ns);
			else
				momy = 0;

			mo = P_SpawnMobj(player->mo->x, player->mo->y, z, MT_FLINGEMERALD);
			mo->health = 1;
			mo->threshold = stoneflag;
			mo->flags2 |= (MF2_DONTRESPAWN|MF2_SLIDEPUSH);
			mo->flags &= ~(MF_NOGRAVITY|MF_NOCLIPHEIGHT);
			P_SetTarget(&mo->target, player->mo);
			mo->fuse = 12*TICRATE;
			P_SetMobjState(mo, statenum);

			mo->momx = momx;
			mo->momy = momy;

			P_SetObjectMomZ(mo, 3*FRACUNIT, false);

			if (player->mo->eflags & MFE_VERTICALFLIP)
				mo->momz = -mo->momz;

			if (toss)
				player->tossdelay = 2*TICRATE;
		}
	}
}

/** Makes an injured or dead player lose possession of the flag.
  *
  * \param player The player with the flag, about to lose it.
  * \sa P_PlayerRingBurst
  */
void P_PlayerFlagBurst(player_t *player, boolean toss)
{
	mobj_t *flag;
	mobjtype_t type;

	if (!(player->gotflag & (GF_REDFLAG|GF_BLUEFLAG)))
		return;

	if (player->gotflag & GF_REDFLAG)
		type = MT_REDFLAG;
	else
		type = MT_BLUEFLAG;

	flag = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, type);

	if (player->mo->eflags & MFE_VERTICALFLIP)
		flag->z += player->mo->height - flag->height;

	if (toss)
		P_InstaThrust(flag, player->mo->angle, FixedMul(6*FRACUNIT, player->mo->scale));
	else
	{
		angle_t fa = P_RandomByte()*FINEANGLES/256;
		flag->momx = FixedMul(FINECOSINE(fa), FixedMul(6*FRACUNIT, player->mo->scale));
		if (!(twodlevel || (player->mo->flags2 & MF2_TWOD)))
			flag->momy = FixedMul(FINESINE(fa), FixedMul(6*FRACUNIT, player->mo->scale));
	}

	flag->momz = FixedMul(8*FRACUNIT, player->mo->scale);
	if (player->mo->eflags & MFE_VERTICALFLIP)
		flag->momz = -flag->momz;

	if (type == MT_REDFLAG)
		flag->spawnpoint = rflagpoint;
	else
		flag->spawnpoint = bflagpoint;

	flag->fuse = cv_flagtime.value * TICRATE;
	P_SetTarget(&flag->target, player->mo);

	// Flag text
	{
		char plname[MAXPLAYERNAME+4];
		const char *flagtext;
		char flagcolor;

		snprintf(plname, sizeof(plname), "%s%s%s",
				 CTFTEAMCODE(player),
				 player_names[player - players],
				 CTFTEAMENDCODE(player));

		if (type == MT_REDFLAG)
		{
			flagtext = M_GetText("Red flag");
			flagcolor = '\x85';
		}
		else
		{
			flagtext = M_GetText("Blue flag");
			flagcolor = '\x84';
		}

		if (toss)
			CONS_Printf(M_GetText("%s tossed the %c%s%c.\n"), plname, flagcolor, flagtext, 0x80);
		else
			CONS_Printf(M_GetText("%s dropped the %c%s%c.\n"), plname, flagcolor, flagtext, 0x80);
	}

	player->gotflag = 0;

	// Pointers set for displaying time value and for consistency restoration.
	if (type == MT_REDFLAG)
		redflag = flag;
	else
		blueflag = flag;

	if (toss)
		player->tossdelay = 2*TICRATE;

	return;
}
