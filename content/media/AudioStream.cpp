/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <math.h>
#include "prlog.h"
#include "prdtoa.h"
#include "AudioStream.h"
#include "VideoUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include <algorithm>
#include "mozilla/Preferences.h"
#include "soundtouch/SoundTouch.h"
#include "Latency.h"

#if defined(MOZ_CUBEB)
#include "nsAutoRef.h"
#include "cubeb/cubeb.h"

template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream>
{
public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

#endif

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nullptr;
#endif

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_CUBEB_LATENCY "media.cubeb_latency_ms"

static Mutex* gAudioPrefsLock = nullptr;
static double gVolumeScale;
static uint32_t gCubebLatency;
static bool gCubebLatencyPrefSet;
static const uint32_t CUBEB_NORMAL_LATENCY_MS = 100;

StaticMutex AudioStream::mMutex;
uint32_t AudioStream::mPreferredSampleRate = 0;

/**
 * When MOZ_DUMP_AUDIO is set in the environment (to anything),
 * we'll drop a series of files in the current working directory named
 * dumped-audio-<nnn>.wav, one per nsBufferedAudioStream created, containing
 * the audio for the stream including any skips due to underruns.
 */
#if defined(MOZ_CUBEB)
static int gDumpedAudioCount = 0;
#endif

static int PrefChanged(const char* aPref, void* aClosure)
{
  if (strcmp(aPref, PREF_VOLUME_SCALE) == 0) {
    nsAdoptingString value = Preferences::GetString(aPref);
    MutexAutoLock lock(*gAudioPrefsLock);
    if (value.IsEmpty()) {
      gVolumeScale = 1.0;
    } else {
      NS_ConvertUTF16toUTF8 utf8(value);
      gVolumeScale = std::max<double>(0, PR_strtod(utf8.get(), nullptr));
    }
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY) == 0) {
    // Arbitrary default stream latency of 100ms.  The higher this
    // value, the longer stream volume changes will take to become
    // audible.
    gCubebLatencyPrefSet = Preferences::HasUserValue(aPref);
    uint32_t value = Preferences::GetUint(aPref, CUBEB_NORMAL_LATENCY_MS);
    MutexAutoLock lock(*gAudioPrefsLock);
    gCubebLatency = std::min<uint32_t>(std::max<uint32_t>(value, 1), 1000);
  }
  return 0;
}

#if defined(MOZ_CUBEB)
static double GetVolumeScale()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  return gVolumeScale;
}

static cubeb* gCubebContext;

static cubeb* GetCubebContext()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  if (gCubebContext ||
      cubeb_init(&gCubebContext, "AudioStream") == CUBEB_OK) {
    return gCubebContext;
  }
  NS_WARNING("cubeb_init failed");
  return nullptr;
}

static uint32_t GetCubebLatency()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  return gCubebLatency;
}

static bool CubebLatencyPrefSet()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  return gCubebLatencyPrefSet;
}
#endif

#if defined(MOZ_CUBEB) && defined(__ANDROID__) && defined(MOZ_B2G)
static cubeb_stream_type ConvertChannelToCubebType(dom::AudioChannelType aType)
{
  switch(aType) {
    case dom::AUDIO_CHANNEL_NORMAL:
      return CUBEB_STREAM_TYPE_SYSTEM;
    case dom::AUDIO_CHANNEL_CONTENT:
      return CUBEB_STREAM_TYPE_MUSIC;
    case dom::AUDIO_CHANNEL_NOTIFICATION:
      return CUBEB_STREAM_TYPE_NOTIFICATION;
    case dom::AUDIO_CHANNEL_ALARM:
      return CUBEB_STREAM_TYPE_ALARM;
    case dom::AUDIO_CHANNEL_TELEPHONY:
      return CUBEB_STREAM_TYPE_VOICE_CALL;
    case dom::AUDIO_CHANNEL_RINGER:
      return CUBEB_STREAM_TYPE_RING;
    // Currently Android openSLES library doesn't support FORCE_AUDIBLE yet.
    case dom::AUDIO_CHANNEL_PUBLICNOTIFICATION:
    default:
      NS_ERROR("The value of AudioChannelType is invalid");
      return CUBEB_STREAM_TYPE_MAX;
  }
}
#endif

AudioStream::AudioStream()
: mInRate(0),
  mOutRate(0),
  mChannels(0),
  mWritten(0),
  mAudioClock(MOZ_THIS_IN_INITIALIZER_LIST()),
  mLatencyRequest(HighLatency),
  mReadPoint(0)
{}

void AudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("AudioStream");
#endif
  gAudioPrefsLock = new Mutex("AudioStream::gAudioPrefsLock");
  PrefChanged(PREF_VOLUME_SCALE, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  PrefChanged(PREF_CUBEB_LATENCY, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY);
#endif
}

void AudioStream::ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY);
#endif
  delete gAudioPrefsLock;
  gAudioPrefsLock = nullptr;

#if defined(MOZ_CUBEB)
  if (gCubebContext) {
    cubeb_destroy(gCubebContext);
    gCubebContext = nullptr;
  }
#endif
}

AudioStream::~AudioStream()
{
}

nsresult AudioStream::EnsureTimeStretcherInitialized()
{
  if (!mTimeStretcher) {
    // SoundTouch does not support a number of channels > 2
    if (mChannels > 2) {
      return NS_ERROR_FAILURE;
    }
    mTimeStretcher = new soundtouch::SoundTouch();
    mTimeStretcher->setSampleRate(mInRate);
    mTimeStretcher->setChannels(mChannels);
    mTimeStretcher->setPitch(1.0);
  }
  return NS_OK;
}

nsresult AudioStream::SetPlaybackRate(double aPlaybackRate)
{
  NS_ASSERTION(aPlaybackRate > 0.0,
               "Can't handle negative or null playbackrate in the AudioStream.");
  // Avoid instantiating the resampler if we are not changing the playback rate.
  if (aPlaybackRate == mAudioClock.GetPlaybackRate()) {
    return NS_OK;
  }

  if (EnsureTimeStretcherInitialized() != NS_OK) {
    return NS_ERROR_FAILURE;
  }

  mAudioClock.SetPlaybackRate(aPlaybackRate);
  mOutRate = mInRate / aPlaybackRate;


  if (mAudioClock.GetPreservesPitch()) {
    mTimeStretcher->setTempo(aPlaybackRate);
    mTimeStretcher->setRate(1.0f);
  } else {
    mTimeStretcher->setTempo(1.0f);
    mTimeStretcher->setRate(aPlaybackRate);
  }
  return NS_OK;
}

nsresult AudioStream::SetPreservesPitch(bool aPreservesPitch)
{
  // Avoid instantiating the timestretcher instance if not needed.
  if (aPreservesPitch == mAudioClock.GetPreservesPitch()) {
    return NS_OK;
  }

  if (EnsureTimeStretcherInitialized() != NS_OK) {
    return NS_ERROR_FAILURE;
  }

  if (aPreservesPitch == true) {
    mTimeStretcher->setTempo(mAudioClock.GetPlaybackRate());
    mTimeStretcher->setRate(1.0f);
  } else {
    mTimeStretcher->setTempo(1.0f);
    mTimeStretcher->setRate(mAudioClock.GetPlaybackRate());
  }

  mAudioClock.SetPreservesPitch(aPreservesPitch);

  return NS_OK;
}

int64_t AudioStream::GetWritten()
{
  return mWritten;
}

#if defined(MOZ_CUBEB)
class nsCircularByteBuffer
{
public:
  nsCircularByteBuffer()
    : mBuffer(nullptr), mCapacity(0), mStart(0), mCount(0)
  {}

  // Set the capacity of the buffer in bytes.  Must be called before any
  // call to append or pop elements.
  void SetCapacity(uint32_t aCapacity) {
    NS_ABORT_IF_FALSE(!mBuffer, "Buffer allocated.");
    mCapacity = aCapacity;
    mBuffer = new uint8_t[mCapacity];
  }

  uint32_t Length() {
    return mCount;
  }

  uint32_t Capacity() {
    return mCapacity;
  }

  uint32_t Available() {
    return Capacity() - Length();
  }

  // Append aLength bytes from aSrc to the buffer.  Caller must check that
  // sufficient space is available.
  void AppendElements(const uint8_t* aSrc, uint32_t aLength) {
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aLength <= Available(), "Buffer full.");

    uint32_t end = (mStart + mCount) % mCapacity;

    uint32_t toCopy = std::min(mCapacity - end, aLength);
    memcpy(&mBuffer[end], aSrc, toCopy);
    memcpy(&mBuffer[0], aSrc + toCopy, aLength - toCopy);
    mCount += aLength;
  }

  // Remove aSize bytes from the buffer.  Caller must check returned size in
  // aSize{1,2} before using the pointer returned in aData{1,2}.  Caller
  // must not specify an aSize larger than Length().
  void PopElements(uint32_t aSize, void** aData1, uint32_t* aSize1,
                   void** aData2, uint32_t* aSize2) {
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aSize <= Length(), "Request too large.");

    *aData1 = &mBuffer[mStart];
    *aSize1 = std::min(mCapacity - mStart, aSize);
    *aData2 = &mBuffer[0];
    *aSize2 = aSize - *aSize1;
    mCount -= *aSize1 + *aSize2;
    mStart += *aSize1 + *aSize2;
    mStart %= mCapacity;
  }

private:
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mCapacity;
  uint32_t mStart;
  uint32_t mCount;
};

class BufferedAudioStream : public AudioStream
{
 public:
  BufferedAudioStream();
  ~BufferedAudioStream();

  nsresult Init(int32_t aNumChannels, int32_t aRate,
                const dom::AudioChannelType aAudioChannelType,
                AudioStream::LatencyRequest aLatencyRequest);
  void Shutdown();
  nsresult Write(const AudioDataValue* aBuf, uint32_t aFrames, TimeStamp *aTime = nullptr);
  uint32_t Available();
  void SetVolume(double aVolume);
  void Drain();
  void Start();
  void Pause();
  void Resume();
  int64_t GetPosition();
  int64_t GetPositionInFrames();
  int64_t GetPositionInFramesInternal();
  int64_t GetLatencyInFrames();
  bool IsPaused();
  void GetBufferInsertTime(int64_t &aTimeMs);
  // This method acquires the monitor and forward the call to the base
  // class, to prevent a race on |mTimeStretcher|, in
  // |AudioStream::EnsureTimeStretcherInitialized|.
  nsresult EnsureTimeStretcherInitialized();

private:
  static long DataCallback_S(cubeb_stream*, void* aThis, void* aBuffer, long aFrames)
  {
    return static_cast<BufferedAudioStream*>(aThis)->DataCallback(aBuffer, aFrames);
  }

  static void StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState)
  {
    static_cast<BufferedAudioStream*>(aThis)->StateCallback(aState);
  }

  long DataCallback(void* aBuffer, long aFrames);
  void StateCallback(cubeb_state aState);

  // aTime is the time in ms the samples were inserted into MediaStreamGraph
  long GetUnprocessed(void* aBuffer, long aFrames, int64_t &aTime);
  long GetTimeStretched(void* aBuffer, long aFrames, int64_t &aTime);
  long GetUnprocessedWithSilencePadding(void* aBuffer, long aFrames, int64_t &aTime);

  // Shared implementation of underflow adjusted position calculation.
  // Caller must own the monitor.
  int64_t GetPositionInFramesUnlocked();

  void StartUnlocked();

  // The monitor is held to protect all access to member variables.  Write()
  // waits while mBuffer is full; DataCallback() notifies as it consumes
  // data from mBuffer.  Drain() waits while mState is DRAINING;
  // StateCallback() notifies when mState is DRAINED.
  Monitor mMonitor;

  // Sum of silent frames written when DataCallback requests more frames
  // than are available in mBuffer.
  uint64_t mLostFrames;

  // Output file for dumping audio
  FILE* mDumpFile;

  // Temporary audio buffer.  Filled by Write() and consumed by
  // DataCallback().  Once mBuffer is full, Write() blocks until sufficient
  // space becomes available in mBuffer.  mBuffer is sized in bytes, not
  // frames.
  nsCircularByteBuffer mBuffer;

  // Software volume level.  Applied during the servicing of DataCallback().
  double mVolume;

  // Owning reference to a cubeb_stream.  cubeb_stream_destroy is called by
  // nsAutoRef's destructor.
  nsAutoRef<cubeb_stream> mCubebStream;

  uint32_t mBytesPerFrame;

  uint32_t BytesToFrames(uint32_t aBytes) {
    NS_ASSERTION(aBytes % mBytesPerFrame == 0,
                 "Byte count not aligned on frames size.");
    return aBytes / mBytesPerFrame;
  }

  uint32_t FramesToBytes(uint32_t aFrames) {
    return aFrames * mBytesPerFrame;
  }


  enum StreamState {
    INITIALIZED, // Initialized, playback has not begun.
    STARTED,     // Started by a call to Write() (iff INITIALIZED) or Resume().
    STOPPED,     // Stopped by a call to Pause().
    DRAINING,    // Drain requested.  DataCallback will indicate end of stream
                 // once the remaining contents of mBuffer are requested by
                 // cubeb, after which StateCallback will indicate drain
                 // completion.
    DRAINED,     // StateCallback has indicated that the drain is complete.
    ERRORED      // Stream disabled due to an internal error.
  };

  StreamState mState;
};
#endif

AudioStream* AudioStream::AllocateStream()
{
#if defined(MOZ_CUBEB)
  return new BufferedAudioStream();
#endif
  return nullptr;
}

int AudioStream::MaxNumberOfChannels()
{
#if defined(MOZ_CUBEB)
  uint32_t maxNumberOfChannels;

  if (cubeb_get_max_channel_count(GetCubebContext(),
                                  &maxNumberOfChannels) == CUBEB_OK) {
    return static_cast<int>(maxNumberOfChannels);
  }
#endif

  return 0;
}

int AudioStream::PreferredSampleRate()
{
  StaticMutexAutoLock lock(AudioStream::mMutex);
  // Get the preferred samplerate for this platform, or fallback to something
  // sensible if we fail. We cache the value, because this might be accessed
  // often, and the complexity of the function call below depends on the
  // backend used.
  const int fallbackSampleRate = 44100;
  if (mPreferredSampleRate == 0) {
#if defined(MOZ_CUBEB)
    if (cubeb_get_preferred_sample_rate(GetCubebContext(),
                                        &mPreferredSampleRate) == CUBEB_OK) {
      return mPreferredSampleRate;
    }
#endif
    mPreferredSampleRate = fallbackSampleRate;
  }

  return mPreferredSampleRate;
}

#if defined(MOZ_CUBEB)
static void SetUint16LE(uint8_t* aDest, uint16_t aValue)
{
  aDest[0] = aValue & 0xFF;
  aDest[1] = aValue >> 8;
}

static void SetUint32LE(uint8_t* aDest, uint32_t aValue)
{
  SetUint16LE(aDest, aValue & 0xFFFF);
  SetUint16LE(aDest + 2, aValue >> 16);
}

static FILE*
OpenDumpFile(AudioStream* aStream)
{
  if (!getenv("MOZ_DUMP_AUDIO"))
    return nullptr;
  char buf[100];
  sprintf(buf, "dumped-audio-%d.wav", gDumpedAudioCount);
  FILE* f = fopen(buf, "wb");
  if (!f)
    return nullptr;
  ++gDumpedAudioCount;

  uint8_t header[] = {
    // RIFF header
    0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45,
    // fmt chunk. We always write 16-bit samples.
    0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x10, 0x00,
    // data chunk
    0x64, 0x61, 0x74, 0x61, 0xFE, 0xFF, 0xFF, 0x7F
  };
  static const int CHANNEL_OFFSET = 22;
  static const int SAMPLE_RATE_OFFSET = 24;
  static const int BLOCK_ALIGN_OFFSET = 32;
  SetUint16LE(header + CHANNEL_OFFSET, aStream->GetChannels());
  SetUint32LE(header + SAMPLE_RATE_OFFSET, aStream->GetRate());
  SetUint16LE(header + BLOCK_ALIGN_OFFSET, aStream->GetChannels()*2);
  fwrite(header, sizeof(header), 1, f);

  return f;
}

static void
WriteDumpFile(FILE* aDumpFile, AudioStream* aStream, uint32_t aFrames,
              void* aBuffer)
{
  if (!aDumpFile)
    return;

  uint32_t samples = aStream->GetChannels()*aFrames;
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    fwrite(aBuffer, 2, samples, aDumpFile);
    return;
  }

  NS_ASSERTION(AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_FLOAT32, "bad format");
  nsAutoTArray<uint8_t, 1024*2> buf;
  buf.SetLength(samples*2);
  float* input = static_cast<float*>(aBuffer);
  uint8_t* output = buf.Elements();
  for (uint32_t i = 0; i < samples; ++i) {
    SetUint16LE(output + i*2, int16_t(input[i]*32767.0f));
  }
  fwrite(output, 2, samples, aDumpFile);
  fflush(aDumpFile);
}

BufferedAudioStream::BufferedAudioStream()
  : mMonitor("BufferedAudioStream"), mLostFrames(0), mDumpFile(nullptr),
    mVolume(1.0), mBytesPerFrame(0), mState(INITIALIZED)
{
  // keep a ref in case we shut down later than nsLayoutStatics
  mLatencyLog = AsyncLatencyLogger::Get(true);
}

BufferedAudioStream::~BufferedAudioStream()
{
  Shutdown();
  if (mDumpFile) {
    fclose(mDumpFile);
  }
}

nsresult
BufferedAudioStream::EnsureTimeStretcherInitialized()
{
  MonitorAutoLock mon(mMonitor);
  return AudioStream::EnsureTimeStretcherInitialized();
}

nsresult
BufferedAudioStream::Init(int32_t aNumChannels, int32_t aRate,
                          const dom::AudioChannelType aAudioChannelType,
                          AudioStream::LatencyRequest aLatencyRequest)
{
  cubeb* cubebContext = GetCubebContext();

  if (!cubebContext || aNumChannels < 0 || aRate < 0) {
    return NS_ERROR_FAILURE;
  }

  PR_LOG(gAudioStreamLog, PR_LOG_DEBUG,
    ("%s  channels: %d, rate: %d", __FUNCTION__, aNumChannels, aRate));
  mInRate = mOutRate = aRate;
  mChannels = aNumChannels;
  mLatencyRequest = aLatencyRequest;

  mDumpFile = OpenDumpFile(this);

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = aNumChannels;
#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  params.stream_type = ConvertChannelToCubebType(aAudioChannelType);
#else
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  if (params.stream_type == CUBEB_STREAM_TYPE_MAX) {
    return NS_ERROR_INVALID_ARG;
  }
#endif
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    params.format = CUBEB_SAMPLE_S16NE;
  } else {
    params.format = CUBEB_SAMPLE_FLOAT32NE;
  }
  mBytesPerFrame = sizeof(AudioDataValue) * aNumChannels;

  mAudioClock.Init();

  // If the latency pref is set, use it. Otherwise, if this stream is intended
  // for low latency playback, try to get the lowest latency possible.
  // Otherwise, for normal streams, use 100ms.
  uint32_t latency;
  if (aLatencyRequest == AudioStream::LowLatency && !CubebLatencyPrefSet()) {
    if (cubeb_get_min_latency(cubebContext, params, &latency) != CUBEB_OK) {
      latency = GetCubebLatency();
    }
  } else {
    latency = GetCubebLatency();
  }

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "BufferedAudioStream", params,
                          latency, DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
      mCubebStream.own(stream);
    }
  }

  if (!mCubebStream) {
    return NS_ERROR_FAILURE;
  }

  // Size mBuffer for one second of audio.  This value is arbitrary, and was
  // selected based on the observed behaviour of the existing AudioStream
  // implementations.
  uint32_t bufferLimit = FramesToBytes(aRate);
  NS_ABORT_IF_FALSE(bufferLimit % mBytesPerFrame == 0, "Must buffer complete frames");
  mBuffer.SetCapacity(bufferLimit);

  // Start the stream right away when low latency has been requested. This means
  // that the DataCallback will feed silence to cubeb, until the first frames
  // are writtent to this BufferedAudioStream.
  if (mLatencyRequest == AudioStream::LowLatency) {
    Start();
  }

  return NS_OK;
}

void
BufferedAudioStream::Shutdown()
{
  if (mState == STARTED) {
    Pause();
  }
  if (mCubebStream) {
    mCubebStream.reset();
  }
}

// aTime is the time in ms the samples were inserted into MediaStreamGraph
nsresult
BufferedAudioStream::Write(const AudioDataValue* aBuf, uint32_t aFrames, TimeStamp *aTime)
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState == ERRORED) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(mState == INITIALIZED || mState == STARTED,
    "Stream write in unexpected state.");

  const uint8_t* src = reinterpret_cast<const uint8_t*>(aBuf);
  uint32_t bytesToCopy = FramesToBytes(aFrames);

  // XXX this will need to change if we want to enable this on-the-fly!
  if (PR_LOG_TEST(GetLatencyLog(), PR_LOG_DEBUG)) {
    // Record the position and time this data was inserted
    int64_t timeMs;
    if (aTime && !aTime->IsNull()) {
      if (mStartTime.IsNull()) {
        AsyncLatencyLogger::Get(true)->GetStartTime(mStartTime);
      }
      timeMs = (*aTime - mStartTime).ToMilliseconds();
    } else {
      timeMs = 0;
    }
    struct Inserts insert = { timeMs, aFrames};
    mInserts.AppendElement(insert);
  }

  while (bytesToCopy > 0) {
    uint32_t available = std::min(bytesToCopy, mBuffer.Available());
    NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0,
        "Must copy complete frames.");

    mBuffer.AppendElements(src, available);
    src += available;
    bytesToCopy -= available;

    if (bytesToCopy > 0) {
      // If we are not playing, but our buffer is full, start playing to make
      // room for soon-to-be-decoded data.
      if (mState != STARTED) {
        StartUnlocked();
        if (mState != STARTED) {
          return NS_ERROR_FAILURE;
        }
      }
      mon.Wait();
    }
  }

  mWritten += aFrames;
  return NS_OK;
}

uint32_t
BufferedAudioStream::Available()
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Buffer invariant violated.");
  return BytesToFrames(mBuffer.Available());
}

void
BufferedAudioStream::SetVolume(double aVolume)
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
  mVolume = aVolume;
}

void
BufferedAudioStream::Drain()
{
  MonitorAutoLock mon(mMonitor);
  if (mState != STARTED) {
    NS_ASSERTION(mBuffer.Available() == 0, "Draining with unplayed audio");
    return;
  }
  mState = DRAINING;
  while (mState == DRAINING) {
    mon.Wait();
  }
}

void
BufferedAudioStream::Start()
{
  MonitorAutoLock mon(mMonitor);
  StartUnlocked();
}

void
BufferedAudioStream::StartUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mCubebStream || mState != INITIALIZED) {
    return;
  }
  if (mState != STARTED) {
    int r;
    {
      MonitorAutoUnlock mon(mMonitor);
      r = cubeb_stream_start(mCubebStream);
    }
    if (mState != ERRORED) {
      mState = r == CUBEB_OK ? STARTED : ERRORED;
    }
  }
}

void
BufferedAudioStream::Pause()
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState != STARTED) {
    return;
  }

  int r;
  {
    MonitorAutoUnlock mon(mMonitor);
    r = cubeb_stream_stop(mCubebStream);
  }
  if (mState != ERRORED && r == CUBEB_OK) {
    mState = STOPPED;
  }
}

void
BufferedAudioStream::Resume()
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState != STOPPED) {
    return;
  }

  int r;
  {
    MonitorAutoUnlock mon(mMonitor);
    r = cubeb_stream_start(mCubebStream);
  }
  if (mState != ERRORED && r == CUBEB_OK) {
    mState = STARTED;
  }
}

int64_t
BufferedAudioStream::GetPosition()
{
  return mAudioClock.GetPosition();
}

// This function is miscompiled by PGO with MSVC 2010.  See bug 768333.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
int64_t
BufferedAudioStream::GetPositionInFrames()
{
  return mAudioClock.GetPositionInFrames();
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

int64_t
BufferedAudioStream::GetPositionInFramesInternal()
{
  MonitorAutoLock mon(mMonitor);
  return GetPositionInFramesUnlocked();
}

int64_t
BufferedAudioStream::GetPositionInFramesUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();

  if (!mCubebStream || mState == ERRORED) {
    return -1;
  }

  uint64_t position = 0;
  {
    MonitorAutoUnlock mon(mMonitor);
    if (cubeb_stream_get_position(mCubebStream, &position) != CUBEB_OK) {
      return -1;
    }
  }

  // Adjust the reported position by the number of silent frames written
  // during stream underruns.
  uint64_t adjustedPosition = 0;
  if (position >= mLostFrames) {
    adjustedPosition = position - mLostFrames;
  }
  return std::min<uint64_t>(adjustedPosition, INT64_MAX);
}

int64_t
BufferedAudioStream::GetLatencyInFrames()
{
  uint32_t latency;
  if(cubeb_stream_get_latency(mCubebStream, &latency)) {
    NS_WARNING("Could not get cubeb latency.");
    return 0;
  }
  return static_cast<int64_t>(latency);
}

bool
BufferedAudioStream::IsPaused()
{
  MonitorAutoLock mon(mMonitor);
  return mState == STOPPED;
}

void
BufferedAudioStream::GetBufferInsertTime(int64_t &aTimeMs)
{
  if (mInserts.Length() > 0) {
    // Find the right block, but don't leave the array empty
    while (mInserts.Length() > 1 && mReadPoint >= mInserts[0].mFrames) {
      mReadPoint -= mInserts[0].mFrames;
      mInserts.RemoveElementAt(0);
    }
    // offset for amount already read
    // XXX Note: could misreport if we couldn't find a block in the right timeframe
    aTimeMs = mInserts[0].mTimeMs + ((mReadPoint * 1000) / mOutRate);
  } else {
    aTimeMs = INT64_MAX;
  }
}

long
BufferedAudioStream::GetUnprocessed(void* aBuffer, long aFrames, int64_t &aTimeMs)
{
  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);

  // Flush the timestretcher pipeline, if we were playing using a playback rate
  // other than 1.0.
  uint32_t flushedFrames = 0;
  if (mTimeStretcher && mTimeStretcher->numSamples()) {
    flushedFrames = mTimeStretcher->receiveSamples(reinterpret_cast<AudioDataValue*>(wpos), aFrames);
    wpos += FramesToBytes(flushedFrames);
  }
  uint32_t toPopBytes = FramesToBytes(aFrames - flushedFrames);
  uint32_t available = std::min(toPopBytes, mBuffer.Length());

  void* input[2];
  uint32_t input_size[2];
  mBuffer.PopElements(available, &input[0], &input_size[0], &input[1], &input_size[1]);
  memcpy(wpos, input[0], input_size[0]);
  wpos += input_size[0];
  memcpy(wpos, input[1], input_size[1]);

  // First time block now has our first returned sample
  mReadPoint += BytesToFrames(available);
  GetBufferInsertTime(aTimeMs);

  return BytesToFrames(available) + flushedFrames;
}

// Get unprocessed samples, and pad the beginning of the buffer with silence if
// there is not enough data.
long
BufferedAudioStream::GetUnprocessedWithSilencePadding(void* aBuffer, long aFrames, int64_t& aTimeMs)
{
  uint32_t toPopBytes = FramesToBytes(aFrames);
  uint32_t available = std::min(toPopBytes, mBuffer.Length());
  uint32_t silenceOffset = toPopBytes - available;

  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);

  memset(wpos, 0, silenceOffset);
  wpos += silenceOffset;

  void* input[2];
  uint32_t input_size[2];
  mBuffer.PopElements(available, &input[0], &input_size[0], &input[1], &input_size[1]);
  memcpy(wpos, input[0], input_size[0]);
  wpos += input_size[0];
  memcpy(wpos, input[1], input_size[1]);

  GetBufferInsertTime(aTimeMs);

  return aFrames;
}

long
BufferedAudioStream::GetTimeStretched(void* aBuffer, long aFrames, int64_t &aTimeMs)
{
  long processedFrames = 0;

  // We need to call the non-locking version, because we already have the lock.
  if (AudioStream::EnsureTimeStretcherInitialized() != NS_OK) {
    return 0;
  }

  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);
  double playbackRate = static_cast<double>(mInRate) / mOutRate;
  uint32_t toPopBytes = FramesToBytes(ceil(aFrames / playbackRate));
  uint32_t available = 0;
  bool lowOnBufferedData = false;
  do {
    // Check if we already have enough data in the time stretcher pipeline.
    if (mTimeStretcher->numSamples() <= static_cast<uint32_t>(aFrames)) {
      void* input[2];
      uint32_t input_size[2];
      available = std::min(mBuffer.Length(), toPopBytes);
      if (available != toPopBytes) {
        lowOnBufferedData = true;
      }
      mBuffer.PopElements(available, &input[0], &input_size[0],
                                     &input[1], &input_size[1]);
      mReadPoint += BytesToFrames(available);
      for(uint32_t i = 0; i < 2; i++) {
        mTimeStretcher->putSamples(reinterpret_cast<AudioDataValue*>(input[i]), BytesToFrames(input_size[i]));
      }
    }
    uint32_t receivedFrames = mTimeStretcher->receiveSamples(reinterpret_cast<AudioDataValue*>(wpos), aFrames - processedFrames);
    wpos += FramesToBytes(receivedFrames);
    processedFrames += receivedFrames;
  } while (processedFrames < aFrames && !lowOnBufferedData);

  GetBufferInsertTime(aTimeMs);

  return processedFrames;
}

long
BufferedAudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  uint32_t available = std::min(static_cast<uint32_t>(FramesToBytes(aFrames)), mBuffer.Length());
  NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames");
  AudioDataValue* output = reinterpret_cast<AudioDataValue*>(aBuffer);
  uint32_t underrunFrames = 0;
  uint32_t servicedFrames = 0;
  int64_t insertTime;

  if (available) {
    // When we are playing a low latency stream, and it is the first time we are
    // getting data from the buffer, we prefer to add the silence for an
    // underrun at the beginning of the buffer, so the first buffer is not cut
    // in half by the silence inserted to compensate for the underrun.
    if (mInRate == mOutRate) {
      if (mLatencyRequest == AudioStream::LowLatency && !mWritten) {
        servicedFrames = GetUnprocessedWithSilencePadding(output, aFrames, insertTime);
      } else {
        servicedFrames = GetUnprocessed(output, aFrames, insertTime);
      }
    } else {
      servicedFrames = GetTimeStretched(output, aFrames, insertTime);
    }
    float scaled_volume = float(GetVolumeScale() * mVolume);

    ScaleAudioSamples(output, aFrames * mChannels, scaled_volume);

    NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

    // Notify any blocked Write() call that more space is available in mBuffer.
    mon.NotifyAll();
  } else {
    GetBufferInsertTime(insertTime);
  }

  underrunFrames = aFrames - servicedFrames;

  if (mState != DRAINING) {
    uint8_t* rpos = static_cast<uint8_t*>(aBuffer) + FramesToBytes(aFrames - underrunFrames);
    memset(rpos, 0, FramesToBytes(underrunFrames));
    if (underrunFrames) {
      PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
             ("AudioStream %p lost %d frames", this, underrunFrames));
    }
    mLostFrames += underrunFrames;
    servicedFrames += underrunFrames;
  }

  WriteDumpFile(mDumpFile, this, aFrames, aBuffer);
  // Don't log if we're not interested or if the stream is inactive
  if (PR_LOG_TEST(GetLatencyLog(), PR_LOG_DEBUG) &&
      insertTime != INT64_MAX && servicedFrames > underrunFrames) {
    uint32_t latency = UINT32_MAX;
    if (cubeb_stream_get_latency(mCubebStream, &latency)) {
      NS_WARNING("Could not get latency from cubeb.");
    }
    TimeStamp now = TimeStamp::Now();

    mLatencyLog->Log(AsyncLatencyLogger::AudioStream, reinterpret_cast<uint64_t>(this),
                     insertTime, now);
    mLatencyLog->Log(AsyncLatencyLogger::Cubeb, reinterpret_cast<uint64_t>(mCubebStream.get()),
                     (latency * 1000) / mOutRate, now);
  }

  mAudioClock.UpdateWritePosition(servicedFrames);
  return servicedFrames;
}

void
BufferedAudioStream::StateCallback(cubeb_state aState)
{
  MonitorAutoLock mon(mMonitor);
  if (aState == CUBEB_STATE_DRAINED) {
    mState = DRAINED;
  } else if (aState == CUBEB_STATE_ERROR) {
    mState = ERRORED;
  }
  mon.NotifyAll();
}

#endif

AudioClock::AudioClock(AudioStream* aStream)
 :mAudioStream(aStream),
  mOldOutRate(0),
  mBasePosition(0),
  mBaseOffset(0),
  mOldBaseOffset(0),
  mOldBasePosition(0),
  mPlaybackRateChangeOffset(0),
  mPreviousPosition(0),
  mWritten(0),
  mOutRate(0),
  mInRate(0),
  mPreservesPitch(true),
  mCompensatingLatency(false)
{}

void AudioClock::Init()
{
  mOutRate = mAudioStream->GetRate();
  mInRate = mAudioStream->GetRate();
  mOldOutRate = mOutRate;
}

void AudioClock::UpdateWritePosition(uint32_t aCount)
{
  mWritten += aCount;
}

uint64_t AudioClock::GetPosition()
{
  int64_t position = mAudioStream->GetPositionInFramesInternal();
  int64_t diffOffset;
  NS_ASSERTION(position < 0 || (mInRate != 0 && mOutRate != 0), "AudioClock not initialized.");
  if (position >= 0) {
    if (position < mPlaybackRateChangeOffset) {
      // See if we are still playing frames pushed with the old playback rate in
      // the backend. If we are, use the old output rate to compute the
      // position.
      mCompensatingLatency = true;
      diffOffset = position - mOldBaseOffset;
      position = static_cast<uint64_t>(mOldBasePosition +
        static_cast<float>(USECS_PER_S * diffOffset) / mOldOutRate);
      mPreviousPosition = position;
      return position;
    }

    if (mCompensatingLatency) {
      diffOffset = position - mPlaybackRateChangeOffset;
      mCompensatingLatency = false;
      mBasePosition = mPreviousPosition;
    } else {
      diffOffset = position - mPlaybackRateChangeOffset;
    }
    position =  static_cast<uint64_t>(mBasePosition +
      (static_cast<float>(USECS_PER_S * diffOffset) / mOutRate));
    return position;
  }
  return UINT64_MAX;
}

uint64_t AudioClock::GetPositionInFrames()
{
  return (GetPosition() * mOutRate) / USECS_PER_S;
}

void AudioClock::SetPlaybackRate(double aPlaybackRate)
{
  int64_t position = mAudioStream->GetPositionInFramesInternal();
  if (position > mPlaybackRateChangeOffset) {
    mOldBasePosition = mBasePosition;
    mBasePosition = GetPosition();
    mOldBaseOffset = mPlaybackRateChangeOffset;
    mBaseOffset = position;
    mPlaybackRateChangeOffset = mWritten;
    mOldOutRate = mOutRate;
    mOutRate = static_cast<int>(mInRate / aPlaybackRate);
  } else {
    // The playbackRate has been changed before the end of the latency
    // compensation phase. We don't update the mOld* variable. That way, the
    // last playbackRate set is taken into account.
    mBasePosition = GetPosition();
    mBaseOffset = position;
    mPlaybackRateChangeOffset = mWritten;
    mOutRate = static_cast<int>(mInRate / aPlaybackRate);
  }
}

double AudioClock::GetPlaybackRate()
{
  return static_cast<double>(mInRate) / mOutRate;
}

void AudioClock::SetPreservesPitch(bool aPreservesPitch)
{
  mPreservesPitch = aPreservesPitch;
}

bool AudioClock::GetPreservesPitch()
{
  return mPreservesPitch;
}
} // namespace mozilla
