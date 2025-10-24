#ifndef AUDIO_PLAYER_CONTROLLER_H__
#define AUDIO_PLAYER_CONTROLLER_H__
#include <Arduino.h>
#include <vector>
#include <console.h>
#include <database.h>

#include "Audio.h"
#include "SPIFFS.h"
#include "FS.h"

Audio audio;

enum AudioPlayMode {
    PLAY_MODE_IMMEDIATE,
    PUT_IN_QUEUE
};

class AudioPlayerController {
    std::vector<String> queue;
public:
    uint32_t play(String filename, AudioPlayMode mode = AudioPlayMode::PUT_IN_QUEUE);
    void loop();
    void logCurrentAudioDetails();
    void flush();
    void unqueue(String filename);
};
AudioPlayerController apCtrl;

void AudioPlayerController::logCurrentAudioDetails() {
    console.log("sample rate:", audio.getSampleRate());
    console.log("bits per sample:", audio.getBitsPerSample());
    console.log("chanel:", audio.getChannels());
    console.log("bit rate:", audio.getBitRate());
    console.log("audio duration", audio.getAudioFileDuration());
    console.log("audio current time", audio.getAudioCurrentTime());
}


uint32_t AudioPlayerController::play(String filename, AudioPlayMode mode) {
    if (mode == AudioPlayMode::PLAY_MODE_IMMEDIATE) {
        audio.stopSong();
        audio.connecttoFS(SPIFFS, filename.c_str());
        logCurrentAudioDetails();
        return audio.getTotalPlayingTime();
    } else {
        if (Database::hasFile(filename)) {
            queue.push_back(filename);
        } else {
            console.log("Audio file does not exist");
        }
    }
    return 0;
}


void AudioPlayerController::flush() {
    queue.clear();
    audio.stopSong();
}

void AudioPlayerController::unqueue(String filename) {
    auto it = std::find(queue.begin(), queue.end(), filename);

    if (it != queue.end()) {
        queue.erase(it);
        queue.shrink_to_fit();
    }
}

void AudioPlayerController::loop() {
    if (audio.isRunning()) {
        audio.loop();  
        
    } else {
        if (!queue.empty()) {
            console.log(queue);
            String nextFile = queue.front();
            queue.erase(queue.begin());
            audio.connecttoFS(SPIFFS, nextFile.c_str());  
            play(nextFile, AudioPlayMode::PLAY_MODE_IMMEDIATE);
        }
    }
}
#endif