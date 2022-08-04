/************************************************************************
 * Audio library
 * https://github.com/tdmaav/AudioLib
 ************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <type_traits>
#include <random>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomma"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#include "stb_vorbis.h"
#pragma GCC diagnostic pop

#ifdef AUDIOLIB_BACKEND_AUDIOTOOLBOX
    #include <AudioToolbox/AudioQueue.h>
    #include <AVFoundation/AVFoundation.h>
    #include <AVFoundation/AVAudioSession.h>
    #include <AVFoundation/AVPlayer.h>
    #include <AVFoundation/AVAsset.h>
#elif defined(AUDIOLIB_BACKEND_OPENSLES)
    #include <SLES/OpenSLES.h>
    #include <SLES/OpenSLES_Android.h>
#endif

/*
 */

namespace AudioLib {

class Manager;
constexpr size_t SAMPLE_SIZE = sizeof(int16_t) * 2;
constexpr size_t SAMPLE_COUNT = 2048;
constexpr size_t BUFFER_SIZE = SAMPLE_COUNT * SAMPLE_SIZE;

enum {
    AUDIOLIB_SUCCESS = 0,
    AUDIOLIB_FILE_ERROR,
    AUDIOLIB_DECODE_ERROR,
    AUDIOLIB_WRONG_SAMPLE_RATE,
    AUDIOLIB_WRONG_CHANNEL_COUNT
};

struct Sound {
    friend class Manager;
    
    Sound() { }
    virtual ~Sound() { delete [] data; }

    virtual int32_t load(const std::string &filename, int32_t _loop) = 0;
    virtual void read(size_t samples) { }
    
    void play() { is_playing = true; }
    void pause() { is_playing = false; }
    void stop() {
        is_playing = false;
        pos_sample = 0;
        delete [] data;
        data = nullptr;
    }
    void seek(float t_sec) { pos_sample = t_sec * bps * channels; }

    float getPositionSec() const { return (pos_sample / channels) / float(bps); }
    float getDuration() const { return duration_sec; }
    bool isPlaying() const { return is_playing; }
    std::string getFilePath() const { return filename; }
    
    float volume = 1.0f;
    float pan = 0.0f;
    
protected:
    void fillBuffer(void *buf, void *prev_buf, void *temp_buf, size_t samples) {
        if(!is_playing || !data) return;

        int16_t *dst = static_cast<int16_t*>(temp_buf);
        const int16_t *src = reinterpret_cast<const int16_t*>(data);
        size_t src_samples = this->size / sizeof(int16_t);
        size_t src_samples_repeats = src_samples * (loop+1);
        int32_t sample_scale = 44100 / freq;
        
        samples /= sample_scale;

        if(loop >= 0) memset(temp_buf, 0, BUFFER_SIZE);
        read(samples);
        
        // stereo
        if(channels == 2) {
            if(loop < 0) {
                for(int j = 0; j < samples * 2; j++) {
                    dst[j] = src[(pos_sample + j) % src_samples];
                }
            } else if(pos_sample < src_samples_repeats) {
                for(int j = 0; j < std::min(samples * 2, src_samples_repeats - pos_sample); j++) {
                    dst[j] = src[(pos_sample + j) % src_samples];
                }
            }
            
        // mono
        } else {
            if(loop < 0) {
                for(int j = 0; j < samples; j++) {
                    dst[j*2] = dst[j*2+1] = src[(pos_sample + j) % src_samples];
                }
            } else if(pos_sample < src_samples_repeats) {
                for(int j = 0; j < std::min(samples, src_samples_repeats - pos_sample); j++) {
                    dst[j*2] = dst[j*2+1] = src[(pos_sample + j) % src_samples];
                }
            }
        }
        pos_sample += samples * channels;
//        pos_sample %= this->size / sizeof(int16_t);
        
        // resample
        size_t i = samples * 2 - 1;
        int32 num = int32(samples * sample_scale * 2);
        
        // 22050 to 44100
        if(sample_scale == 2) {
            for(int32 j = num - 1; j > sample_scale * 2; j -= sample_scale * 2) {
                const int16_t w[] = { dst[i-3], dst[i-2], dst[i-1], dst[i] };
                dst[j-3] = (w[2] + w[0]) / 2;
                dst[j-2] = (w[3] + w[1]) / 2;
                dst[j-1] = w[2];
                dst[j  ] = w[3];
                i -= 2;
            }
            if(prev_buf) {
                const int16_t *prev_dst = static_cast<const int16_t*>(prev_buf);
                const int16_t w[] = { prev_dst[num-2], prev_dst[num-1], dst[0], dst[1] };
                dst[0] = (w[2] + w[0]) / 2;
                dst[1] = (w[3] + w[1]) / 2;
                dst[2] = w[2];
                dst[3] = w[3];
            }
            
        // 11025 to 44100
        } else if(sample_scale == 4) {
            for(int32 j = num - 1; j > sample_scale * 2; j -= sample_scale * 2) {
                const int16_t w[] = { dst[i-3], dst[i-2], dst[i-1], dst[i] };
                dst[j-7] = (w[2] + w[0]*3) / 4;
                dst[j-6] = (w[3] + w[1]*3) / 4;
                dst[j-5] = (w[2] + w[0]) / 2;
                dst[j-4] = (w[3] + w[1]) / 2;
                dst[j-3] = (w[2]*3 + w[0]) / 4;
                dst[j-2] = (w[3]*3 + w[1]) / 4;
                dst[j-1] = w[2];
                dst[j  ] = w[3];
                i -= 2;
            }
            if(prev_buf) {
                const int16_t *prev_dst = static_cast<const int16_t*>(prev_buf);
                const int16_t w[] = { prev_dst[num-2], prev_dst[num-1], dst[0], dst[1] };
                dst[0] = (w[2] + w[0]*3) / 4;
                dst[1] = (w[3] + w[1]*3) / 4;
                dst[2] = (w[2] + w[0]) / 2;
                dst[3] = (w[3] + w[1]) / 2;
                dst[4] = (w[2]*3 + w[0]) / 4;
                dst[5] = (w[3]*3 + w[1]) / 4;
                dst[6] = w[2];
                dst[7] = w[3];
            }
        }
        
        // mix
        float volumes[2] = { std::min(-pan + 1.0f, 1.0f) * volume, std::min(pan + 1.0f, 1.0f) * volume };
        int16_t *outbuf = static_cast<int16_t*>(buf);
        for(i = 0; i < num; i++) {
            int32_t v = outbuf[i] + dst[i] * volumes[i % 2];
            outbuf[i] = std::min(std::max(v, -32768), 32767);
        }
    }

    uint8_t *data = nullptr;
    size_t size;
    int channels, freq, bps;
    int32_t loop = 0;
    std::string filename;
    size_t pos_sample = 0;
    float duration_sec = 0.0f;
    bool is_playing = false;
};

/************************************************************************
 * WAV
 ************************************************************************/

struct SoundWAV : Sound {
    int32_t load(const std::string &_filename, int32_t _loop) override {
        this->filename = _filename;
        this->loop = _loop;

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
                this->duration_sec = float(size / sizeof(int16_t) / channels) / freq;
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

/************************************************************************
 * OGG
 ************************************************************************/

struct SoundOGG : Sound {
    int32_t load(const std::string &_filename, int32_t _loop) override {
        this->filename = _filename;
        this->loop = _loop;
        
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
        this->duration_sec = float(samples / channels) / freq;
        this->data = new uint8_t[this->size];
        stb_vorbis_get_samples_short_interleaved(stream, info.channels, reinterpret_cast<short*>(data), samples);
        stb_vorbis_close(stream);
        return AUDIOLIB_SUCCESS;
    }
};

/************************************************************************
 * Generative
 ************************************************************************/

struct SoundNoise : Sound {
    int32_t load(const std::string &_filename, int32_t _loop) override {
        this->loop = -1;
        this->channels = 1;
        this->bps = 16;
        this->freq = 44100;
        this->size = SAMPLE_COUNT * sizeof(int16_t);
        this->data = new uint8_t[this->size];
        return AUDIOLIB_SUCCESS;
    }

    void read(size_t samples) override {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(
            std::numeric_limits<std::int16_t>::min(),
            std::numeric_limits<std::int16_t>::max()
        );
        int16_t *dst = reinterpret_cast<int16_t*>(data);
        for(size_t i = 0; i < samples * channels; i++) dst[i] = distrib(gen);
    }
};

struct SoundSin : Sound {
    int32_t load(const std::string &_filename, int32_t _loop) override {
        this->loop = -1;
        this->channels = 1;
        this->bps = 16;
        this->freq = 44100;
        this->size = SAMPLE_COUNT * sizeof(int16_t);
        this->data = new uint8_t[this->size];
        return AUDIOLIB_SUCCESS;
    }

    void read(size_t samples) override {
        int16_t *dst = reinterpret_cast<int16_t*>(data);
        for(size_t i = 0; i < samples * channels; i++) {
            size_t pos = pos_sample + i;
            dst[pos % SAMPLE_COUNT] = std::sin((pos+std::sin(pos*0.0001f)*1000)*0.05f) * 32767;
        }
    }
};

/************************************************************************
 * Filters
 ************************************************************************/

struct Filter {
    virtual int16_t *process(int16_t *data, size_t samples) = 0;
};

/************************************************************************
 * Backends
 ************************************************************************/

#ifdef AUDIOLIB_BACKEND_AUDIOTOOLBOX
static void fill_buffer(void* in_user_data, AudioQueueRef queue, AudioQueueBufferRef buffer);
struct Backend {
    Backend(Manager *mgr) {
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

        auto r = AudioQueueNewOutput(&desc, fill_buffer, mgr, NULL, kCFRunLoopCommonModes, 0, &queue);
        if(r) printf("AudioQueueNewOutput() failed");
        
        for(int i = 0; i < 2; i++) {
            AudioQueueBufferRef buffer;
            r = AudioQueueAllocateBuffer(queue, BUFFER_SIZE, &buffer);
            if(r) printf("AudioQueueAllocateBuffer() failed");
            buffer->mAudioDataByteSize = BUFFER_SIZE;
            memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
            AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
        }
        r = AudioQueueStart(queue, NULL);
        if(r) printf("AudioQueueStart() failed");
    }
    
    ~Backend() {
        AudioQueueStop(queue, true);
        AudioQueueDispose(queue, true);
    }
    
    AudioQueueRef queue;
};

#elif defined(AUDIOLIB_BACKEND_OPENSLES)
static void fill_buffer(SLBufferQueueItf, void*);
struct Backend {
    Backend(Manager *mgr) {
        auto r = slCreateEngine(&engine, 0, nullptr, 0, NULL, NULL);
        assert(r == SL_RESULT_SUCCESS);
        (*engine)->Realize(engine, SL_BOOLEAN_FALSE);

        SLEngineItf engine_itf;
        (*engine)->GetInterface(engine, SL_IID_ENGINE, &engine_itf);
        r = (*engine_itf)->CreateOutputMix(engine_itf, &output, 0, NULL, NULL);
        assert(r == SL_RESULT_SUCCESS);
        (*output)->Realize(output, SL_BOOLEAN_FALSE);

        SLDataFormat_PCM format;
        format.formatType = SL_DATAFORMAT_PCM;
        format.numChannels = 2;
        format.samplesPerSec = SL_SAMPLINGRATE_44_1;
        format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
        format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
        format.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        format.endianness = SL_BYTEORDER_LITTLEENDIAN;

        SLDataLocator_AndroidSimpleBufferQueue loc_source = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
        SLDataLocator_OutputMix loc_sink = { SL_DATALOCATOR_OUTPUTMIX, output };
        SLDataSource data_src = {&loc_source, &format };
        SLDataSink data_sink = {&loc_sink, nullptr };
        SLInterfaceID audio_itf[] = {SL_IID_BUFFERQUEUE, SL_IID_PLAY  };
        SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
        SLPlayItf play_itf  = nullptr;

        (*engine_itf)->CreateAudioPlayer(engine_itf, &player, &data_src, &data_sink, 2, audio_itf, req);
        (*player)->Realize(player, SL_BOOLEAN_FALSE);
        (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &queue);
        (*player)->GetInterface(player, SL_IID_PLAY, &play_itf);
        (*queue)->RegisterCallback(queue, fill_buffer, mgr);
        (*play_itf)->SetPlayState(play_itf, SL_PLAYSTATE_PLAYING);
        memset(buffer[0], 0, BUFFER_SIZE);
        (*queue)->Enqueue(queue, buffer[0], BUFFER_SIZE);
    }

    ~Backend() {
        if(player) (*player)->Destroy(player);
        if(output) (*output)->Destroy(output);
        if(engine) (*engine)->Destroy(engine);
    }

    SLObjectItf engine, output, player;
    SLBufferQueueItf queue = nullptr;
    uint8_t buffer[2][BUFFER_SIZE];
};
#endif

/************************************************************************
 * Manager
 ************************************************************************/

inline void low(uint8_t &c) { if((c>191 && c<224) || (c>64 && c<91)) c += 32; }
static std::string strlow(const std::string &_str) {
    std::string temp(_str);
    for(auto &c : temp) low(reinterpret_cast<uint8_t&>(c));
    return temp;
}

class Manager {
public:
    Manager() {
        temp_buf = new int16_t[BUFFER_SIZE / sizeof(int16_t)];
        backend = new Backend(this);
    }
    
    ~Manager() {
        delete backend;
        delete [] temp_buf;
    }
    
    Sound *load(const std::string &path, int32_t _is_loop, int32_t *err) {
        std::string ext;
        size_t dot = path.rfind('.');
        if(dot != std::string::npos) ext = path.substr(dot+1, path.length() - dot - 1);
        ext = strlow(ext);
        
        Sound *ret = nullptr;
        if(ext == "wav") ret = new SoundWAV();
        else if(ext == "ogg") ret = new SoundOGG();
        else return nullptr;
        
        *err = ret->load(path,_is_loop);
        sounds.push_back(ret);
        return ret;
    }

    template<class T> Sound *load() {
        auto ret = new T();
        ret->load("",true);
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

    Backend *getBackend() const { return backend; }
    
private:
    Backend *backend = nullptr;
    int16_t *temp_buf = nullptr;
    std::vector<Sound*> sounds;
    void *prev_buffer = nullptr;
};

/************************************************************************
 * Backend callbacks
 ************************************************************************/

#ifdef AUDIOLIB_BACKEND_AUDIOTOOLBOX
static void fill_buffer(void* in_user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    auto manager = static_cast<Manager*>(in_user_data);
    buffer->mAudioDataByteSize = BUFFER_SIZE;
    memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
    manager->fillBuffer(buffer->mAudioData, buffer->mAudioDataBytesCapacity / SAMPLE_SIZE);
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

#elif defined(AUDIOLIB_BACKEND_OPENSLES)
static void fill_buffer(SLBufferQueueItf bq, void *context) {
    static size_t i = 0;
    auto manager = static_cast<Manager*>(context);
    auto backend = manager->getBackend();
    auto data = backend->buffer[i];
    memset(data, 0, BUFFER_SIZE);
    manager->fillBuffer(data, SAMPLE_COUNT);
    (*backend->queue)->Enqueue(backend->queue, data, BUFFER_SIZE);
    i ^= 1;
}
#endif

} // namespace AudioLib
