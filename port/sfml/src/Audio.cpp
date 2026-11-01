/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

sf::SoundBuffer / sf::Sound (Web Audio API) and sf::Music (<audio>
element). Browsers only allow audio to start after a user gesture, so
everything requested before that is resumed on the first key press,
click or touch.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#include <SFML/Audio.hpp>

#include <emscripten.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

// ---------------------------------------------------------------------
// JavaScript side
// ---------------------------------------------------------------------
EM_JS(void, etr_audio_setup, (), {
	if (Module.etrAudio) return;
	var A = Module.etrAudio = {
		ctx: null,
		master: null,
		buffers: [],     // decoded AudioBuffers (or null while decoding)
		voices: [],      // {gain, source, playing, wantLoop, bufferId}
		musics: [],      // {el, url, wantPlay}
		unlocked: false
	};
	A.getCtx = function() {
		if (!A.ctx) {
			var AC = window.AudioContext || window.webkitAudioContext;
			if (!AC) return null;
			A.ctx = new AC();
			A.master = A.ctx.createGain();
			A.master.connect(A.ctx.destination);
		}
		return A.ctx;
	};
	A.unlock = function() {
		var ctx = A.getCtx();
		if (ctx && ctx.state !== 'running') ctx.resume();
		A.unlocked = true;
		for (var i = 0; i < A.musics.length; i++) {
			var m = A.musics[i];
			if (m && m.wantPlay && m.el.paused) m.el.play().catch(function() {});
		}
	};
	['keydown', 'mousedown', 'touchend', 'pointerdown'].forEach(function(t) {
		window.addEventListener(t, A.unlock, true);
	});
	A.startVoice = function(v) {
		var buf = A.buffers[v.bufferId];
		var ctx = A.getCtx();
		if (!buf || !ctx) return;
		if (v.source) { try { v.source.onended = null; v.source.stop(); } catch (e) {} }
		var src = ctx.createBufferSource();
		src.buffer = buf;
		src.loop = v.wantLoop;
		src.connect(v.gain);
		src.onended = function() { if (v.source === src) { v.playing = false; v.source = null; } };
		v.source = src;
		v.playing = true;
		src.start();
	};
});

EM_JS(int, etr_audio_can_play_ogg, (), {
	var a = document.createElement('audio');
	return (a.canPlayType && a.canPlayType('audio/ogg; codecs="vorbis"').length > 0) ? 1 : 0;
});

EM_JS(int, etr_buffer_create, (const unsigned char* data, int len), {
	var A = Module.etrAudio;
	var id = A.buffers.length;
	A.buffers.push(null);
	var ctx = A.getCtx();
	if (!ctx) return id;
	var bytes = HEAPU8.slice(data, data + len).buffer;
	ctx.decodeAudioData(bytes, function(buf) {
		A.buffers[id] = buf;
		// start looping voices that were requested before decoding finished
		for (var i = 0; i < A.voices.length; i++) {
			var v = A.voices[i];
			if (v && v.bufferId === id && v.playing && !v.source && v.wantLoop) A.startVoice(v);
		}
	}, function() { console.warn('Could not decode audio buffer ' + id); });
	return id;
});

EM_JS(int, etr_voice_create, (), {
	var A = Module.etrAudio;
	var ctx = A.getCtx();
	var v = {gain: ctx ? ctx.createGain() : null, source: null, playing: false, wantLoop: false, bufferId: -1};
	if (v.gain) v.gain.connect(A.master);
	A.voices.push(v);
	return A.voices.length - 1;
});

EM_JS(void, etr_voice_destroy, (int id), {
	var A = Module.etrAudio;
	var v = A.voices[id];
	if (!v) return;
	if (v.source) { try { v.source.onended = null; v.source.stop(); } catch (e) {} }
	if (v.gain) v.gain.disconnect();
	A.voices[id] = null;
});

EM_JS(void, etr_voice_play, (int id, int bufferId, int loop), {
	var A = Module.etrAudio;
	var v = A.voices[id];
	if (!v || !v.gain) return;
	v.bufferId = bufferId;
	v.wantLoop = !!loop;
	if (A.buffers[bufferId]) A.startVoice(v);
	else { v.playing = v.wantLoop; v.source = null; }  // looping voices start once decoded
});

EM_JS(void, etr_voice_stop, (int id), {
	var v = Module.etrAudio.voices[id];
	if (!v) return;
	if (v.source) { try { v.source.onended = null; v.source.stop(); } catch (e) {} }
	v.source = null;
	v.playing = false;
});

EM_JS(void, etr_voice_set_loop, (int id, int loop), {
	var v = Module.etrAudio.voices[id];
	if (!v) return;
	v.wantLoop = !!loop;
	if (v.source) v.source.loop = !!loop;
});

EM_JS(void, etr_voice_set_volume, (int id, float volume), {
	var v = Module.etrAudio.voices[id];
	if (v && v.gain) v.gain.gain.value = volume;
});

EM_JS(int, etr_voice_is_playing, (int id), {
	var v = Module.etrAudio.voices[id];
	return (v && v.playing) ? 1 : 0;
});

EM_JS(int, etr_music_create, (const unsigned char* data, int len, const char* mime), {
	var A = Module.etrAudio;
	var blob = new Blob([HEAPU8.slice(data, data + len)], {type: UTF8ToString(mime)});
	var url = URL.createObjectURL(blob);
	var el = new Audio();
	el.preload = 'auto';
	el.src = url;
	var m = {el: el, url: url, wantPlay: false};
	el.addEventListener('ended', function() { if (!el.loop) m.wantPlay = false; });
	A.musics.push(m);
	return A.musics.length - 1;
});

EM_JS(void, etr_music_destroy, (int id), {
	var A = Module.etrAudio;
	var m = A.musics[id];
	if (!m) return;
	m.el.pause();
	m.el.removeAttribute('src');
	URL.revokeObjectURL(m.url);
	A.musics[id] = null;
});

EM_JS(void, etr_music_play, (int id), {
	var m = Module.etrAudio.musics[id];
	if (!m) return;
	m.wantPlay = true;
	var p = m.el.play();
	if (p && p.catch) p.catch(function() { /* retried on first user gesture */ });
});

EM_JS(void, etr_music_pause, (int id), {
	var m = Module.etrAudio.musics[id];
	if (!m) return;
	m.wantPlay = false;
	m.el.pause();
});

EM_JS(void, etr_music_stop, (int id), {
	var m = Module.etrAudio.musics[id];
	if (!m) return;
	m.wantPlay = false;
	m.el.pause();
	try { m.el.currentTime = 0; } catch (e) {}
});

EM_JS(void, etr_music_set_loop, (int id, int loop), {
	var m = Module.etrAudio.musics[id];
	if (m) m.el.loop = !!loop;
});

EM_JS(void, etr_music_set_volume, (int id, float volume), {
	var m = Module.etrAudio.musics[id];
	if (m) m.el.volume = Math.max(0, Math.min(1, volume));
});

EM_JS(int, etr_music_status, (int id), {
	var m = Module.etrAudio.musics[id];
	if (!m) return 0;
	if (m.wantPlay) return 2;
	return (m.el.currentTime > 0 && !m.el.ended) ? 1 : 0;
});

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------
namespace {

bool ReadFile(const std::string& filename, std::vector<unsigned char>& out) {
	std::ifstream in(filename, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

bool EndsWith(const std::string& s, const char* suffix) {
	const std::size_t n = std::strlen(suffix);
	if (s.size() < n) return false;
	for (std::size_t i = 0; i < n; i++) {
		char a = s[s.size() - n + i];
		if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
		if (a != suffix[i]) return false;
	}
	return true;
}

void PutLE(std::vector<unsigned char>& out, std::uint32_t v, int bytes) {
	for (int i = 0; i < bytes; i++) out.push_back((v >> (8 * i)) & 0xFF);
}

// Decodes Ogg Vorbis to a 16-bit PCM WAV image (fallback for browsers
// without native Vorbis support).
bool OggToWav(const std::vector<unsigned char>& ogg, std::vector<unsigned char>& wav) {
	int channels = 0, rate = 0;
	short* pcm = nullptr;
	const int frames = stb_vorbis_decode_memory(ogg.data(), (int)ogg.size(), &channels, &rate, &pcm);
	if (frames <= 0 || !pcm) return false;
	const std::uint32_t dataBytes = (std::uint32_t)frames * channels * 2;
	wav.clear();
	wav.reserve(44 + dataBytes);
	const char* riff = "RIFF";
	wav.insert(wav.end(), riff, riff + 4);
	PutLE(wav, 36 + dataBytes, 4);
	const char* wavefmt = "WAVEfmt ";
	wav.insert(wav.end(), wavefmt, wavefmt + 8);
	PutLE(wav, 16, 4);
	PutLE(wav, 1, 2);
	PutLE(wav, channels, 2);
	PutLE(wav, rate, 4);
	PutLE(wav, rate * channels * 2, 4);
	PutLE(wav, channels * 2, 2);
	PutLE(wav, 16, 2);
	const char* data = "data";
	wav.insert(wav.end(), data, data + 4);
	PutLE(wav, dataBytes, 4);
	const unsigned char* p = reinterpret_cast<const unsigned char*>(pcm);
	wav.insert(wav.end(), p, p + dataBytes);
	std::free(pcm);
	return true;
}

// Loads an audio file, converting Ogg to WAV if the browser can't play it.
bool LoadPlayable(const std::string& filename, std::vector<unsigned char>& bytes, const char*& mime) {
	if (!ReadFile(filename, bytes)) return false;
	mime = "audio/wav";
	if (EndsWith(filename, ".ogg")) {
		if (etr_audio_can_play_ogg()) {
			mime = "audio/ogg";
		} else {
			std::vector<unsigned char> wav;
			if (!OggToWav(bytes, wav)) return false;
			bytes.swap(wav);
		}
	}
	return true;
}

} // namespace

namespace sf {

// ---------------------------------------------------------------------
// SoundBuffer / Sound
// ---------------------------------------------------------------------
SoundBuffer::SoundBuffer() : handle(-1) {}
SoundBuffer::~SoundBuffer() {}

bool SoundBuffer::loadFromFile(const std::string& filename) {
	etr_audio_setup();
	std::vector<unsigned char> bytes;
	const char* mime = nullptr;
	if (!LoadPlayable(filename, bytes, mime)) {
		std::fprintf(stderr, "Failed to load sound file \"%s\"\n", filename.c_str());
		return false;
	}
	handle = etr_buffer_create(bytes.data(), (int)bytes.size());
	return true;
}

Sound::Sound() : handle(-1), buffer(nullptr), loop(false), volume(100.f) {}

Sound::Sound(const SoundBuffer& b) : Sound() { setBuffer(b); }

Sound::~Sound() {
	if (handle >= 0) etr_voice_destroy(handle);
}

void Sound::setBuffer(const SoundBuffer& b) {
	buffer = &b;
	if (handle < 0) {
		etr_audio_setup();
		handle = etr_voice_create();
		etr_voice_set_volume(handle, volume / 100.f);
	}
}

void Sound::play() {
	if (handle < 0 || !buffer || buffer->getHandle() < 0) return;
	etr_voice_play(handle, buffer->getHandle(), loop ? 1 : 0);
}

void Sound::pause() { stop(); }

void Sound::stop() {
	if (handle >= 0) etr_voice_stop(handle);
}

void Sound::setLoop(bool l) {
	loop = l;
	if (handle >= 0) etr_voice_set_loop(handle, l ? 1 : 0);
}

void Sound::setVolume(float v) {
	volume = v;
	if (handle >= 0) etr_voice_set_volume(handle, v / 100.f);
}

SoundSource::Status Sound::getStatus() const {
	return (handle >= 0 && etr_voice_is_playing(handle)) ? Playing : Stopped;
}

// ---------------------------------------------------------------------
// Music (loaded lazily on first play to keep start-up fast)
// ---------------------------------------------------------------------
Music::Music() : handle(-1), loaded(false), loop(false), volume(100.f) {}

Music::~Music() {
	if (handle >= 0) etr_music_destroy(handle);
}

bool Music::openFromFile(const std::string& filename) {
	std::ifstream in(filename, std::ios::binary);
	if (!in) {
		std::fprintf(stderr, "Failed to open music file \"%s\"\n", filename.c_str());
		return false;
	}
	path = filename;
	return true;
}

bool Music::ensureLoaded() {
	if (loaded) return handle >= 0;
	loaded = true;
	etr_audio_setup();
	std::vector<unsigned char> bytes;
	const char* mime = nullptr;
	if (!LoadPlayable(path, bytes, mime)) {
		std::fprintf(stderr, "Failed to load music file \"%s\"\n", path.c_str());
		return false;
	}
	handle = etr_music_create(bytes.data(), (int)bytes.size(), mime);
	etr_music_set_loop(handle, loop ? 1 : 0);
	etr_music_set_volume(handle, volume / 100.f);
	return true;
}

void Music::play() {
	if (ensureLoaded()) etr_music_play(handle);
}

void Music::pause() {
	if (handle >= 0) etr_music_pause(handle);
}

void Music::stop() {
	if (handle >= 0) etr_music_stop(handle);
}

void Music::setLoop(bool l) {
	loop = l;
	if (handle >= 0) etr_music_set_loop(handle, l ? 1 : 0);
}

void Music::setVolume(float v) {
	volume = v;
	if (handle >= 0) etr_music_set_volume(handle, v / 100.f);
}

SoundSource::Status Music::getStatus() const {
	if (handle < 0) return Stopped;
	switch (etr_music_status(handle)) {
		case 2: return Playing;
		case 1: return Paused;
		default: return Stopped;
	}
}

} // namespace sf

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
