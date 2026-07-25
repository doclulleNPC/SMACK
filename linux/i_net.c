// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// DESCRIPTION:
//      Network system interface. The original SMMU Linux port was a
//      single-player stub (the -net CLI was not parsed in the
//      original i_net.c). We preserve that behaviour. If you want
//      network play, implement PacketSend/PacketGet and a parser
//      for "-net" in I_InitNetwork.
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../d_event.h"
#include "../d_net.h"
#include "../m_argv.h"
#include "../i_net.h"

void NetSend(void);
boolean NetListen(void);

void (*netget)(void)  = NULL;
void (*netsend)(void) = NULL;

void PacketSend(void) {}
void PacketGet(void)  {}

void I_InitNetwork(void)
{
  int i;

  // set up the singleplayer doomcom
  singleplayer.id           = DOOMCOM_ID;
  singleplayer.numplayers   = 1;
  singleplayer.numnodes     = 1;
  singleplayer.deathmatch   = false;
  singleplayer.consoleplayer = 0;
  singleplayer.extratics    = 0;
  singleplayer.ticdup       = 1;

  i = 0;  // M_CheckParm("-net");  // intentionally not parsed yet
  if (!i)
  {
    doomcom = &singleplayer;
    netgame = false;
    return;
  }

  // if you implement -net, set doomcom to a real buffer and
  // hook netsend / netget here.
  doomcom = &singleplayer;
  netgame = false;
}

void I_NetCmd(void)
{
  if (doomcom->command == CMD_SEND)
  {
    if (netsend) netsend();
  }
  else if (doomcom->command == CMD_GET)
  {
    if (netget) netget();
  }
  else
    I_Error("Bad net cmd: %i\n", doomcom->command);
}
