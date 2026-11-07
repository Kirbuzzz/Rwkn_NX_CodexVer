/* opensl_types.h -- tipos minimos de OpenSL ES 1.0.1 + extensiones de Android, escritos a mano
 * para no depender de los headers del NDK. El orden de las tablas de funciones es el de la especificacion. */
#ifndef __OPENSL_TYPES_H__
#define __OPENSL_TYPES_H__

#include <stdint.h>

typedef uint32_t SLuint32;
typedef int32_t  SLint32;
typedef uint16_t SLuint16;
typedef uint8_t  SLuint8;
typedef int16_t  SLint16;
typedef SLuint32 SLresult;
typedef SLuint32 SLboolean;

#define SL_RESULT_SUCCESS               0u
#define SL_RESULT_PRECONDITIONS_VIOLATED 1u
#define SL_RESULT_PARAMETER_INVALID     2u
#define SL_RESULT_MEMORY_FAILURE        3u
#define SL_RESULT_BUFFER_INSUFFICIENT   7u
#define SL_RESULT_FEATURE_UNSUPPORTED  12u

#define SL_PLAYSTATE_STOPPED 1u
#define SL_PLAYSTATE_PAUSED  2u
#define SL_PLAYSTATE_PLAYING 3u

#define SL_OBJECT_STATE_UNREALIZED 1u
#define SL_OBJECT_STATE_REALIZED   2u

#define SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE 0x800007BDu
#define SL_DATAFORMAT_PCM            2u
#define SL_ANDROID_DATAFORMAT_PCM_EX 4u
#define SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT   1u
#define SL_ANDROID_PCM_REPRESENTATION_UNSIGNED_INT 2u
#define SL_ANDROID_PCM_REPRESENTATION_FLOAT        3u

struct SLInterfaceID_ { SLuint32 time_low; SLuint16 time_mid, time_hi_and_version, clock_seq; SLuint8 node[6]; };
typedef const struct SLInterfaceID_ *SLInterfaceID;

typedef struct { void *pLocator; void *pFormat; } SLDataSource;
typedef struct { void *pLocator; void *pFormat; } SLDataSink;
typedef struct { SLuint32 locatorType; SLuint32 numBuffers; } SLDataLocator_AndroidSimpleBufferQueue;
typedef struct { SLuint32 formatType, numChannels, samplesPerSec, bitsPerSample, containerSize, channelMask, endianness; } SLDataFormat_PCM;
typedef struct { SLuint32 formatType, numChannels, sampleRate, bitsPerSample, containerSize, channelMask, endianness, representation; } SLAndroidDataFormat_PCM_EX;
typedef struct { SLuint32 count; SLuint32 index; } SLAndroidSimpleBufferQueueState;

/* ---- objeto ---- */
typedef const struct SLObjectItf_ *const *SLObjectItf;
struct SLObjectItf_ {
  SLresult (*Realize)(SLObjectItf self, SLboolean async);
  SLresult (*Resume)(SLObjectItf self, SLboolean async);
  SLresult (*GetState)(SLObjectItf self, SLuint32 *pState);
  SLresult (*GetInterface)(SLObjectItf self, SLInterfaceID iid, void *pInterface);
  SLresult (*RegisterCallback)(SLObjectItf self, void *callback, void *pContext);
  void     (*AbortAsyncOperation)(SLObjectItf self);
  void     (*Destroy)(SLObjectItf self);
  SLresult (*SetPriority)(SLObjectItf self, SLint32 priority, SLboolean preemptable);
  SLresult (*GetPriority)(SLObjectItf self, SLint32 *pPriority, SLboolean *pPreemptable);
  SLresult (*SetLossOfControlInterfaces)(SLObjectItf self, SLint16 n, SLInterfaceID *ids, SLboolean enabled);
};

/* ---- motor ---- */
typedef const struct SLEngineItf_ *const *SLEngineItf;
typedef SLresult (*sl_create_fn)(SLEngineItf self, SLObjectItf *pObj, SLuint32 a, SLuint32 b, const SLInterfaceID *ids, const SLboolean *req);
struct SLEngineItf_ {
  sl_create_fn CreateLEDDevice;
  sl_create_fn CreateVibraDevice;
  SLresult (*CreateAudioPlayer)(SLEngineItf self, SLObjectItf *pPlayer, SLDataSource *src, SLDataSink *snk,
                                SLuint32 numInterfaces, const SLInterfaceID *ids, const SLboolean *req);
  void *CreateAudioRecorder;
  void *CreateMidiPlayer;
  void *CreateListener;
  void *Create3DGroup;
  SLresult (*CreateOutputMix)(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces, const SLInterfaceID *ids, const SLboolean *req);
  void *CreateMetadataExtractor;
  void *CreateExtensionObject;
  void *QueryNumSupportedInterfaces;
  void *QuerySupportedInterfaces;
  void *QueryNumSupportedExtensions;
  void *QuerySupportedExtension;
  void *IsExtensionSupported;
};

/* ---- reproduccion ---- */
typedef const struct SLPlayItf_ *const *SLPlayItf;
struct SLPlayItf_ {
  SLresult (*SetPlayState)(SLPlayItf self, SLuint32 state);
  SLresult (*GetPlayState)(SLPlayItf self, SLuint32 *pState);
  SLresult (*GetDuration)(SLPlayItf self, SLuint32 *pMsec);
  SLresult (*GetPosition)(SLPlayItf self, SLuint32 *pMsec);
  SLresult (*RegisterCallback)(SLPlayItf self, void *callback, void *pContext);
  SLresult (*SetCallbackEventsMask)(SLPlayItf self, SLuint32 eventFlags);
  SLresult (*GetCallbackEventsMask)(SLPlayItf self, SLuint32 *pEventFlags);
  SLresult (*SetMarkerPosition)(SLPlayItf self, SLuint32 mSec);
  SLresult (*ClearMarkerPosition)(SLPlayItf self);
  SLresult (*GetMarkerPosition)(SLPlayItf self, SLuint32 *pMsec);
  SLresult (*SetPositionUpdatePeriod)(SLPlayItf self, SLuint32 mSec);
  SLresult (*GetPositionUpdatePeriod)(SLPlayItf self, SLuint32 *pMsec);
};

/* ---- cola de buffers (Android) ---- */
typedef const struct SLAndroidSimpleBufferQueueItf_ *const *SLAndroidSimpleBufferQueueItf;
typedef void (*slAndroidSimpleBufferQueueCallback)(SLAndroidSimpleBufferQueueItf caller, void *pContext);
struct SLAndroidSimpleBufferQueueItf_ {
  SLresult (*Enqueue)(SLAndroidSimpleBufferQueueItf self, const void *pBuffer, SLuint32 size);
  SLresult (*Clear)(SLAndroidSimpleBufferQueueItf self);
  SLresult (*GetState)(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState *pState);
  SLresult (*RegisterCallback)(SLAndroidSimpleBufferQueueItf self, slAndroidSimpleBufferQueueCallback callback, void *pContext);
};

/* ---- configuracion Android ---- */
typedef const struct SLAndroidConfigurationItf_ *const *SLAndroidConfigurationItf;
struct SLAndroidConfigurationItf_ {
  SLresult (*SetConfiguration)(SLAndroidConfigurationItf self, const void *configKey, const void *pConfigValue, SLuint32 valueSize);
  SLresult (*GetConfiguration)(SLAndroidConfigurationItf self, const void *configKey, SLuint32 *pValueSize, void *pConfigValue);
};

#endif
