/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Browser integration used by the game code (persistence, exit).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#include "web_platform.h"

#include "../../src/bh.h"
#include "../../src/game_ctrl.h"
#include "../../src/score.h"
#include "../../src/racing.h"

#include <emscripten.h>

namespace WebPlatform {

void SyncFileSystem() {
	EM_ASM({ if (Module.etrSyncFS) Module.etrSyncFS(); });
}

void SaveProgress() {
	// Players are only valid once a player has been selected on the
	// registration screen; saving earlier would overwrite stored data.
	if (g_game.argument < 1 && g_game.player != nullptr) {
		Score.SaveHighScore();
		Players.SavePlayers();
	}
	SyncFileSystem();
}

EM_JS(int, etr_is_fullscreen, (), {
	return (document.fullscreenElement || document.webkitFullscreenElement) ? 1 : 0;
});

EM_JS(void, etr_set_fullscreen, (int on), {
	if (Module.etrSetFullscreen) Module.etrSetFullscreen(!!on);
});

bool IsFullscreen() {
	return etr_is_fullscreen() != 0;
}

void SetFullscreen(bool on) {
	etr_set_fullscreen(on ? 1 : 0);
}

void StateEntered(const State* state) {
	const int racing = (state == &Racing) ? 1 : 0;
	EM_ASM({ if (Module.etrSetRacing) Module.etrSetRacing(!!$0); }, racing);
}

void GameExited() {
	SyncFileSystem();
	EM_ASM({ if (Module.etrOnExit) Module.etrOnExit(); });
}

} // namespace WebPlatform

// Called from JavaScript when the page is hidden or about to unload.
extern "C" EMSCRIPTEN_KEEPALIVE void etr_save_progress() {
	WebPlatform::SaveProgress();
}
