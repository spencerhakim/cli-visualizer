/*
 * MacOsXAudioSource.cpp
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 */

#include "Source/MacOsXAudioSource.h"
#include "Utils/Logger.h"

vis::MacOsXAudioSource::MacOsXAudioSource(const vis::Settings *const settings)
#ifdef _OS_OSX
    : m_device_id{0u}, m_proc_id{nullptr}, m_settings{settings}
{
    // get default input device
    // setup handle
    // start CFRun thread
    m_settings->get_fps();

    m_buffers = new moodycamel::ReaderWriterQueue<int32_t>(1000000);

    m_device_id = get_default_input_device();
    print_device_info();
    print_stream_info();

    auto result = AudioDeviceCreateIOProcID(
        m_device_id,
        vis::MacOsXAudioSource::core_audio_handle,
        this,
        &m_proc_id
    );
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "AudioDeviceAddIOProc failed: %s", result);
        return;
    }

    result = AudioDeviceStart(m_device_id, m_proc_id);
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "AudioDeviceStart failed: %s", result);
        return;
    }

    m_run_loop_thread = std::thread(start_run_loop);
}
#else
{}
#endif

vis::MacOsXAudioSource::~MacOsXAudioSource()
{
#ifdef _OS_OSX
    // TODO: reap thread
    CFRunLoopStop(CFRunLoopGetCurrent());

    m_run_loop_thread.join();

    AudioDeviceStop(m_device_id, m_proc_id);
    AudioDeviceDestroyIOProcID(m_device_id, m_proc_id);

    delete m_buffers;
#endif
}

#ifdef _OS_OSX
char *CFStringToUTF8String(CFStringRef aString);
char *CFStringToUTF8String(CFStringRef aString)
{
    if (!aString)
    {
        return nullptr;
    }

    auto length = CFStringGetLength(aString);
    auto maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char *buffer = static_cast<char *>(malloc(static_cast<size_t>(maxSize)));
    if (CFStringGetCString(aString, buffer, maxSize, kCFStringEncodingUTF8))
    {
        return buffer;
    }

    return nullptr;
}

AudioDeviceID vis::MacOsXAudioSource::get_default_input_device()
{
    AudioDeviceID device_id;
    UInt32 property_size = sizeof(device_id);

    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    // get the ID of the default input device
    auto result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &property_address,
        CORE_AUDIO_OSX_CHANNEL,
        nullptr,
        &property_size,
        &device_id
    );
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "Error getting default device: %s", result);
        return 0;
    }

    return device_id;
}

OSStatus vis::MacOsXAudioSource::core_audio_handle(
    AudioDeviceID,
    const AudioTimeStamp *,
    const AudioBufferList *inputData,
    const AudioTimeStamp *,
    AudioBufferList *,
    const AudioTimeStamp *,
    void *clientData)
{
    auto that = static_cast<vis::MacOsXAudioSource *>(clientData);
    const AudioBuffer *buf = &inputData->mBuffers[0];

    auto numFrames = static_cast<size_t>(buf->mDataByteSize / sizeof(int32_t));
    auto frames = static_cast<int32_t *>(buf->mData);

    for (size_t i = 0; i < numFrames; i++)
    {
        that->m_buffers->enqueue( frames[i * sizeof(int32_t)] );
    }

    return 0;
}

void vis::MacOsXAudioSource::print_stream_info()
{
    AudioStreamBasicDescription description;
    UInt32 propertySize = sizeof(AudioStreamBasicDescription);

    AudioObjectPropertyAddress deviceAddress;
    deviceAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
    deviceAddress.mScope = kAudioDevicePropertyScopeOutput;
    deviceAddress.mElement = kAudioObjectPropertyElementMaster;

    auto result = AudioObjectGetPropertyData(
        m_device_id,
        &deviceAddress,
        CORE_AUDIO_OSX_CHANNEL,
        nullptr,
        &propertySize,
        &description
    );
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "Failed to get info on stream");
        return;
    }

    // format ID to string name
    char formatIDString[5] = {0, 0, 0, 0, 0};
    UInt32 formatID = CFSwapInt32HostToBig(description.mFormatID);
    memcpy(formatIDString, &formatID, 4);

    VIS_LOG(
        vis::LogLevel::INFO, "Sample rate: %f\tFormat: %4.4s\tmFormatFlags: %u",
        description.mSampleRate, &formatIDString, description.mFormatFlags
    );
}

void vis::MacOsXAudioSource::print_device_info()
{
    char deviceName[64];
    char manufacturerName[64];

    UInt32 propertySize = sizeof(deviceName);
    AudioObjectPropertyAddress deviceAddress;

    deviceAddress.mSelector = kAudioDevicePropertyDeviceName;
    deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
    deviceAddress.mElement = kAudioObjectPropertyElementMaster;

    auto result = AudioObjectGetPropertyData(
        m_device_id,
        &deviceAddress,
        CORE_AUDIO_OSX_CHANNEL,
        nullptr,
        &propertySize,
        deviceName
    );
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "Could not get device name");
        return;
    }

    propertySize = sizeof(manufacturerName);
    deviceAddress.mSelector = kAudioDevicePropertyDeviceManufacturer;
    deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
    deviceAddress.mElement = kAudioObjectPropertyElementMaster;

    result = AudioObjectGetPropertyData(
        m_device_id,
        &deviceAddress,
        CORE_AUDIO_OSX_CHANNEL,
        nullptr,
        &propertySize,
        manufacturerName
    );
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "Could not get device manufacturer name");
        return;
    }

    CFStringRef uidString;
    propertySize = sizeof(uidString);

    deviceAddress.mSelector = kAudioDevicePropertyDeviceUID;
    deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
    deviceAddress.mElement = kAudioObjectPropertyElementMaster;

    result = AudioObjectGetPropertyData(
        m_device_id,
        &deviceAddress,
        CORE_AUDIO_OSX_CHANNEL,
        nullptr,
        &propertySize,
        &uidString
    );
    if (result != noErr)
    {
        VIS_LOG(vis::LogLevel::ERROR, "Could not get device UID");
        return;
    }

    char *uid = CFStringToUTF8String(uidString);
    VIS_LOG(
        vis::LogLevel::INFO, "Device %s\t%s\t%s",
        deviceName, manufacturerName, uid
    );
    free(uid);
    CFRelease(uidString);
}
#endif

bool vis::MacOsXAudioSource::read(pcm_stereo_sample *buffer, const uint32_t buffer_size)
{
    size_t buffer_size_bytes = static_cast<size_t>(sizeof(pcm_stereo_sample) * buffer_size);
    memset(buffer, 0, buffer_size_bytes);

#ifdef _OS_OSX
    int32_t frame = 0;

    for (auto i = 0u; i < buffer_size; i++)
    {
        if (!m_buffers->try_dequeue(frame))
        {
            VIS_LOG(vis::LogLevel::WARN, "Queue depleted");
            break;
        }

        memcpy(&buffer[i], &frame, sizeof(int32_t));
    }

    return true;
#else
    return false;
#endif
}
