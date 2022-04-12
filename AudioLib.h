/*
 * Audio library
 */

#pragma once

#include <string>
#include <vector>
#include "stb_vorbis.h"

#include <AudioToolbox/AudioQueue.h>
#include <AVFoundation/AVFoundation.h>
#include <AVFoundation/AVAudioSession.h>
#include <AVFoundation/AVPlayer.h>
#include <AVFoundation/AVAsset.h>

#include "Utils.h"

/*
*/

class AudioFile {
public:
    AudioFile();
    virtual ~AudioFile();

    virtual bool load(const std::string &filename, bool _is_loop) = 0;
    void readBufferS16(void *buf, void *prev_buf, void *temp_buf, size_t samples);

    void play();
    void pause();
    void reset();
    void seek(float t_sec);

    float getPositionSec() const { return 0; }

    float getDuration() const { return duration_sec; }
    bool isPlaying() const { return is_playing; }
    std::string getFilePath() const { return filename; }
    
    float volume = 1.0f;
    
protected:
    void free();

    void *data = nullptr;
    size_t size;
    int channels, freq, bps;
    bool is_loop = false;
    
    std::string filename;
    size_t pos_sample = 0;
    float duration_sec = 0.0f;
    bool is_playing = false;
};

class AudioFileWAV : public AudioFile {
public:
    bool load(const std::string &filename, bool _is_loop) override;
};

class AudioFileOGG : public AudioFile {
public:
    bool load(const std::string &filename, bool _is_loop) override;
};

/*
 */

class AudioManager {
public:
    AudioManager();
    ~AudioManager();
    
    AudioFile *load(const std::string &path, bool _is_loop);
    void free(AudioFile *p);
    void readBufferS16(void *buf, size_t samples);
    
private:
    int16_t *temp_buf = nullptr;
    AudioQueueRef audioQueue;
    std::vector<AudioFile*> sounds;
    void *prev_buffer = nullptr;
};

constexpr size_t FRAME_SIZE = sizeof(int16_t) * 2;
constexpr size_t BUFFER_SIZE = 2048 * FRAME_SIZE;

/*
*/

AudioFile::AudioFile() {
}

AudioFile::~AudioFile() {
    free();
}

void AudioFile::free() {
    pause();
}

void AudioFile::play() {
    is_playing = true;
}

void AudioFile::pause()  {
    is_playing = false;
}

void AudioFile::reset() {
    pos_sample = 0;
}

void AudioFile::seek(float t_sec) {
    pos_sample = 0;
}

void AudioFile::readBufferS16(void *buf, void *prev_buf, void *temp_buf, size_t samples) {
    if(!isPlaying()) return;
    
    int16_t *dst = static_cast<int16_t*>(temp_buf);
    const int16_t *src = static_cast<const int16_t*>(data);
    size_t src_samples = this->size / sizeof(int16_t);
    int32_t sample_scale = 44100 / freq;
    
    samples /= sample_scale;

    if(!is_loop) memset(temp_buf, 0, BUFFER_SIZE / 2);
    
    // stereo
    if(channels == 2) {
        if(is_loop) {
            for(int j = 0; j < samples * 2; j++) {
                dst[j] = src[(pos_sample + j) % src_samples];
            }
        } else if(pos_sample < src_samples) {
            for(int j = 0; j < std::min(samples * 2, src_samples - pos_sample); j++) {
                dst[j] = src[(pos_sample + j) % src_samples];
            }
        }
        
    // mono
    } else {
        if(is_loop) {
            for(int j = 0; j < samples; j++) {
                dst[j*2] = dst[j*2+1] = src[(pos_sample + j) % src_samples];
            }
        } else if(pos_sample < src_samples) {
            for(int j = 0; j < std::min(samples, src_samples - pos_sample); j++) {
                dst[j*2] = dst[j*2+1] = src[pos_sample + j];
            }
        }
    }
    pos_sample += samples * channels;
    
    // resample
    size_t i = samples * 2 - 1;
    size_t num = samples * sample_scale * 2;
    
    // 22050 to 44100
    if(sample_scale == 2) {
        for(int32 j = num - 1; j > sample_scale * 2; j -= sample_scale * 2) {
            int16_t w0 = dst[i-3];
            int16_t w1 = dst[i-2];
            int16_t w2 = dst[i-1];
            int16_t w3 = dst[i];
            dst[j-3] = (w2 + w0) / 2;
            dst[j-2] = (w3 + w1) / 2;
            dst[j-1] = w2;
            dst[j  ] = w3;
            i -= 2;
        }
        if(prev_buf) {
            int16_t *prev_dst = static_cast<int16_t*>(prev_buf);
            int16_t w0 = prev_dst[num-2];
            int16_t w1 = prev_dst[num-1];
            int16_t w2 = dst[0];
            int16_t w3 = dst[1];
            dst[0] = (w2 + w0) / 2;
            dst[1] = (w3 + w1) / 2;
            dst[2] = w2;
            dst[3] = w3;
        }
        
    // 11025 to 44100
    } else if(sample_scale == 4) {
        for(int32 j = num - 1; j > sample_scale * 2; j -= sample_scale * 2) {
            int16_t w0 = dst[i-3];
            int16_t w1 = dst[i-2];
            int16_t w2 = dst[i-1];
            int16_t w3 = dst[i];
            dst[j-7] = (w2 + w0*3) / 4;
            dst[j-6] = (w3 + w1*3) / 4;
            dst[j-5] = (w2 + w0) / 2;
            dst[j-4] = (w3 + w1) / 2;
            dst[j-3] = (w2*3 + w0) / 4;
            dst[j-2] = (w3*3 + w1) / 4;
            dst[j-1] = w2;
            dst[j  ] = w3;
            i -= 2;
        }
        if(prev_buf) {
            int16_t *prev_dst = static_cast<int16_t*>(prev_buf);
            int16_t w0 = prev_dst[num-2];
            int16_t w1 = prev_dst[num-1];
            int16_t w2 = dst[0];
            int16_t w3 = dst[1];
            dst[0] = (w2 + w0*3) / 4;
            dst[1] = (w3 + w1*3) / 4;
            dst[2] = (w2 + w0) / 2;
            dst[3] = (w3 + w1) / 2;
            dst[4] = (w2*3 + w0) / 4;
            dst[5] = (w3*3 + w1) / 4;
            dst[6] = w2;
            dst[7] = w3;
        }
    }
    
    // mix
    int16_t *outbuf = static_cast<int16_t*>(buf);
    for(size_t i = 0; i < num; i++) {
        int32_t v = outbuf[i] + dst[i] * volume;
        outbuf[i] = std::min(std::max(v, int32_t(std::numeric_limits<int16>::min())),
                             int32_t(std::numeric_limits<int16>::max()));
    }
}


/*
 * WAV
 */

enum {
    WAV_CHUNK_RIFF = 0x46464952,
    WAV_CHUNK_WAVE = 0x45564157,
    WAV_CHUNK_FMT = 0x20746D66,
    WAV_CHUNK_DATA = 0x61746164,
};

#pragma pack(push,1)
struct WavChunkFmt{
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bps;
};
#pragma pack(pop)

bool AudioFileWAV::load(const std::string &_filename, bool _is_loop) {
    this->filename = _filename;
    this->is_loop = _is_loop;

    FILE *file = fopen(filename.c_str(), "rb");
    if(!file) return false;

    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t chunk_wave;

    fread(&chunk_id,4,1,file);
    fread(&chunk_size,4,1,file);
    fread(&chunk_wave,4,1,file);

    if(chunk_id != WAV_CHUNK_RIFF || chunk_wave != WAV_CHUNK_WAVE) {
        fclose(file);
        return false;
    }

    // read fmt
    while(true) {
        if(!fread(&chunk_id,4,1,file)) break;
        if(!fread(&chunk_size,4,1,file)) break;
        if(chunk_id == WAV_CHUNK_FMT) break;
        fseek(file,chunk_size,SEEK_CUR);
    }
    if(feof(file)) {
        fclose(file);
        return false;
    }

    WavChunkFmt fmt;
    fread(&fmt,sizeof(WavChunkFmt),1,file);

    if(fmt.format != 1 || fmt.bps != 16) {
        fclose(file);
        return false;
    }

    // read data
    while(true) {
        if(!fread(&chunk_id,4,1,file)) break;
        if(!fread(&chunk_size,4,1,file)) break;
        if(chunk_id == WAV_CHUNK_DATA) break;
        fseek(file,chunk_size,SEEK_CUR);
    }

    this->channels = fmt.channels;
    this->bps = fmt.bps;
    this->freq = fmt.sample_rate;
    this->size = chunk_size;
    this->data = new uint8_t[chunk_size];
    fread(data,chunk_size,1,file);
    fclose(file);
    
    LOGI("Audio file is loaded \"%s\" freq:%u channels:%u BPS:%i format:%i",
         filename.c_str(),freq,channels,bps,fmt.format);
    return true;
}

/*
 * OGG
 */

bool AudioFileOGG::load(const std::string &_filename, bool _is_loop) {
    this->filename = _filename;
    this->is_loop = _is_loop;
    
    stb_vorbis *stream = stb_vorbis_open_filename(filename.c_str(), NULL, NULL);
    if(!stream) {
        LOGI("ERROR: Loading %s failed", filename.c_str());
        return false;
    }

    auto info = stb_vorbis_get_info(stream);
    uint32_t samples = stb_vorbis_stream_length_in_samples(stream) * info.channels;
    if(!samples) {
        LOGI("ERROR: Unable to decode %s", filename.c_str());
        return false;
    }
    if(info.channels < 0 || info.channels > 2) {
        LOGI("ERROR: Unsupported number of channels %s", filename.c_str());
        return false;
    }
    
    this->channels = info.channels;
    this->bps = 16;
    this->freq = info.sample_rate;
    this->size = samples * sizeof(int16_t);
    this->data = new uint8_t[this->size];
    
    if(freq != 44100 && freq != 22050 && freq != 11025) {
        LOGI("ERROR: Unsupported sample rate %s %i", filename.c_str(), freq);
    }

    stb_vorbis_get_samples_short_interleaved(stream, info.channels, static_cast<short*>(data), samples);
    stb_vorbis_close(stream);
    
    LOGI("Audio file is loaded \"%s\" freq:%u channels:%u",
         filename.c_str(),freq,channels);
    return true;
}

/*
 * Manager
 */

static void fill_buffer(void* inUserData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    auto manager = static_cast<AudioManager*>(inUserData);
    buffer->mAudioDataByteSize = BUFFER_SIZE;
    memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
    manager->readBufferS16(buffer->mAudioData, buffer->mAudioDataBytesCapacity / FRAME_SIZE);
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

AudioManager::AudioManager() {
    AudioStreamBasicDescription deviceFormat;
    deviceFormat.mSampleRate = 44100;
    deviceFormat.mFormatID = kAudioFormatLinearPCM;
    deviceFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
    deviceFormat.mBytesPerPacket = 4;
    deviceFormat.mFramesPerPacket = 1;
    deviceFormat.mBytesPerFrame = 4;
    deviceFormat.mChannelsPerFrame = 2;
    deviceFormat.mBitsPerChannel = 16;
    deviceFormat.mReserved = 0;

    OSStatus r = AudioQueueNewOutput(&deviceFormat, fill_buffer, this, NULL, kCFRunLoopCommonModes, 0, &audioQueue);
    if(r) printf("AudioQueueNewOutput() failed");
    
    for(int i = 0; i < 2; i++) {
        AudioQueueBufferRef buffer;
        r = AudioQueueAllocateBuffer(audioQueue, BUFFER_SIZE, &buffer);
        if(r) printf("AudioQueueAllocateBuffer() failed");
        buffer->mAudioDataByteSize = BUFFER_SIZE;
        memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
        AudioQueueEnqueueBuffer(audioQueue, buffer, 0, NULL);
    }
    r = AudioQueueStart(audioQueue, NULL);
    if(r) printf("AudioQueueStart() failed");
    
    temp_buf = new int16_t[BUFFER_SIZE / 2];
}

AudioManager::~AudioManager() {
    delete [] temp_buf;
    AudioQueueStop(audioQueue, true);
    AudioQueueDispose(audioQueue, true);
}

AudioFile *AudioManager::load(const std::string &path, bool _is_loop) {
    AudioFile *ret = nullptr;
    std::string ext = getFileExt(path);
    if(ext == "wav") ret = new AudioFileWAV();
    else if(ext == "ogg") ret = new AudioFileOGG();
    else return nullptr;
    
    ret->load(path,_is_loop);
    sounds.push_back(ret);
    return ret;
}

void AudioManager::free(AudioFile *p) {
    auto it = std::remove(sounds.begin(), sounds.end(), p);
    sounds.erase(it, sounds.end());
    delete p;
}

void AudioManager::readBufferS16(void *buf, size_t samples) {
    for(auto *sound : sounds) {
        sound->readBufferS16(buf,prev_buffer,temp_buf,samples);
    }
    prev_buffer = buf;
}
