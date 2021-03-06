/* Copyright (c) 2002-2012 Croteam Ltd.
This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as published by
the Free Software Foundation


This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include "StdH.h"

// Game state properties
INDEX _iRound = 1;
BOOL _bHadPlayers = 0;
BOOL _bRestart = 0;

CTimerValue _tvLastLevelEnd(-1i64);

static CTString strBegScript;
static CTString strEndScript;

// Start new game on a specific level
static BOOL StartGame(const CTString &strLevel)
{
  // [Cecil] Reset start player indices
  GetGameAPI()->ResetStartPlayers();

  GetGameAPI()->SetCustomLevel(strLevel);
  GetGameAPI()->SetNetworkProvider(CGameAPI::NP_SERVER);

  // [Cecil] Pass byte container
  CSesPropsContainer sp;
  _pGame->SetMultiPlayerSession((CSessionProperties &)sp);

  BOOL bGameStarted = _pGame->NewGame(GetGameAPI()->GetSessionName(), strLevel, (CSessionProperties &)sp);

  // [Cecil] Custom logic after loading in the world
  if (bGameStarted)
  {
    // Start game for Core
    GetAPI()->OnGameStart();
  }

  return bGameStarted;
};

// Begin round on the current map
void RoundBegin(void)
{
  // repeat generate script names
  FOREVER {
    strBegScript.PrintF("%s%d_begin.ini", ded_strConfig, _iRound);
    strEndScript.PrintF("%s%d_end.ini", ded_strConfig, _iRound);

    // if start script exists
    if (FileExists(strBegScript)) {
      // stop searching
      break;

    // if start script doesn't exist
    } else {
      // if this is first round
      if (_iRound == 1) {
        // error
        CPrintF(TRANS("No scripts present!\n"));
        _bRunning = FALSE;
        return;
      }

      // try again with first round
      _iRound = 1;
    }
  }

  // run start script
  ExecScript(strBegScript);

  // start the level specified there
  if (ded_strLevel == "") {
    CPrintF(TRANS("ERROR: No next level specified!\n"));
    _bRunning = FALSE;

  } else {
    StartNewMap();

    CPrintF(TRANS("\nALL OK: Dedicated server is now running!\n"));
    CPrintF(TRANS("Use Ctrl+C to shutdown the server.\n"));
    CPrintF(TRANS("DO NOT use the 'Close' button, it might leave the port hanging!\n\n"));
  }
};

// End round on the current map
void RoundEnd(BOOL bGameEnd)
{
  // [Cecil] Stop game for Core
  GetAPI()->OnGameStop();

  // [Cecil] Not the end of the game yet
  if (!bGameEnd) {
    CPrintF("end of round---------------------------\n");

    ExecScript(strEndScript);
    _iRound++;
  }
};

// Start new map loading
void StartNewMap(void)
{
  EnableLoadingHook();
  StartGame(ded_strLevel);

  _bHadPlayers = 0;
  _bRestart = 0;

  DisableLoadingHook();
  _tvLastLevelEnd = CTimerValue(-1i64);
};

// Limit current frame rate if neeeded
static void LimitFrameRate(void) {
  // measure passed time for each loop
  static CTimerValue tvLast(-1.0f);

  CTimerValue tvNow = _pTimer->GetHighPrecisionTimer();
  TIME tmCurrentDelta = (tvNow - tvLast).GetSeconds();

  // limit maximum frame rate
  ded_iMaxFPS = ClampDn(ded_iMaxFPS, 1L);
  TIME tmWantedDelta = 1.0f / ded_iMaxFPS;

  if (tmCurrentDelta < tmWantedDelta) {
    Sleep((tmWantedDelta - tmCurrentDelta) * 1000.0f);
  }

  // remember new time
  tvLast = _pTimer->GetHighPrecisionTimer();
};

// Main game loop
void DoGame(void)
{
  // do the main game loop
  if (GetGameAPI()->IsGameOn()) {
    _pGame->GameMainLoop();

    // if any player is connected
    if (_pGame->GetPlayersCount()) {
      if (!_bHadPlayers) {
        // unpause server
        if (_pNetwork->IsPaused()) {
          _pNetwork->TogglePause();
        }
      }

      // remember that
      _bHadPlayers = 1;

    // if no player is connected
    } else {
      // if was before
      if (_bHadPlayers) {
        // make it restart
        _bRestart = TRUE;

      // if never had any player yet
      } else {
        // keep the server paused
        if (!_pNetwork->IsPaused()) {
          _pNetwork->TogglePause();
        }
      }
    }

  // if game is not started
  } else {
    // just handle broadcast messages
    _pNetwork->GameInactive();
  }

  // limit current frame rate if needed
  LimitFrameRate();
};
