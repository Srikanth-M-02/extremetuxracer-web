// Extreme Tux Racer - WebGL port
// Runtime glue included with --pre-js (runs inside the Emscripten module).
//
// * Mounts /persist on IndexedDB (IDBFS) and loads saved options, players
//   and high scores before main() runs.
// * Provides Module.etrSyncFS() to write /persist back to IndexedDB.
// * Saves progress when the page is hidden or closed.

Module.preRun = Module.preRun || [];
Module.preRun.push(function() {
	FS.mkdir('/persist');
	FS.mount(IDBFS, {}, '/persist');
	addRunDependency('etr-persist');
	FS.syncfs(true, function(err) {
		if (err) console.warn('Extreme Tux Racer: could not load saved data', err);
		removeRunDependency('etr-persist');
	});
});

(function() {
	var syncing = false;
	var again = false;
	Module.etrSyncFS = function() {
		if (syncing) { again = true; return; }
		syncing = true;
		FS.syncfs(false, function(err) {
			syncing = false;
			if (err) console.warn('Extreme Tux Racer: could not save data', err);
			if (again) { again = false; Module.etrSyncFS(); }
		});
	};

	function saveNow() {
		if (Module.calledRun && Module._etr_save_progress) {
			try { Module._etr_save_progress(); } catch (e) { console.warn(e); }
		}
	}
	document.addEventListener('visibilitychange', function() {
		if (document.visibilityState === 'hidden') saveNow();
	});
	window.addEventListener('pagehide', saveNow);
})();
