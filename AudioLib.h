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

/*
*/

namespace AudioLib {

class Manager;
constexpr size_t FRAME_SIZE = sizeof(int16_t) * 2;
constexpr size_t BUFFER_SIZE = 2048 * FRAME_SIZE;

enum {
    AUDIOLIB_SUCCESS = 0,
    AUDIOLIB_FILE_ERROR,
    AUDIOLIB_DECODE_ERROR,
    AUDIOLIB_WRONG_SAMPLE_RATE,
    AUDIOLIB_WRONG_CHANNEL_COUNT
};

class Sound {
public:
    friend class Manager;
    
    Sound() { }
    virtual ~Sound() { delete [] data; }

    virtual int32_t load(const std::string &filename, bool _is_loop) = 0;
    
    void play() { is_playing = true; }
    void pause() { is_playing = false; }
    void seek(float t_sec) { pos_sample = t_sec * bps * channels; }

    float getPositionSec() const { return (pos_sample / channels) / float(bps); }
    float getDuration() const { return duration_sec; }
    bool isPlaying() const { return is_playing; }
    std::string getFilePath() const { return filename; }
    
    float volume = 1.0f;
    float pan = 0.0f;
    
protected:
    void fillBuffer(void *buf, void *prev_buf, void *temp_buf, size_t samples) {
        if(!isPlaying() || !data) return;
        
        int16_t *dst = static_cast<int16_t*>(temp_buf);
        const int16_t *src = reinterpret_cast<const int16_t*>(data);
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
        int32 num = int32(samples * sample_scale * 2);
        
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
        float volumes[2] = { std::min(-pan + 1.0f, 1.0f) * volume, std::min(pan + 1.0f, 1.0f) * volume };
        int16_t *outbuf = static_cast<int16_t*>(buf);
        for(size_t i = 0; i < num; i++) {
            int32_t v = outbuf[i] + dst[i] * volumes[i % 2];
            outbuf[i] = std::min(std::max(v, int32_t(std::numeric_limits<int16>::min())),
                                 int32_t(std::numeric_limits<int16>::max()));
        }
    }

    uint8_t *data = nullptr;
    size_t size;
    int channels, freq, bps;
    bool is_loop = false;
    std::string filename;
    size_t pos_sample = 0;
    float duration_sec = 0.0f;
    bool is_playing = false;
};

/*
 * WAV
 */

class AudioFileWAV : public Sound {
public:
    int32_t load(const std::string &_filename, bool _is_loop) override {
        this->filename = _filename;
        this->is_loop = _is_loop;

        FILE *file = fopen(filename.c_str(), "rb");
        if(!file) return AUDIOLIB_FILE_ERROR;
        fseek(file,12,SEEK_SET);

        struct WaveFormat{
            uint16_t format;
            uint16_t channels;
            uint32_t sample_rate;
            uint32_t byte_rate;
            uint16_t block_align;
            uint16_t bps;
        } fmt;
        
        while(!feof(file)) {
            uint32_t chunk_id;
            uint32_t chunk_size;
            if(!fread(&chunk_id,4,1,file)) break;
            if(!fread(&chunk_size,4,1,file)) break;
            if(chunk_id == 0x20746D66) { // format
                fread(&fmt,sizeof(WaveFormat),1,file);
                this->channels = fmt.channels;
                this->bps = fmt.bps;
                this->freq = fmt.sample_rate;
                if(fmt.format != 1 || fmt.bps != 16) break;
                if(fmt.channels < 0 || fmt.channels > 2) {
                    fclose(file);
                    return AUDIOLIB_WRONG_CHANNEL_COUNT;
                }
                if(freq != 44100 && freq != 22050 && freq != 11025) {
                    fclose(file);
                    return AUDIOLIB_WRONG_SAMPLE_RATE;
                }
            } else if(chunk_id == 0x61746164) { // data
                this->size = chunk_size;
                this->data = new uint8_t[chunk_size];
                fread(data,chunk_size,1,file);
                fclose(file);
                return AUDIOLIB_SUCCESS;
            } else {
                fseek(file,chunk_size,SEEK_CUR);
            }
        }
        
        fclose(file);
        return AUDIOLIB_DECODE_ERROR;
    }
};

/*
 * OGG
 */

class AudioFileOGG : public Sound {
public:
    int32_t load(const std::string &_filename, bool _is_loop) override {
        this->filename = _filename;
        this->is_loop = _is_loop;
        
        stb_vorbis *stream = stb_vorbis_open_filename(filename.c_str(), NULL, NULL);
        if(!stream) return AUDIOLIB_FILE_ERROR;

        auto info = stb_vorbis_get_info(stream);
        uint32_t samples = stb_vorbis_stream_length_in_samples(stream) * info.channels;
        if(!samples) return AUDIOLIB_DECODE_ERROR;
        if(info.channels < 0 || info.channels > 2)
            return AUDIOLIB_WRONG_CHANNEL_COUNT;
        if(info.sample_rate != 44100 && info.sample_rate != 22050 && info.sample_rate != 11025)
            return AUDIOLIB_WRONG_SAMPLE_RATE;
        
        this->channels = info.channels;
        this->bps = 16;
        this->freq = info.sample_rate;
        this->size = samples * sizeof(int16_t);
        this->data = new uint8_t[this->size];
        stb_vorbis_get_samples_short_interleaved(stream, info.channels, reinterpret_cast<short*>(data), samples);
        stb_vorbis_close(stream);
        return AUDIOLIB_SUCCESS;
    }
};

/*
 * Backends
 */

struct Backend {
    Backend(Manager *mgr) { }
    virtual ~Backend() { };
};

static void fill_buffer(void* inUserData, AudioQueueRef queue, AudioQueueBufferRef buffer);
struct BackendAudioToolbox : Backend {
    BackendAudioToolbox(Manager *mgr) : Backend(mgr) {
        AudioStreamBasicDescription desc;
        desc.mSampleRate = 44100;
        desc.mFormatID = kAudioFormatLinearPCM;
        desc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
        desc.mBytesPerPacket = 4;
        desc.mFramesPerPacket = 1;
        desc.mBytesPerFrame = 4;
        desc.mChannelsPerFrame = 2;
        desc.mBitsPerChannel = 16;
        desc.mReserved = 0;

        auto r = AudioQueueNewOutput(&desc, fill_buffer, mgr, NULL, kCFRunLoopCommonModes, 0, &audioQueue);
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
    }
    ~BackendAudioToolbox() {
        AudioQueueStop(audioQueue, true);
        AudioQueueDispose(audioQueue, true);
    }
    
    AudioQueueRef audioQueue;
};


/*
 * Manager
 */

inline void low(uint8_t &c) { if((c>191 && c<224) || (c>64 && c<91)) c += 32; }
static std::string strlow(const std::string &_str) {
    std::string temp(_str);
    for(auto &c : temp) low(reinterpret_cast<uint8_t&>(c));
    return temp;
}

class Manager {
public:
    Manager() {
        temp_buf = new int16_t[BUFFER_SIZE / 2];
        backend = new BackendAudioToolbox(this);
    }
    
    ~Manager() {
        delete backend;
        delete [] temp_buf;
    }
    
    Sound *load(const std::string &path, bool _is_loop) {
        std::string ext;
        size_t dot = path.rfind('.');
        if(dot != std::string::npos) ext = path.substr(dot+1, path.length() - dot - 1);
        ext = strlow(ext);
        
        Sound *ret = nullptr;
        if(ext == "wav") ret = new AudioFileWAV();
        else if(ext == "ogg") ret = new AudioFileOGG();
        else return nullptr;
        
        ret->load(path,_is_loop);
        sounds.push_back(ret);
        return ret;
    }
    
    void free(Sound *p) {
        auto it = std::remove(sounds.begin(), sounds.end(), p);
        sounds.erase(it, sounds.end());
        delete p;
    }
    
    void fillBuffer(void *buf, size_t samples) {
        for(auto *s : sounds) s->fillBuffer(buf,prev_buffer,temp_buf,samples);
        prev_buffer = buf;
    }
    
private:
    Backend *backend = nullptr;
    int16_t *temp_buf = nullptr;
    std::vector<Sound*> sounds;
    void *prev_buffer = nullptr;
};


/*
 * Backend callbacks
 */

static void fill_buffer(void* inUserData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    auto manager = static_cast<Manager*>(inUserData);
    buffer->mAudioDataByteSize = BUFFER_SIZE;
    memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
    manager->fillBuffer(buffer->mAudioData, buffer->mAudioDataBytesCapacity / FRAME_SIZE);
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

} // namespace AudioLib
