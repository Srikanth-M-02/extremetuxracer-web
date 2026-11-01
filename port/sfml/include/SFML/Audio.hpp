/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Minimal re-implementation of the SFML 2 Audio module on top of the
browser: sound effects use the Web Audio API, music is streamed through
an <audio> element. Ogg Vorbis files are decoded with stb_vorbis on
browsers that cannot play them natively.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#ifndef ETR_SFML_AUDIO_HPP
#define ETR_SFML_AUDIO_HPP

#include <SFML/System.hpp>
#include <string>

namespace sf {

class SoundSource {
public:
	enum Status { Stopped, Paused, Playing };
	virtual ~SoundSource() {}
};

class SoundBuffer {
public:
	SoundBuffer();
	~SoundBuffer();
	SoundBuffer(const SoundBuffer&) = delete;
	SoundBuffer& operator=(const SoundBuffer&) = delete;
	bool loadFromFile(const std::string& filename);
	int getHandle() const { return handle; }
private:
	int handle;
};

class Sound : public SoundSource {
public:
	Sound();
	explicit Sound(const SoundBuffer& buffer);
	~Sound();
	Sound(const Sound&) = delete;
	Sound& operator=(const Sound&) = delete;
	void setBuffer(const SoundBuffer& buffer);
	void play();
	void pause();
	void stop();
	void setLoop(bool loop);
	bool getLoop() const { return loop; }
	void setVolume(float volume);
	float getVolume() const { return volume; }
	Status getStatus() const;
private:
	int handle;
	const SoundBuffer* buffer;
	bool loop;
	float volume;
};

class Music : public SoundSource {
public:
	Music();
	~Music();
	Music(const Music&) = delete;
	Music& operator=(const Music&) = delete;
	bool openFromFile(const std::string& filename);
	void play();
	void pause();
	void stop();
	void setLoop(bool loop);
	bool getLoop() const { return loop; }
	void setVolume(float volume);
	float getVolume() const { return volume; }
	Status getStatus() const;
private:
	bool ensureLoaded();
	int handle;
	std::string path;
	bool loaded;
	bool loop;
	float volume;
};

} // namespace sf

#endif
