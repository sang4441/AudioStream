#include "com_example_audiostream_MainActivity.h"

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <jni.h>
#include <string.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

static const char hello[] =
#include "hello_clip.h"
;


// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;
static SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLEffectSendItf bqPlayerEffectSend;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

// aux effect on the output mix, used by the buffer queue player
static const SLEnvironmentalReverbSettings reverbSettings =
    SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

// URI player interfaces
static SLObjectItf uriPlayerObject = NULL;
static SLPlayItf uriPlayerPlay;
static SLSeekItf uriPlayerSeek;
static SLMuteSoloItf uriPlayerMuteSolo;
static SLVolumeItf uriPlayerVolume;

// file descriptor player interfaces
static SLObjectItf fdPlayerObject = NULL;
static SLPlayItf fdPlayerPlay;
static SLSeekItf fdPlayerSeek;
static SLMuteSoloItf fdPlayerMuteSolo;
static SLVolumeItf fdPlayerVolume;

// recorder interfaces
static SLObjectItf recorderObject = NULL;
static SLRecordItf recorderRecord;
static SLAndroidSimpleBufferQueueItf recorderBufferQueue;

// synthesized sawtooth clip
#define SAWTOOTH_FRAMES 8000
static short sawtoothBuffer[SAWTOOTH_FRAMES];

// 5 seconds of recorded audio at 16 kHz mono, 16-bit signed little endian
//#define RECORDER_FRAMES (16000 * 5)
#define RECORDER_FRAMES 1600//10ms
static short recorderBuffer[RECORDER_FRAMES];
static short bufferTmp[RECORDER_FRAMES];
//static short recorderBuffer[160];

static unsigned recorderSize = 0;
static SLmilliHertz recorderSR;

// pointer and size of the next player buffer to enqueue, and number of remaining buffers
static short *nextBuffer;

static unsigned nextSize;
static int nextCount;
//

typedef struct ElemTypes {
	short value[RECORDER_FRAMES * sizeof(short)];
} ElemType;

//struct ElemTypes *ElemType;

/* Circular buffer object */
typedef struct CircularBuffers {
    int         size;   /* maximum number of elements           */
    int         start;  /* index of oldest element              */
    int         end;    /* index at which to write new element  */
    ElemType      *elems;  /* vector of elements                   */
} CircularBuffer;

CircularBuffer CB;

//struct CircularBuffers *CircularBuffer;

void cbInit(CircularBuffer *cb, int size) {
    cb->size  = size + 1; /* include empty elem */
    cb->start = 0;
    cb->end   = 0;
    cb->elems = calloc(cb->size, RECORDER_FRAMES * sizeof(short));
}

void cbFree(CircularBuffer *cb) {
    free(cb->elems); /* OK if null */
}

int cbIsFull(CircularBuffer *cb) {
    return (cb->end + 1) % cb->size == cb->start;
}

int cbIsEmpty(CircularBuffer *cb) {
    return cb->end == cb->start;
}

/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
void cbWrite(CircularBuffer *cb, ElemType *elem) {
    cb->elems[cb->end] = *elem;
    cb->end = (cb->end + 1) % cb->size;
    if (cb->end == cb->start)
	cb->start = (cb->start + 1) % cb->size; /* full, overwrite */
}

/* Read oldest element. App must ensure !cbIsEmpty() first. */
short * cbRead(CircularBuffer *cb, ElemType *elem) {
    *elem = cb->elems[cb->start];
    cb->start = (cb->start + 1) % cb->size;
    return elem->value;
}

typedef struct ElemTypes2 {
	short value[RECORDER_FRAMES * sizeof(short)];
} ElemType2;

//struct ElemTypes *ElemType;

/* Circular buffer object */
typedef struct CircularBuffers2 {
    int         size;   /* maximum number of elements           */
    int         start;  /* index of oldest element              */
    int         end;    /* index at which to write new element  */
    ElemType2      *elems;  /* vector of elements                   */
} CircularBuffer2;

CircularBuffer2 CB2;
ElemType2 ET2 = {0};
//struct CircularBuffers *CircularBuffer;

void cbInit2(CircularBuffer2 *cb, int size) {
    cb->size  = size + 1; /* include empty elem */
    cb->start = 0;
    cb->end   = 0;
    cb->elems = calloc(cb->size, RECORDER_FRAMES * sizeof(short));
}

void cbFree2(CircularBuffer2 *cb) {
    free(cb->elems); /* OK if null */
}

int cbIsFull2(CircularBuffer2 *cb) {
    return (cb->end + 1) % cb->size == cb->start;
}

int cbIsEmpty2(CircularBuffer2 *cb) {
    return cb->end == cb->start;
}

/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
void cbWrite2(CircularBuffer2 *cb, ElemType2 *elem) {
    cb->elems[cb->end] = *elem;
    cb->end = (cb->end + 1) % cb->size;
    if (cb->end == cb->start)
        cb->start = (cb->start + 1) % cb->size; /* full, overwrite */
}

/* Read oldest element. App must ensure !cbIsEmpty() first. */
short * cbRead2(CircularBuffer2 *cb, ElemType2 *elem) {
    *elem = cb->elems[cb->start];
    cb->start = (cb->start + 1) % cb->size;
    return *elem->value;
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    assert(bq == bqPlayerBufferQueue);
    assert(NULL == context);

	ElemType ET;
	if (!cbIsEmpty(&CB)) {
//		nextBuffer = cbRead(&CB, &ET);
		nextBuffer = recorderBuffer;
		nextSize = RECORDER_FRAMES;

		recorderSize = RECORDER_FRAMES * sizeof(short);
		recorderSR = SL_SAMPLINGRATE_16;

		if (recorderSR == SL_SAMPLINGRATE_16) {
		   unsigned i;
		   for (i = 0; i < recorderSize; i += 2 * sizeof(short)) {
			   nextBuffer[i >> 2] = nextBuffer[i >> 1];
		   }
		   recorderSR = SL_SAMPLINGRATE_8;
		   recorderSize >>= 1;
		}
	} else {
		short emptyBuffer[RECORDER_FRAMES];
		nextBuffer = (short *) hello;
		nextSize = sizeof(hello);
	}

	SLresult result;
//		// enqueue another buffer
	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);

	// the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
	// which for this code example would indicate a programming error

	//__android_log_write(ANDROID_LOG_ERROR, "JNI", "playback after enqueue");
	assert(SL_RESULT_SUCCESS == result);
	(void)result;

}

void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    assert(bq == recorderBufferQueue);
    assert(NULL == context);

    SLresult result;
	ElemType ET;

	short buffer[RECORDER_FRAMES];
	memmove(ET.value, recorderBuffer, RECORDER_FRAMES);
	cbWrite(&CB, &ET);

    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recorderBuffer,
    	    		            RECORDER_FRAMES * sizeof(short));
}


JNIEXPORT void JNICALL Java_com_example_audiostream_MainActivity_playBack
  (JNIEnv * env, jclass cls, jshortArray lin, jint size) {

//	  if (recorderSR == SL_SAMPLINGRATE_16) {
//		   unsigned i;
//		   for (i = 0; i < recorderSize; i += 2 * sizeof(short)) {
//			   recorderBuffer[i >> 2] = recorderBuffer[i >> 1];
//		   }
//		   recorderSR = SL_SAMPLINGRATE_8;
//		   recorderSize >>= 1;
//	   }
//
//	  nextBuffer = recorderBuffer;
////	  nextSize = recorderSize;
//
//    char  buf[10];
//	sprintf(buf, "%d", CB.start);

//
//	  if (!cbIsEmpty(&CB)) {
//		  ElemType ET;
//		  nextBuffer = cbRead(&CB, &ET);
//	  }
	  short bufferTmp[RECORDER_FRAMES];
	  nextBuffer = bufferTmp;
	  nextSize = RECORDER_FRAMES;


	SLresult result;
	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);

}


JNIEXPORT jshortArray JNICALL Java_com_example_audiostream_MainActivity_startRecording
  (JNIEnv * env, jclass cls) {

	 SLresult result;
//	     in case already recording, stop recording and clear buffer queue
	    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
	    assert(SL_RESULT_SUCCESS == result);
	    (void)result;
	    result = (*recorderBufferQueue)->Clear(recorderBufferQueue);
	    assert(SL_RESULT_SUCCESS == result);
	    (void)result;

	    // the buffer is not valid for playback yet
	    recorderSize = 0;
		result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recorderBuffer,
	    		            RECORDER_FRAMES * sizeof(short));
	    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
	    // which for this code example would indicate a programming error
	    assert(SL_RESULT_SUCCESS == result);
	    (void)result;

		result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
		assert(SL_RESULT_SUCCESS == result);
		(void)result;

		jshortArray ret = (*env)->NewShortArray(env, RECORDER_FRAMES);
			(*env)->SetShortArrayRegion(env, ret, 0, RECORDER_FRAMES, recorderBuffer);
		return ret;
}

JNIEXPORT jshortArray JNICALL Java_com_example_audiostream_MainActivity_getBuffer
  (JNIEnv * env, jclass cls) {

	short emptyBuffer[RECORDER_FRAMES];
	short recordedBuffer[RECORDER_FRAMES];
	ElemType ET;

	jshortArray ret = (*env)->NewShortArray(env, RECORDER_FRAMES);

	if (!cbIsEmpty(&CB)) {
		(*env)->SetShortArrayRegion(env, ret, 0, RECORDER_FRAMES, cbRead(&CB, &ET));
	} else {
		(*env)->SetShortArrayRegion(env, ret, 0, RECORDER_FRAMES, emptyBuffer);
	}

	return ret;

}
JNIEXPORT void JNICALL Java_com_example_audiostream_MainActivity_setBuffer
  (JNIEnv * env, jclass cls, jshortArray lin, jint size) {

	ElemType2 ET2;
	memcpy(ET2.value, lin, RECORDER_FRAMES);
	cbWrite2(&CB2, &ET2);

}


JNIEXPORT void JNICALL Java_com_example_audiostream_MainActivity_createEngine
  (JNIEnv * env, jclass cls) {


	SLresult result;

		    // create engine
		    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // realize the engine
		    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // get the engine interface, which is needed in order to create other objects
		    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // create output mix, with environmental reverb specified as a non-required interface
		    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
		    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
		    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // realize the output mix
		    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // get the environmental reverb interface
		    // this could fail if the environmental reverb effect is not available,
		    // either because the feature is not present, excessive CPU load, or
		    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
		    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
		            &outputMixEnvironmentalReverb);
		    if (SL_RESULT_SUCCESS == result) {
		        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
		                outputMixEnvironmentalReverb, &reverbSettings);
		        (void)result;
		    }

		    cbInit(&CB, 64);

		    char  buf[10];
			sprintf(buf, "%d", CB.size);

		    cbInit2(&CB2, 64);
}


JNIEXPORT void JNICALL Java_com_example_audiostream_MainActivity_createBufferQueueAudioPlayer
  (JNIEnv * env, jclass cls) {

	 SLresult result;

			    // configure audio source
			    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
			    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8,
			        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
			        SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
			    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

			    // configure audio sink
			    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
			    SLDataSink audioSnk = {&loc_outmix, NULL};

			    // create audio player
			    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND,
			            /*SL_IID_MUTESOLO,*/ SL_IID_VOLUME};
			    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
			            /*SL_BOOLEAN_TRUE,*/ SL_BOOLEAN_TRUE};
			    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
			            3, ids, req);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			    // realize the player
			    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			    // get the play interface
			    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			    // get the buffer queue interface
			    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
			            &bqPlayerBufferQueue);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			    // register callback on the buffer queue
			    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			    // get the effect send interface
			    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
			            &bqPlayerEffectSend);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
			    // get the mute/solo interface
			    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;
			#endif

			    // get the volume interface
			    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

			    // set the player's state to playing
			    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
			    assert(SL_RESULT_SUCCESS == result);
			    (void)result;

}


JNIEXPORT jboolean JNICALL Java_com_example_audiostream_MainActivity_createAudioRecorder
  (JNIEnv * env, jclass cls) {

	SLresult result;

		    // configure audio source
		    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
		            SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
		    SLDataSource audioSrc = {&loc_dev, NULL};

		    // configure audio sink
		    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
		    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_16,
		        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
		        SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
		    SLDataSink audioSnk = {&loc_bq, &format_pcm};

		    // create audio recorder
		    // (requires the RECORD_AUDIO permission)
		    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
		    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
		    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
		            &audioSnk, 1, id, req);
		    if (SL_RESULT_SUCCESS != result) {
		        return JNI_FALSE;
		    }

		    // realize the audio recorder
		    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
		    if (SL_RESULT_SUCCESS != result) {
		        return JNI_FALSE;
		    }

		    // get the record interface
		    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // get the buffer queue interface
		    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
		            &recorderBufferQueue);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    // register callback on the buffer queue
		    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback,
		            NULL);
		    assert(SL_RESULT_SUCCESS == result);
		    (void)result;

		    return JNI_TRUE;
}

