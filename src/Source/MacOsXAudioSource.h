/*
 * MacOsXAudioSource.h
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 */

#ifndef _VIS_MAC_OS_X_AUDIO_SOURCE_H
#define _VIS_MAC_OS_X_AUDIO_SOURCE_H

#include "Domain/Settings.h"
#include "Domain/VisTypes.h"
#include "Source/AudioSource.h"
#include <cstdint>
#include <cstring>

#ifdef _OS_OSX
#include "Utils/rwqueue.h"
#include <thread>
#include <CoreAudio/CoreAudio.h>
#define CORE_AUDIO_OSX_CHANNEL 0
#endif

namespace vis
{

class MacOsXAudioSource : public vis::AudioSource
{
  public:
    explicit MacOsXAudioSource(const vis::Settings *const settings);
    ~MacOsXAudioSource() override;

    /**
     * Reads "buffer_size" frames of the audio stream into "buffer"
     */
    bool read(pcm_stereo_sample *buffer, uint32_t buffer_size) override;

#ifdef _OS_OSX
  private:
    AudioDeviceID m_device_id;
    AudioDeviceIOProcID m_proc_id;
    std::thread m_run_loop_thread;
    moodycamel::ReaderWriterQueue<int32_t> *m_buffers;
    const vis::Settings * const m_settings;

    AudioDeviceID get_default_input_device();
    void print_device_info();
    void print_stream_info();

    static void start_run_loop() { CFRunLoopRun(); }
    static OSStatus core_audio_handle(
        AudioDeviceID id,
        const AudioTimeStamp *now,
        const AudioBufferList *inputData,
        const AudioTimeStamp *inputTime,
        AudioBufferList *outputData,
        const AudioTimeStamp *outputTime,
        void *clientData
    );
#endif
};
}

#endif
