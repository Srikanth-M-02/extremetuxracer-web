/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Browser integration used by the game code (persistence, exit).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#ifndef ETR_WEB_PLATFORM_H
#define ETR_WEB_PLATFORM_H

class State;

namespace WebPlatform {

// Writes players and high scores (once they have been loaded) and
// flushes /persist to IndexedDB.
void SaveProgress();

// Flushes /persist to IndexedDB (e.g. after the options were saved).
void SyncFileSystem();

// Browser full screen (the page, not the game's video mode).
bool IsFullscreen();
void SetFullscreen(bool on);

// Called after a state has been entered (shows or hides the touch
// controls, which are only needed while racing).
void StateEntered(const State* state);

// Called when the player chose "Quit" in the game.
void GameExited();

} // namespace WebPlatform

#endif
