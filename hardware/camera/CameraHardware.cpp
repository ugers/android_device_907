
#include "CameraDebug.h"
#if DBG_CAMERA_HARDWARE
#define LOG_NDEBUG 0
#endif
#define LOG_TAG "CameraHardware"
#include <cutils/log.h>

#include <cutils/properties.h>
#include <ui/Rect.h>

#include <drv_display_sun4i.h>
#include "CameraHardware.h"
#include "V4L2CameraDevice.h"

#define BASE_ZOOM	0

namespace android {

// defined in HALCameraFactory.cpp
extern void getCallingProcessName(char *name);

#if DEBUG_PARAM
/* Calculates and logs parameter changes.
 * Param:
 *  current - Current set of camera parameters.
 *  new_par - String representation of new parameters.
 */
static void PrintParamDiff(const CameraParameters& current, const char* new_par);
#else
#define PrintParamDiff(current, new_par)   (void(0))
#endif  /* DEBUG_PARAM */

/* A helper routine that adds a value to the camera parameter.
 * Param:
 *  param - Camera parameter to add a value to.
 *  val - Value to add.
 * Return:
 *  A new string containing parameter with the added value on success, or NULL on
 *  a failure. If non-NULL string is returned, the caller is responsible for
 *  freeing it with 'free'.
 */
static char* AddValue(const char* param, const char* val);

static int faceNotifyCb(int cmd, void * data, void * user)
{
	CameraHardware* camera_hw = (CameraHardware *)user;
	
	switch (cmd)
	{
		case FACE_NOTITY_CMD_REQUEST_FRAME:
			return camera_hw->getCurrentFaceFrame(data);
			
		case FACE_NOTITY_CMD_RESULT:
			return camera_hw->faceDetection((camera_frame_metadata_t*)data);

		case FACE_NOTITY_CMD_POSITION:
			{
				FocusArea_t * pdata = (FocusArea_t *)data;
				char face_area[128];
				sprintf(face_area, "(%d, %d, %d, %d, 1)", 
						pdata->x, pdata->y, pdata->x1, pdata->y1);
				return camera_hw->parse_focus_areas(face_area);
			}
		case FACE_NOTITY_CMD_REQUEST_ORIENTION:
			camera_hw->getCurrentOriention((int*)data);
			break;
			
		default:
			break;
	}
	
	return 0;
}

// Parse string like "640x480" or "10000,20000"
static int parse_pair(const char *str, int *first, int *second, char delim,
                      char **endptr = NULL)
{
    // Find the first integer.
    char *end;
    int w = (int)strtol(str, &end, 10);
    // If a delimeter does not immediately follow, give up.
    if (*end != delim) {
        LOGE("Cannot find delimeter (%c) in str=%s", delim, str);
        return -1;
    }

    // Find the second integer, immediately after the delimeter.
    int h = (int)strtol(end+1, &end, 10);

    *first = w;
    *second = h;

    if (endptr) {
        *endptr = end;
    }

    return 0;
}

CameraHardware::CameraHardware(struct hw_module_t* module, CCameraConfig* pCameraCfg)
        : mPreviewWindow(),
          mCallbackNotifier(),
          mCameraConfig(pCameraCfg),
          mIsCameraIdle(true),
          mFirstSetParameters(true),
          mFullSizeWidth(0),
          mFullSizeHeight(0),
          mCaptureWidth(0),
          mCaptureHeight(0),
          mUseHwEncoder(false),
          mFaceDetection(NULL),
          mFocusStatus(FOCUS_STATUS_IDLE),
          mIsSingleFocus(false),
          mOriention(0)
{
    /*
     * Initialize camera_device descriptor for this object.
     */
	F_LOG;

    /* Common header */
    common.tag = HARDWARE_DEVICE_TAG;
    common.version = 0;
    common.module = module;
    common.close = CameraHardware::close;

    /* camera_device fields. */
    ops = &mDeviceOps;
    priv = this;

	// instance V4L2CameraDevice object
	mV4L2CameraDevice = new V4L2CameraDevice(this, &mPreviewWindow, &mCallbackNotifier);
	if (mV4L2CameraDevice == NULL)
	{
		LOGE("Failed to create V4L2Camera instance");
		return ;
	}

	memset((void*)mCallingProcessName, 0, sizeof(mCallingProcessName));

	memset(&mFrameRectCrop, 0, sizeof(mFrameRectCrop));
	memset((void*)mFocusAreasStr, 0, sizeof(mFocusAreasStr));
	memset((void*)&mFocusAreas, 0, sizeof(mFocusAreas));

	// init command queue
	OSAL_QueueCreate(&mQueueCommand, CMD_QUEUE_MAX);
	memset((void*)mQueueElement, 0, sizeof(mQueueElement));

	// init command thread
	pthread_mutex_init(&mCommandMutex, NULL);
	pthread_cond_init(&mCommandCond, NULL);
	mCommandThread = new DoCommandThread(this);
	mCommandThread->startThread();
	
	// init auto focus thread
	pthread_mutex_init(&mAutoFocusMutex, NULL);
	pthread_cond_init(&mAutoFocusCond, NULL);
	mAutoFocusThread = new DoAutoFocusThread(this);
}

CameraHardware::~CameraHardware()
{
	if (mCommandThread != NULL)
	{
		mCommandThread->stopThread();
		pthread_cond_signal(&mCommandCond);
		mCommandThread.clear();
		mCommandThread = 0;
	}
		
	pthread_mutex_destroy(&mCommandMutex);
	pthread_cond_destroy(&mCommandCond);
	
	if (mAutoFocusThread != NULL)
	{
		mAutoFocusThread.clear();
		mAutoFocusThread = 0;
	}

	pthread_mutex_destroy(&mAutoFocusMutex);
	pthread_cond_destroy(&mAutoFocusCond);

	OSAL_QueueTerminate(&mQueueCommand);

	if (mFaceDetection != NULL)
	{
		DestroyFaceDetectionDev(mFaceDetection);
		mFaceDetection = NULL;
	}

	if (mV4L2CameraDevice != NULL)
	{
		delete mV4L2CameraDevice;
		mV4L2CameraDevice = NULL;
	}
}

// single focus
bool CameraHardware::autoFocusThread()
{
	pthread_mutex_lock(&mAutoFocusMutex);
	pthread_cond_wait(&mAutoFocusCond, &mAutoFocusMutex);
	if (mAutoFocusThread->getThreadStatus() == THREAD_STATE_EXIT)
	{
		LOGD("autoFocusThread exited");
		pthread_mutex_unlock(&mAutoFocusMutex);
		return false;
	}
	pthread_mutex_unlock(&mAutoFocusMutex);
	
	int ret = 0;
	const char *new_focus_mode_str = mParameters.get(CameraParameters::KEY_FOCUS_MODE);

	if (!mCameraConfig->supportFocusMode())
	{
		goto FOCUS_END;
	}
	
	if (!strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_INFINITY)
		|| !strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_FIXED))
	{
		// 
	}
	else
	{
		bool timeout = false;
		int64_t lastTimeMs = systemTime() / 1000000;
		
		setAutoFocusCtrl(V4L2_AF_TRIG_SINGLE, NULL);
		usleep(200000);

		while(1)
		{
			// if hw af ok
			ret = mV4L2CameraDevice->getAutoFocusStatus(V4L2_AF_TRIG_SINGLE);
			if ( ret == 0)
			{
				LOGV("auto focus ok, use time: %lld(ms)", systemTime() / 1000000 - lastTimeMs);
				break;
			}
			else if (ret == EBUSY)
			{
				if ((systemTime() / 1000000 - lastTimeMs) > 5000)	// 5s timeout
				{
					LOGW("auto focus time out");
					timeout = true;
					break;
				}
				//LOGV("wait auto focus ......");
				usleep(10000);
			}
			else if (ret == EFAULT)
			{
				LOGW("auto focus failed");
				break;
			}
			else if (ret < 0)
			{
				LOGE("auto focus interface error");
				break;
			}
		}

		if (ret == 0)
		{
			setAutoFocusCtrl(V4L2_AF_LOCK, NULL);
		}
	}

FOCUS_END:

	mCallbackNotifier.autoFocusMsg(ret == 0);
	
	LOGV("auto focus end");

	return true;
}

bool CameraHardware::commandThread()
{
	pthread_mutex_lock(&mCommandMutex);
	if (mCommandThread->getThreadStatus() == THREAD_STATE_EXIT)
	{
		LOGD("commandThread exited");
		pthread_mutex_unlock(&mCommandMutex);
		return false;
	}
	pthread_mutex_unlock(&mCommandMutex);
	
	Queue_Element * queue = (Queue_Element *)OSAL_Dequeue(&mQueueCommand);
	if (queue == NULL)
	{
		LOGV("wait commond queue ......");
		pthread_mutex_lock(&mCommandMutex);
		pthread_cond_wait(&mCommandCond, &mCommandMutex);
		pthread_mutex_unlock(&mCommandMutex);
		return true;
	}

	V4L2CameraDevice* pV4L2Device = mV4L2CameraDevice;
	int cmd = queue->cmd;
	switch(cmd)
	{
		case CMD_QUEUE_SET_COLOR_EFFECT:
		{
			int new_image_effect = queue->data;
			LOGV("CMD_QUEUE_SET_COLOR_EFFECT: %d", new_image_effect);
			
			if (pV4L2Device->setImageEffect(new_image_effect) < 0) 
			{
                LOGE("ERR(%s):Fail on mV4L2Camera->setImageEffect(effect(%d))", __FUNCTION__, new_image_effect);
            }
			break;
		}
		case CMD_QUEUE_SET_WHITE_BALANCE:
		{
			int new_white = queue->data;
			LOGV("CMD_QUEUE_SET_WHITE_BALANCE: %d", new_white);
			
            if (pV4L2Device->setWhiteBalance(new_white) < 0) 
			{
                LOGE("ERR(%s):Fail on mV4L2Camera->setWhiteBalance(white(%d))", __FUNCTION__, new_white);
            }
			break;
		}
		case CMD_QUEUE_SET_EXPOSURE_COMPENSATION:
		{
			int new_exposure_compensation = queue->data;
			LOGV("CMD_QUEUE_SET_EXPOSURE_COMPENSATION: %d", new_exposure_compensation);
			
			if (pV4L2Device->setExposure(new_exposure_compensation) < 0) 
			{
				LOGE("ERR(%s):Fail on mV4L2Camera->setBrightness(brightness(%d))", __FUNCTION__, new_exposure_compensation);
			}
			break;
		}
		case CMD_QUEUE_SET_FLASH_MODE:
		{
			LOGD("CMD_QUEUE_SET_FLASH_MODE to do ...");
			break;
		}
		case CMD_QUEUE_SET_FOCUS_MODE:
		{
			LOGV("CMD_QUEUE_SET_FOCUS_MODE");
			if(setAutoFocusMode() != OK)
	        {
				LOGE("unknown focus mode");
	       	}
			break;
		}
		case CMD_QUEUE_SET_FOCUS_AREA:
		{
			char * new_focus_areas_str = (char *)queue->data;
			if (new_focus_areas_str != NULL)
			{
				LOGV("CMD_QUEUE_SET_FOCUS_AREA: %s", new_focus_areas_str);
        		parse_focus_areas(new_focus_areas_str);
			}
        	break;
		}
		case CMD_QUEUE_START_FACE_DETECTE:
		{
			int width = 0, height = 0;
			LOGV("CMD_QUEUE_START_FACE_DETECTE");
			const char * preview_capture_width_str = mParameters.get(KEY_PREVIEW_CAPTURE_SIZE_WIDTH);
			const char * preview_capture_height_str = mParameters.get(KEY_PREVIEW_CAPTURE_SIZE_HEIGHT);
			if (preview_capture_width_str != NULL
				&& preview_capture_height_str != NULL)
			{
				width  = atoi(preview_capture_width_str);
				height = atoi(preview_capture_height_str);
			}
			if (mFaceDetection != 0)
			{
				usleep(500000);
				mFaceDetection->ioctrl(mFaceDetection, FACE_OPS_CMD_START, width, height);
			}
			else
			{
				LOGW("CMD_QUEUE_START_FACE_DETECTE failed, mFaceDetection not opened.");
			}
			break;
		}
		case CMD_QUEUE_STOP_FACE_DETECTE:
		{
			LOGV("CMD_QUEUE_STOP_FACE_DETECTE");
			if (mFaceDetection != 0)
			{
				mFaceDetection->ioctrl(mFaceDetection, FACE_OPS_CMD_STOP, 0, 0);
			}
			else
			{
				LOGW("CMD_QUEUE_STOP_FACE_DETECTE failed, mFaceDetection not opened.");
			}
			break;
		}
		case CMD_QUEUE_TAKE_PICTURE:
		{
			LOGV("CMD_QUEUE_TAKE_PICTURE");
			doTakePicture();
			break;
		}
		case CMD_QUEUE_PICTURE_MSG:
		{
			LOGV("CMD_QUEUE_PICTURE_MSG");
			void * frame = (void *)queue->data;
			mCallbackNotifier.notifyPictureMsg(frame);
			if (strcmp(mCallingProcessName, "com.android.cts.stub") != 0
				&& strcmp(mCallingProcessName, "com.android.cts.mediastress") != 0)
			{
				doTakePictureEnd();
			}
			break;
		}
		case CMD_QUEUE_STOP_CONTINUOUSSNAP:
		{
			cancelPicture();
			doTakePictureEnd();
			break;
		}
		case CMD_QUEUE_SET_FOCUS_STATUS:
		{
			mCallbackNotifier.autoFocusMsg(true);
			break;
		}
		default:
			LOGW("unknown queue command: %d", cmd);
			break;
	}
	
	return true;
}

/****************************************************************************
 * Public API
 ***************************************************************************/

status_t CameraHardware::Initialize()
{
	F_LOG;

	getCallingProcessName(mCallingProcessName);
	mCallbackNotifier.setCallingProcess(mCallingProcessName);

	if (mFaceDetection == NULL)
	{
		// create FaceDetection object
		CreateFaceDetectionDev(&mFaceDetection);
		if (mFaceDetection == NULL)
		{
			LOGE("create FaceDetection failed");
			return UNKNOWN_ERROR;
		}
	}

	mFaceDetection->ioctrl(mFaceDetection, FACE_OPS_CMD_REGISTE_USER, (int)this, 0);
	mFaceDetection->setCallback(mFaceDetection, faceNotifyCb);

	initDefaultParameters();

    return NO_ERROR;
}

void CameraHardware::initDefaultParameters()
{
    CameraParameters p = mParameters;
	String8 parameterString;
	char * value;

	// version
	p.set(KEY_CAMERA_HAL_VERSION, CAMERA_HAL_VERSION);

	// facing and orientation
	if (mHalCameraInfo.facing == CAMERA_FACING_BACK)
	{
	    p.set(CameraHardware::FACING_KEY, CameraHardware::FACING_BACK);
	    LOGV("%s: camera is facing %s", __FUNCTION__, CameraHardware::FACING_BACK);
	}
	else
	{
	    p.set(CameraHardware::FACING_KEY, CameraHardware::FACING_FRONT);
	    LOGV("%s: camera is facing %s", __FUNCTION__, CameraHardware::FACING_FRONT);
	}
	
    p.set(CameraHardware::ORIENTATION_KEY, mHalCameraInfo.orientation);

	// exif Make and Model
	mCallbackNotifier.setExifMake(mCameraConfig->getExifMake());
	mCallbackNotifier.setExifModel(mCameraConfig->getExifModel());

	LOGD("........................... to do initDefaultParameters");
	// for USB camera
	if (mHalCameraInfo.is_uvc)
	{
		// preview, picture, video size
		char sizeStr[256];
		mV4L2CameraDevice->enumSize(sizeStr, 256);
		LOGV("enum size: %s", sizeStr);
		if (strlen(sizeStr) > 0)
		{
			p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, sizeStr);
			p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, sizeStr);
		}
		else
		{
			p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, "640x480");
			p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "640x480");
		}
		p.set(CameraParameters::KEY_PREVIEW_SIZE, "640x480");
		p.set(CameraParameters::KEY_PICTURE_SIZE, "640x480");
		p.set(CameraParameters::KEY_VIDEO_SIZE, "640x480");

		// add for android CTS
		p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, CameraParameters::FOCUS_MODE_AUTO);
		p.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);
		p.set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");
		p.set(CameraParameters::KEY_FOCAL_LENGTH, "3.43");
		mCallbackNotifier.setFocalLenght(3.43);
		p.set(CameraParameters::KEY_FOCUS_DISTANCES, "0.10,1.20,Infinity");

		// fps
		p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "30");
		p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "5000,60000");				// add temp for CTS
		p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(5000,60000)");	// add temp for CTS
		p.set(CameraParameters::KEY_PREVIEW_FRAME_RATE, "30");
		mV4L2CameraDevice->setFrameRate(30);

		// exposure compensation
		p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "0");
		p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "0");
		p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0");
		p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");
		
		goto COMMOM_PARAMS;
	}

	// fast picture mode
	if (mHalCameraInfo.fast_picture_mode)
	{
		// capture size of picture-mode preview
		
		
		// any other differences ??
		
	}
	
	// preview size
	LOGV("init preview size");
	value = mCameraConfig->supportPreviewSizeValue();
	p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, value);
	LOGV("supportPreviewSizeValue: [%s] %s", CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, value);

#ifdef USE_NEW_MODE
	// KEY_SUPPORTED_VIDEO_SIZES and KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO should be set
	// at the same time, or both NULL. 
	// At present, we use preview and video the same size. Next version, maybe different.
	p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, value);
	p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, "1280x720");
#endif

	value = mCameraConfig->defaultPreviewSizeValue();
	p.set(CameraParameters::KEY_PREVIEW_SIZE, value);
	p.set(CameraParameters::KEY_VIDEO_SIZE, value);
	
	// picture size
	LOGV("to init picture size");
	value = mCameraConfig->supportPictureSizeValue();
	p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, value);
	LOGV("supportPreviewSizeValue: [%s] %s", CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, value);

	value = mCameraConfig->defaultPictureSizeValue();
	p.set(CameraParameters::KEY_PICTURE_SIZE, value);

	// frame rate
	LOGV("to init frame rate");
	value = mCameraConfig->supportFrameRateValue();
	p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, value);
	LOGV("supportFrameRateValue: [%s] %s", CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, value);

	p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "5000,60000");				// add temp for CTS
	p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(5000,60000)");	// add temp for CTS

	value = mCameraConfig->defaultFrameRateValue();
	p.set(CameraParameters::KEY_PREVIEW_FRAME_RATE, value);

	mV4L2CameraDevice->setFrameRate(atoi(value));

	// focus
	LOGV("to init focus");
	if (mCameraConfig->supportFocusMode())
	{
		value = mCameraConfig->supportFocusModeValue();
		p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, value);
		value = mCameraConfig->defaultFocusModeValue();
		p.set(CameraParameters::KEY_FOCUS_MODE, value);
		p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,"1");
	}
	else
	{
		// add for CTS
		p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, CameraParameters::FOCUS_MODE_FIXED);
		p.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_FIXED);
	}
	p.set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");
	p.set(CameraParameters::KEY_FOCAL_LENGTH, "3.43");
	mCallbackNotifier.setFocalLenght(3.43);
	p.set(CameraParameters::KEY_FOCUS_DISTANCES, "0.10,1.20,Infinity");

	
	// color effect 
	LOGV("to init color effect");
	if (mCameraConfig->supportColorEffect())
	{
		value = mCameraConfig->supportColorEffectValue();
		p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, value);
		value = mCameraConfig->defaultColorEffectValue();
		p.set(CameraParameters::KEY_EFFECT, value);
	}

	// flash mode
	LOGV("to init flash mode");
	if (mCameraConfig->supportFlashMode())
	{
		value = mCameraConfig->supportFlashModeValue();
		p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, value);
		value = mCameraConfig->defaultFlashModeValue();
		p.set(CameraParameters::KEY_FLASH_MODE, value);
	}

	// scene mode
	LOGV("to init scene mode");
	if (mCameraConfig->supportSceneMode())
	{
		value = mCameraConfig->supportSceneModeValue();
		p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, value);
		value = mCameraConfig->defaultSceneModeValue();
		p.set(CameraParameters::KEY_SCENE_MODE, value);
	}

	// white balance
	LOGV("to init white balance");
	if (mCameraConfig->supportWhiteBalance())
	{
		value = mCameraConfig->supportWhiteBalanceValue();
		p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, value);
		value = mCameraConfig->defaultWhiteBalanceValue();
		p.set(CameraParameters::KEY_WHITE_BALANCE, value);
	}

	// exposure compensation
	LOGV("to init exposure compensation");
	if (mCameraConfig->supportExposureCompensation())
	{
		value = mCameraConfig->minExposureCompensationValue();
		p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, value);

		value = mCameraConfig->maxExposureCompensationValue();
		p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, value);

		value = mCameraConfig->stepExposureCompensationValue();
		p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, value);

		value = mCameraConfig->defaultExposureCompensationValue();
		p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, value);
	}
	else
	{
		p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "0");
		p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "0");
		p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0");
		p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");
	}

COMMOM_PARAMS:
	// zoom
	LOGV("to init zoom");
	p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");
	p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
	p.set(CameraParameters::KEY_MAX_ZOOM, "100");

	int max_zoom = 100;
	char zoom_ratios[1024];
	memset(zoom_ratios, 0, 1024);
	for (int i = 0; i <= max_zoom; i++)
	{
		int i_ratio = 200 * i / max_zoom + 100;
		char str[8];
		sprintf(str, "%d,", i_ratio);
		strcat(zoom_ratios, str);
	}
	int len = strlen(zoom_ratios);
	zoom_ratios[len - 1] = 0;
	LOGV("zoom_ratios: %s", zoom_ratios);
	p.set(CameraParameters::KEY_ZOOM_RATIOS, zoom_ratios);
	p.set(CameraParameters::KEY_ZOOM, "0");

	mV4L2CameraDevice->setCrop(BASE_ZOOM, max_zoom);

	mZoomRatio = 100;
	
	// for some apps
	if (strcmp(mCallingProcessName, "com.android.facelock") == 0)
	{
		p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, "160x120");
		p.set(CameraParameters::KEY_PREVIEW_SIZE, "160x120");
	}

	// capture size
	const char *parm = p.get(CameraParameters::KEY_PREVIEW_SIZE);
	parse_pair(parm, &mCaptureWidth, &mCaptureHeight, 'x');
	LOGV("default preview size: %dx%d", mCaptureWidth, mCaptureHeight);

	p.set(KEY_CAPTURE_SIZE_WIDTH, mCaptureWidth);
	p.set(KEY_CAPTURE_SIZE_HEIGHT, mCaptureHeight);
	p.set(KEY_PREVIEW_CAPTURE_SIZE_WIDTH, mCaptureWidth);
	p.set(KEY_PREVIEW_CAPTURE_SIZE_HEIGHT, mCaptureHeight);

	mCallbackNotifier.setCBSize(mCaptureWidth, mCaptureHeight);

	// preview formats, CTS must support at least 2 formats
	parameterString = CameraParameters::PIXEL_FORMAT_YUV420SP;			// NV21, default
	parameterString.append(",");
	parameterString.append(CameraParameters::PIXEL_FORMAT_YUV420P);		// YV12
	p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, parameterString.string());
	
	p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);
	p.set(CameraParameters::KEY_PREVIEW_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
	
	p.set(CameraParameters::KEY_JPEG_QUALITY, "95"); // maximum quality
	p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "320x240,0x0");
	p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "320");
	p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "240");
	p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "90");
	
	p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);

	mCallbackNotifier.setJpegThumbnailSize(320, 240);

	// record hint
	p.set(CameraParameters::KEY_RECORDING_HINT, "false");

	// rotation
	p.set(CameraParameters::KEY_ROTATION, 0);
		
	// add for CTS
	p.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "54.6");
    p.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "39.4");

	p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, 10);
	p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW, 0);
	
	// take picture in video mode
	p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "true");

	mParameters = p;

	mFirstSetParameters = true;

	LOGV("CameraHardware::initDefaultParameters ok");
}

void CameraHardware::onCameraDeviceError(int err)
{
	F_LOG;
    /* Errors are reported through the callback notifier */
    mCallbackNotifier.onCameraDeviceError(err);
}

/****************************************************************************
 * Camera API implementation.
 ***************************************************************************/

status_t CameraHardware::setCameraHardwareInfo(HALCameraInfo * halInfo)
{
	// check input params
	if (halInfo == NULL)
	{
		LOGE("ERROR camera info");
		return EINVAL;
	}
	
	memcpy((void*)&mHalCameraInfo, (void*)halInfo, sizeof(HALCameraInfo));

	return NO_ERROR;
}

bool CameraHardware::isCameraIdle()
{
	Mutex::Autolock locker(&mCameraIdleLock);
	return mIsCameraIdle;
}

status_t CameraHardware::connectCamera(hw_device_t** device)
{
    F_LOG;
    status_t res = EINVAL;
	
	{
		Mutex::Autolock locker(&mCameraIdleLock);
		mIsCameraIdle = false;
	}

	if (mV4L2CameraDevice != NULL)
	{
		res = mV4L2CameraDevice->connectDevice(&mHalCameraInfo);
		if (res == NO_ERROR)
		{
			*device = &common;
			if (mCameraConfig->supportFocusMode())
			{
				// auto focus init for some module
				setAutoFocusCtrl(V4L2_AF_INIT, NULL);
			
				// start focus thread
				mAutoFocusThread->startThread();
			}
		}
		else
		{
			Mutex::Autolock locker(&mCameraIdleLock);
			mIsCameraIdle = true;
		}
	}
    return -res;
}

status_t CameraHardware::closeCamera()
{
    F_LOG;
	
    return cleanupCamera();
}

status_t CameraHardware::setPreviewWindow(struct preview_stream_ops* window)
{
	F_LOG;
    /* Callback should return a negative errno. */
	return -mPreviewWindow.setPreviewWindow(window);
}

void CameraHardware::setCallbacks(camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void* user)
{
	F_LOG;
    mCallbackNotifier.setCallbacks(notify_cb, data_cb, data_cb_timestamp,
                                    get_memory, user);
}

void CameraHardware::enableMsgType(int32_t msg_type)
{
    mCallbackNotifier.enableMessage(msg_type);
}

void CameraHardware::disableMsgType(int32_t msg_type)
{
    mCallbackNotifier.disableMessage(msg_type);
}

int CameraHardware::isMsgTypeEnabled(int32_t msg_type)
{
    return mCallbackNotifier.isMessageEnabled(msg_type);
}

status_t CameraHardware::startPreview()
{
	F_LOG;
    Mutex::Autolock locker(&mObjectLock);
    /* Callback should return a negative errno. */
    return -doStartPreview();
}

void CameraHardware::stopPreview()
{
	F_LOG;
	
	mQueueElement[CMD_QUEUE_STOP_FACE_DETECTE].cmd = CMD_QUEUE_STOP_FACE_DETECTE;
	OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_STOP_FACE_DETECTE]);
	pthread_cond_signal(&mCommandCond);
	
    Mutex::Autolock locker(&mObjectLock);
    doStopPreview();
}

int CameraHardware::isPreviewEnabled()
{
	F_LOG;
    return mPreviewWindow.isPreviewEnabled();
}

status_t CameraHardware::storeMetaDataInBuffers(int enable)
{
	F_LOG;
    /* Callback should return a negative errno. */
    return -mCallbackNotifier.storeMetaDataInBuffers(enable);
}

status_t CameraHardware::startRecording()
{
	F_LOG;
	
	// video hint
    const char * valstr = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
    if (valstr) 
	{
		LOGV("KEY_RECORDING_HINT: %s -> true", valstr);
    }

	mParameters.set(KEY_SNAP_PATH, "");
	mCallbackNotifier.setSnapPath("");

	mParameters.set(CameraParameters::KEY_RECORDING_HINT, CameraParameters::TRUE);

	if (mVideoCaptureWidth != mCaptureWidth
		|| mVideoCaptureHeight != mCaptureHeight)
	{
		doStopPreview();
		doStartPreview();
	}
	
    /* Callback should return a negative errno. */
    return -mCallbackNotifier.enableVideoRecording();
}

void CameraHardware::stopRecording()
{
	F_LOG;
    mCallbackNotifier.disableVideoRecording();
	mV4L2CameraDevice->setHwEncoder(false);
}

int CameraHardware::isRecordingEnabled()
{
	F_LOG;
    return mCallbackNotifier.isVideoRecordingEnabled();
}

void CameraHardware::releaseRecordingFrame(const void* opaque)
{
	if (mUseHwEncoder)
	{
    	mV4L2CameraDevice->releasePreviewFrame(*(int*)opaque);
	}
}

/****************************************************************************
 * Focus 
 ***************************************************************************/

status_t CameraHardware::setAutoFocus()
{
	if (!mCameraConfig->supportFocusMode())
	{
		mQueueElement[CMD_QUEUE_SET_FOCUS_STATUS].cmd = CMD_QUEUE_SET_FOCUS_STATUS;
		OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_SET_FOCUS_STATUS]);
		pthread_cond_signal(&mCommandCond);
		
		return OK;
	}
	
	pthread_cond_signal(&mAutoFocusCond);

	return OK;
}

status_t CameraHardware::cancelAutoFocus()
{
	F_LOG;

	/* TODO: Future enhancements. */
	return NO_ERROR;
}

int CameraHardware::parse_focus_areas(const char *str)
{
	int ret = -1;
	char *ptr,*tmp;
	char p1[6] = {0}, p2[6] = {0};
	char p3[6] = {0}, p4[6] = {0}, p5[6] = {0};
	int  l,t,r,b;
	int  w,h;

	if (str == NULL
		|| !mCameraConfig->supportFocusMode())
	{
		return 0;
	}

	// LOGV("parse_focus_areas : %s", str);

	tmp = strchr(str,'(');
	tmp++;
	ptr = strchr(tmp,',');
	memcpy(p1,tmp,ptr-tmp);
	
	tmp = ptr+1;
	ptr = strchr(tmp,',');
	memcpy(p2,tmp,ptr-tmp);

	tmp = ptr+1;
	ptr = strchr(tmp,',');
	memcpy(p3,tmp,ptr-tmp);

	tmp = ptr+1;
	ptr = strchr(tmp,',');
	memcpy(p4,tmp,ptr-tmp);

	tmp = ptr+1;
	ptr = strchr(tmp,')');
	memcpy(p5,tmp,ptr-tmp);

	l = atoi(p1);
	t = atoi(p2);
	r = atoi(p3);
	b = atoi(p4);
	
	w = l + (r-l)/2;
	h = t + (b-t)/2;

	mFocusAreas.width = mFrameRectCrop.right - (1000 - w) * (mFrameRectCrop.right - mFrameRectCrop.left) / 2000;
	mFocusAreas.height= mFrameRectCrop.bottom - (1000 - h) * (mFrameRectCrop.bottom - mFrameRectCrop.top) / 2000;

	// LOGV("V4L2_AF_WIN_XY: %d,%d", mFocusAreas.width, mFocusAreas.height);
	setAutoFocusCtrl(V4L2_AF_WIN_XY, (void*)&mFocusAreas);

	return 0;
}

int CameraHardware::setAutoFocusMode()
{
	F_LOG;
	v4l2_autofocus_mode af_mode = V4L2_AF_FIXED;
    if (mCameraConfig->supportFocusMode())
	{
	    // focus
		const char *new_focus_mode_str = mParameters.get(CameraParameters::KEY_FOCUS_MODE);
		if (!strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_AUTO))
		{
			af_mode = V4L2_AF_AUTO;
		}
		else if (!strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_INFINITY))
		{
			af_mode = V4L2_AF_INFINITY;
		}
		else if (!strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_MACRO))
		{
			af_mode = V4L2_AF_MACRO;
		}
		else if (!strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_FIXED))
		{
			af_mode = V4L2_AF_FIXED;
		}
		else if (!strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)
					|| !strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO))
		{
			LOGD("%s : continuous focus to do ...", __FUNCTION__);
			af_mode = V4L2_AF_AUTO;
		}
		else
		{
			return -EINVAL;
		}
	}
	else
	{
		af_mode = V4L2_AF_FIXED;
	}
	
	mV4L2CameraDevice->setAutoFocusMode(af_mode);
	
	return OK;
}

int CameraHardware::setAutoFocusCtrl(int ctrl, void *data)
{
	if (mCameraConfig->supportFocusMode())
	{
		mV4L2CameraDevice->setAutoFocusCtrl(ctrl, data);
	}
	return OK;
}

bool CameraHardware::checkFocusMode(const char * mode)
{
	const char * str = mParameters.get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES);
	if (!strstr(str, mode))
	{
		return false;
	}
	return true;
}

bool CameraHardware::checkFocusArea(const char * area)
{
	if (area == NULL)
	{
		return false;
	}

	int i = 0;
	int arr[5];		// [l, t, r, b, w]
	char temp[128];
	strcpy(temp, area);
	char *pval = temp;
	char *seps = "(,)";
	int offset = 0;
	pval = strtok(pval, seps);
	while (pval != NULL)
	{
		if (i >= 5)
		{
			return false;
		}
		arr[i++] = atoi(pval);
		pval = strtok(NULL, seps);
	}

	LOGV("%s : (%d, %d, %d, %d, %d)", __FUNCTION__, arr[0], arr[1],arr[2],arr[3],arr[4]);
	
	if ((arr[0] == 0)
		&& (arr[1] == 0)
		&& (arr[2] == 0)
		&& (arr[3] == 0)
		&& (arr[4] == 0))
	{
		return true;
	}
	
	if (arr[4] < 1)
	{
		return false;
	}
	
	for(i = 0; i < 4; i++)
	{
		if ((arr[i] < -1000) || (arr[i] > 1000))
		{
			return false;
		}
	}

	if ((arr[0] >= arr[2])
		|| (arr[1] >= arr[3]))
	{
		return false;
	}
	
	return true;
}

/****************************************************************************
 * take picture management
 ***************************************************************************/

status_t CameraHardware::doTakePicture()
{
	F_LOG;
    Mutex::Autolock locker(&mObjectLock);

	/* Get JPEG quality. */
    int jpeg_quality = mParameters.getInt(CameraParameters::KEY_JPEG_QUALITY);
    if (jpeg_quality <= 0) {
        jpeg_quality = 90;
    }

	/* Get JPEG rotate. */
    int jpeg_rotate = mParameters.getInt(CameraParameters::KEY_ROTATION);
    if (jpeg_rotate <= 0) {
        jpeg_rotate = 0;  /* Fall back to default. */
    }

	/* Get JPEG size. */
	int pic_width, pic_height;
    mParameters.getPictureSize(&pic_width, &pic_height);

	mCallbackNotifier.setJpegQuality(jpeg_quality);
	mCallbackNotifier.setJpegRotate(jpeg_rotate);
    mCallbackNotifier.setPictureSize(pic_width, pic_height);

	// if in recording mode
	const char * valstr = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
	bool video_hint = (strcmp(valstr, CameraParameters::TRUE) == 0);
	if (video_hint)
	{
		//mCallbackNotifier.setPictureSize(mCaptureWidth*4, 4*mCaptureHeight);
		mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_RECORD);
		return OK;
	}

	// picture mode
	const char * cur_picture_mode = mParameters.get(KEY_PICTURE_MODE);
	if (cur_picture_mode != NULL)
	{
		// continuous picture
		if (!strcmp(cur_picture_mode, PICTURE_MODE_CONTINUOUS)
			|| !strcmp(cur_picture_mode, PICTURE_MODE_CONTINUOUS_FAST))
		{
			// test continuous picture
			int number = 0;
			if (!strcmp(cur_picture_mode, PICTURE_MODE_CONTINUOUS))
			{
				number = 100;
				mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_CONTINUOUS);
			}
			else if (!strcmp(cur_picture_mode, PICTURE_MODE_CONTINUOUS_FAST))
			{
				number = 10;
				mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_CONTINUOUS_FAST);
			}
			mCallbackNotifier.setPictureSize(pic_width, pic_height);
			mCallbackNotifier.setContinuousPictureCnt(number);
			mCallbackNotifier.startContinuousPicture();
			mV4L2CameraDevice->setContinuousPictureCnt(number);
			mV4L2CameraDevice->startContinuousPicture();

			return OK;
		}
	}

	// normal picture mode

	// full-size capture
	bool fast_picture_mode = mHalCameraInfo.fast_picture_mode;
	if (fast_picture_mode)
	{
		mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_FAST);
		return OK;
	}

	// normal taking picture mode
	int frame_width = pic_width;
	int frame_height = pic_height;
	mV4L2CameraDevice->tryFmtSize(&frame_width, &frame_height);

	if (mCaptureWidth == frame_width
		&& mCaptureHeight == frame_height)
	{
		LOGV("current capture size[%dx%d] is the same as picture size", frame_width, frame_height);
		mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_FAST);
		return OK;
	}
	
	// preview format and video format are the same
	const char* pix_fmt = mParameters.getPictureFormat();
	uint32_t org_fmt = V4L2_PIX_FMT_NV12;

	/*
     * Make sure preview is not running, and device is stopped before taking
     * picture.
     */
    const bool preview_on = mPreviewWindow.isPreviewEnabled();
    if (preview_on) {
        doStopPreview();
    }

	/* Camera device should have been stopped when the shutter message has been
	 * enabled. */
	if (mV4L2CameraDevice->isStarted()) 
	{
		LOGW("%s: Camera device is started", __FUNCTION__);
		mV4L2CameraDevice->stopDeliveringFrames();
		mV4L2CameraDevice->stopDevice();
	}

	/*
	 * Take the picture now.
	 */
	
	mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_NORMAL);

	mCaptureWidth = frame_width;
	mCaptureHeight = frame_height;

	LOGD("Starting camera: %dx%d -> %.4s(%s)",
		 mCaptureWidth, mCaptureHeight, reinterpret_cast<const char*>(&org_fmt), pix_fmt);
	status_t res = mV4L2CameraDevice->startDevice(mCaptureWidth, mCaptureHeight, org_fmt, video_hint);
	if (res != NO_ERROR) 
	{
		if (preview_on) {
            doStartPreview();
        }
		return res;
	}
	
	res = mV4L2CameraDevice->startDeliveringFrames();
	if (res != NO_ERROR) 
	{
		mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_NULL);
        if (preview_on) {
            doStartPreview();
        }
	}

	return OK;
}

status_t CameraHardware::doTakePictureEnd()
{
	F_LOG;
	
    Mutex::Autolock locker(&mObjectLock);

	if (mV4L2CameraDevice->isConnected() 			// camera is connected or started
		&& !mPreviewWindow.isPreviewEnabled()		// preview is not enable
		&& !mHalCameraInfo.fast_picture_mode)
	{
		// 
		LOGV("doTakePictureEnd to doStartPreview");
		doStartPreview();
	}

	return OK;
}

status_t CameraHardware::takePicture()
{
	F_LOG;
	mQueueElement[CMD_QUEUE_TAKE_PICTURE].cmd = CMD_QUEUE_TAKE_PICTURE;
	OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_TAKE_PICTURE]);
	pthread_cond_signal(&mCommandCond);
	
    return OK;
}

status_t CameraHardware::cancelPicture()
{
    F_LOG;
	mV4L2CameraDevice->setTakePictureState(TAKE_PICTURE_NULL);

    return NO_ERROR;
}

// 
void CameraHardware::notifyPictureMsg(const void* frame)
{
	mQueueElement[CMD_QUEUE_PICTURE_MSG].cmd = CMD_QUEUE_PICTURE_MSG;
	mQueueElement[CMD_QUEUE_PICTURE_MSG].data = (int)frame;
	OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_PICTURE_MSG]);
	pthread_cond_signal(&mCommandCond);
}

/****************************************************************************
 * set and get parameters
 ***************************************************************************/

void CameraHardware::setVideoCaptureSize(int video_w, int video_h)
{
	// video size is video_w x video_h, capture size may be different
	// now the same
	mVideoCaptureWidth = video_w;
	mVideoCaptureHeight= video_h;

	LOGD("setVideoCaptureSize next version to do ......");

	int ret = mV4L2CameraDevice->tryFmtSize(&mVideoCaptureWidth, &mVideoCaptureHeight);
	if(ret < 0)
	{
		LOGE("setVideoCaptureSize tryFmtSize failed");
		return;
	}
	LOGV("setVideoCaptureSize video source: %dx%d", mVideoCaptureWidth, mVideoCaptureHeight);
	
	mParameters.set(KEY_CAPTURE_SIZE_WIDTH, mVideoCaptureWidth);
	mParameters.set(KEY_CAPTURE_SIZE_HEIGHT, mVideoCaptureHeight);
}

void CameraHardware::getCurrentOriention(int * oriention)
{
	*oriention = mOriention;
	
	if(mHalCameraInfo.facing == CAMERA_FACING_FRONT)   //for direction of front camera facedetection by fuqiang
	{
		if(*oriention == 90)
		{
			*oriention = 270;
		}
		else if(*oriention == 270)
		{
			*oriention = 90;
		}
	}
}

status_t CameraHardware::setParameters(const char* p)
{
	F_LOG;
	int ret = UNKNOWN_ERROR;
	
    PrintParamDiff(mParameters, p);

    CameraParameters params;
	String8 str8_param(p);
    params.unflatten(str8_param);

	V4L2CameraDevice* pV4L2Device = mV4L2CameraDevice;
	if (pV4L2Device == NULL)
	{
		LOGE("%s : getCameraDevice failed", __FUNCTION__);
		return UNKNOWN_ERROR;
	}

	// stop continuous picture
	const char * cur_picture_mode = mParameters.get(KEY_PICTURE_MODE);
	const char * stop_continuous_picture = params.get(KEY_CANCEL_CONTINUOUS_PICTURE);
	LOGV("%s : stop_continuous_picture : %s", __FUNCTION__, stop_continuous_picture);
	if (cur_picture_mode != NULL
		&& stop_continuous_picture != NULL
		&& !strcmp(cur_picture_mode, PICTURE_MODE_CONTINUOUS)
		&& !strcmp(stop_continuous_picture, "true")) 
	{
		mQueueElement[CMD_QUEUE_STOP_CONTINUOUSSNAP].cmd = CMD_QUEUE_STOP_CONTINUOUSSNAP;
		OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_STOP_CONTINUOUSSNAP]);
	}

	// picture mode
	const char * new_picture_mode = params.get(KEY_PICTURE_MODE);
	LOGV("%s : new_picture_mode : %s", __FUNCTION__, new_picture_mode);
    if (new_picture_mode != NULL) 
	{
		if (!strcmp(new_picture_mode, PICTURE_MODE_NORMAL)
			|| !strcmp(new_picture_mode, PICTURE_MODE_CONTINUOUS)
			|| !strcmp(new_picture_mode, PICTURE_MODE_CONTINUOUS_FAST))
		{
        	mParameters.set(KEY_PICTURE_MODE, new_picture_mode);
		}
		else
		{
        	LOGW("%s : unknown picture mode: %s", __FUNCTION__, new_picture_mode);
		}
	
		// continuous picture path
		if (!strcmp(new_picture_mode, PICTURE_MODE_CONTINUOUS)
			|| !strcmp(new_picture_mode, PICTURE_MODE_CONTINUOUS_FAST))
		{
			const char * new_path = params.get(KEY_CONTINUOUS_PICTURE_PATH);
			LOGV("%s : new_path : %s", __FUNCTION__, new_path);
			if (new_path != NULL)
			{
				mParameters.set(KEY_CONTINUOUS_PICTURE_PATH, new_path);
				mCallbackNotifier.setSaveFolderPath(new_path);
			}
			else
			{
				LOGW("%s : invalid path: %s in %s picture mode", __FUNCTION__, new_path, new_picture_mode);
				mParameters.set(KEY_PICTURE_MODE, PICTURE_MODE_NORMAL);
			}
		}
		else if(!strcmp(new_picture_mode, PICTURE_MODE_NORMAL))
		{
			const char * new_path = params.get(KEY_SNAP_PATH);
			LOGV("%s : snap new_path : %s", __FUNCTION__, new_path);
			if (new_path != NULL)
			{
				mParameters.set(KEY_SNAP_PATH, new_path);
				mCallbackNotifier.setSnapPath(new_path);
			}
		}
    }

	// preview format
	const char * new_preview_format = params.getPreviewFormat();
	LOGV("%s : new_preview_format : %s", __FUNCTION__, new_preview_format);
	if (new_preview_format != NULL
		&& (strcmp(new_preview_format, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0
		|| strcmp(new_preview_format, CameraParameters::PIXEL_FORMAT_YUV420P) == 0)) 
	{
		mParameters.setPreviewFormat(new_preview_format);
	}
	else
    {
        LOGE("%s : Only yuv420sp or yuv420p preview is supported", __FUNCTION__);
        return -EINVAL;
    }

	// picture format
	const char * new_picture_format = params.getPictureFormat();
	LOGV("%s : new_picture_format : %s", __FUNCTION__, new_picture_format);
	if (new_picture_format == NULL
		|| (strcmp(new_picture_format, CameraParameters::PIXEL_FORMAT_JPEG) != 0)) 
    {
        LOGE("%s : Only jpeg still pictures are supported", __FUNCTION__);
        return -EINVAL;
    }

	// picture size
	int new_picture_width  = 0;
    int new_picture_height = 0;
    params.getPictureSize(&new_picture_width, &new_picture_height);
    LOGV("%s : new_picture_width x new_picture_height = %dx%d", __FUNCTION__, new_picture_width, new_picture_height);
    if (0 < new_picture_width && 0 < new_picture_height) 
	{
		mParameters.setPictureSize(new_picture_width, new_picture_height);
    }
	else
	{
		LOGE("%s : error picture size", __FUNCTION__);
		return -EINVAL;
	}

	// preview size
    int new_preview_width  = 0;
    int new_preview_height = 0;
    params.getPreviewSize(&new_preview_width, &new_preview_height);
    LOGV("%s : new_preview_width x new_preview_height = %dx%d",
         __FUNCTION__, new_preview_width, new_preview_height);
	if (0 < new_preview_width && 0 < new_preview_height)
	{
		mParameters.setPreviewSize(new_preview_width, new_preview_height);
	
		mCallbackNotifier.setCBSize(new_preview_width, new_preview_height);
		
		// try size
		ret = pV4L2Device->tryFmtSize(&new_preview_width, &new_preview_height);
		if(ret < 0)
		{
			return ret;
		}
		
		mParameters.set(KEY_PREVIEW_CAPTURE_SIZE_WIDTH, new_preview_width);
		mParameters.set(KEY_PREVIEW_CAPTURE_SIZE_HEIGHT, new_preview_height);
	}
	else
	{
		LOGE("%s : error preview size", __FUNCTION__);
		return -EINVAL;
	}

	// video size
	int new_video_width		= 0;
	int new_video_height	= 0;
	params.getVideoSize(&new_video_width, &new_video_height);
    LOGV("%s : new_video_width x new_video_height = %dx%d",
         __FUNCTION__, new_video_width, new_video_height);
	if (0 < new_video_width && 0 < new_video_height)
	{
		int video_width		= 0;
		int video_height	= 0;
		mParameters.getVideoSize(&video_width, &video_height);
		if (mFirstSetParameters
			|| video_width != new_video_width
			|| video_height != new_video_height)
		{
			mParameters.setVideoSize(new_video_width, new_video_height);
			if (new_video_width != mVideoCaptureWidth
				|| new_video_height != mVideoCaptureHeight)
			{
				setVideoCaptureSize(new_video_width, new_video_height);
			}
		}
	}
	else
	{
		LOGE("%s : error video size", __FUNCTION__);
		return -EINVAL;
	}

	// video hint
    const char * valstr = params.get(CameraParameters::KEY_RECORDING_HINT);
    if (valstr) 
	{
		LOGV("%s : KEY_RECORDING_HINT: %s", __FUNCTION__, valstr);
        mParameters.set(CameraParameters::KEY_RECORDING_HINT, valstr);
    }

	// frame rate
	int new_min_frame_rate, new_max_frame_rate;
	params.getPreviewFpsRange(&new_min_frame_rate, &new_max_frame_rate);
	int new_preview_frame_rate = params.getPreviewFrameRate();
	if (0 < new_preview_frame_rate && 0 < new_min_frame_rate 
		&& new_min_frame_rate <= new_max_frame_rate)
	{
		int preview_frame_rate = mParameters.getPreviewFrameRate();
		if (mFirstSetParameters
			|| preview_frame_rate != new_preview_frame_rate)
		{
			mParameters.setPreviewFrameRate(new_preview_frame_rate);
			pV4L2Device->setFrameRate(new_preview_frame_rate);
		}
	}
	else
	{
		if (pV4L2Device->getCaptureFormat() == V4L2_PIX_FMT_YUYV)
		{
			LOGV("may be usb camera, don't care frame rate");
		}
		else
		{
			LOGE("%s : error preview frame rate", __FUNCTION__);
			return -EINVAL;
		}
	}

	// JPEG image quality
    int new_jpeg_quality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    LOGV("%s : new_jpeg_quality %d", __FUNCTION__, new_jpeg_quality);
    if (new_jpeg_quality >=1 && new_jpeg_quality <= 100) 
	{
		mParameters.set(CameraParameters::KEY_JPEG_QUALITY, new_jpeg_quality);
    }
	else
	{
		if (pV4L2Device->getCaptureFormat() == V4L2_PIX_FMT_YUYV)
		{
			LOGV("may be usb camera, don't care picture quality");
			mParameters.set(CameraParameters::KEY_JPEG_QUALITY, 90);
		}
		else
		{
			LOGE("%s : error picture quality", __FUNCTION__);
			return -EINVAL;
		}
	}

	// rotation	
	int new_rotation = params.getInt(CameraParameters::KEY_ROTATION);
    LOGV("%s : new_rotation %d", __FUNCTION__, new_rotation);
    if (0 <= new_rotation) 
	{
		mOriention = new_rotation;
        mParameters.set(CameraParameters::KEY_ROTATION, new_rotation);
    }
	else
	{
		LOGE("%s : error rotate", __FUNCTION__);
		return -EINVAL;
	}

	// image effect
	if (mCameraConfig->supportColorEffect())
	{
		const char *now_image_effect_str = mParameters.get(CameraParameters::KEY_EFFECT);
		const char *new_image_effect_str = params.get(CameraParameters::KEY_EFFECT);
		LOGV("%s : new_image_effect_str %s", __FUNCTION__, new_image_effect_str);
	    if ((new_image_effect_str != NULL)
			&& (mFirstSetParameters || strcmp(now_image_effect_str, new_image_effect_str)))
		{
	        int  new_image_effect = -1;

	        if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_NONE))
	            new_image_effect = V4L2_COLORFX_NONE;
	        else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_MONO))
	            new_image_effect = V4L2_COLORFX_BW;
	        else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_SEPIA))
	            new_image_effect = V4L2_COLORFX_SEPIA;
	        else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_AQUA))
	            new_image_effect = V4L2_COLORFX_GRASS_GREEN;
	        else if (!strcmp(new_image_effect_str, CameraParameters::EFFECT_NEGATIVE))
	            new_image_effect = V4L2_COLORFX_NEGATIVE;
	        else {
	            //posterize, whiteboard, blackboard, solarize
	            LOGE("ERR(%s):Invalid effect(%s)", __FUNCTION__, new_image_effect_str);
	            ret = UNKNOWN_ERROR;
	        }

	        if (new_image_effect >= 0) {
	            mParameters.set(CameraParameters::KEY_EFFECT, new_image_effect_str);
				mQueueElement[CMD_QUEUE_SET_COLOR_EFFECT].cmd = CMD_QUEUE_SET_COLOR_EFFECT;
				mQueueElement[CMD_QUEUE_SET_COLOR_EFFECT].data = new_image_effect;
				OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_SET_COLOR_EFFECT]);
	        }
	    }
	}

	// white balance
	if (mCameraConfig->supportWhiteBalance())
	{
		const char *now_white_str = mParameters.get(CameraParameters::KEY_WHITE_BALANCE);
		const char *new_white_str = params.get(CameraParameters::KEY_WHITE_BALANCE);
	    LOGV("%s : new_white_str %s", __FUNCTION__, new_white_str);
	    if ((new_white_str != NULL)
			&& (mFirstSetParameters || strcmp(now_white_str, new_white_str)))
		{
	        int new_white = -1;
	        int no_auto_balance = 1;

	        if (!strcmp(new_white_str, CameraParameters::WHITE_BALANCE_AUTO))
	        {
	            new_white = V4L2_WB_AUTO;
	            no_auto_balance = 0;
	        }
	        else if (!strcmp(new_white_str,
	                         CameraParameters::WHITE_BALANCE_DAYLIGHT))
	            new_white = V4L2_WB_DAYLIGHT;
	        else if (!strcmp(new_white_str,
	                         CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT))
	            new_white = V4L2_WB_CLOUD;
	        else if (!strcmp(new_white_str,
	                         CameraParameters::WHITE_BALANCE_FLUORESCENT))
	            new_white = V4L2_WB_FLUORESCENT;
	        else if (!strcmp(new_white_str,
	                         CameraParameters::WHITE_BALANCE_INCANDESCENT))
	            new_white = V4L2_WB_INCANDESCENCE;
	        else if (!strcmp(new_white_str,
	                         CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT))
	            new_white = V4L2_WB_TUNGSTEN;
	        else{
	            LOGE("ERR(%s):Invalid white balance(%s)", __FUNCTION__, new_white_str); //twilight, shade
	            ret = UNKNOWN_ERROR;
	        }

	        mCallbackNotifier.setWhiteBalance(no_auto_balance);

	        if (0 <= new_white)
			{
				mParameters.set(CameraParameters::KEY_WHITE_BALANCE, new_white_str);
				mQueueElement[CMD_QUEUE_SET_WHITE_BALANCE].cmd = CMD_QUEUE_SET_WHITE_BALANCE;
				mQueueElement[CMD_QUEUE_SET_WHITE_BALANCE].data = new_white;
				OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_SET_WHITE_BALANCE]);
	        }
	    }
	}
	
	// exposure compensation
	if (mCameraConfig->supportExposureCompensation())
	{
		int now_exposure_compensation = mParameters.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
		int new_exposure_compensation = params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
		int max_exposure_compensation = params.getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
		int min_exposure_compensation = params.getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
		LOGV("%s : new_exposure_compensation %d", __FUNCTION__, new_exposure_compensation);
		if ((min_exposure_compensation <= new_exposure_compensation)
			&& (max_exposure_compensation >= new_exposure_compensation))
		{
			if (mFirstSetParameters || (now_exposure_compensation != new_exposure_compensation))
			{
				mParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, new_exposure_compensation);
				mQueueElement[CMD_QUEUE_SET_EXPOSURE_COMPENSATION].cmd = CMD_QUEUE_SET_EXPOSURE_COMPENSATION;
				mQueueElement[CMD_QUEUE_SET_EXPOSURE_COMPENSATION].data = new_exposure_compensation;
				OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_SET_EXPOSURE_COMPENSATION]);
			}
		}
		else
		{
			LOGE("ERR(%s):invalid exposure compensation: %d", __FUNCTION__, new_exposure_compensation);
			return -EINVAL;
		}
	}
	
	// flash mode	
	if (mCameraConfig->supportFlashMode())
	{
		const char *new_flash_mode_str = params.get(CameraParameters::KEY_FLASH_MODE);
		mParameters.set(CameraParameters::KEY_FLASH_MODE, new_flash_mode_str);
	}

	// zoom
	int max_zoom = mParameters.getInt(CameraParameters::KEY_MAX_ZOOM);
	int new_zoom = params.getInt(CameraParameters::KEY_ZOOM);
	LOGV("%s : new_zoom: %d", __FUNCTION__, new_zoom);
	if (0 <= new_zoom && new_zoom <= max_zoom)
	{
		mParameters.set(CameraParameters::KEY_ZOOM, new_zoom);
		pV4L2Device->setCrop(new_zoom + BASE_ZOOM, max_zoom);
		mZoomRatio = (new_zoom + BASE_ZOOM) * 2 * 100 / max_zoom + 100;
	}
	else
	{
		LOGE("ERR(%s): invalid zoom value: %d", __FUNCTION__, new_zoom);
		return -EINVAL;
	}

	// focus
	if (mCameraConfig->supportFocusMode())
	{
		const char *now_focus_mode_str = mParameters.get(CameraParameters::KEY_FOCUS_MODE);
		const char *now_focus_areas_str = mParameters.get(CameraParameters::KEY_FOCUS_AREAS);
		const char *new_focus_mode_str = params.get(CameraParameters::KEY_FOCUS_MODE);
        const char *new_focus_areas_str = params.get(CameraParameters::KEY_FOCUS_AREAS);

		if (!checkFocusArea(new_focus_areas_str))
		{
			LOGE("ERR(%s): invalid focus areas", __FUNCTION__);
			return -EINVAL;
		}
		
		if (!checkFocusMode(new_focus_mode_str))
		{
			LOGE("ERR(%s): invalid focus mode", __FUNCTION__);
			return -EINVAL;
		}
		
		if (mFirstSetParameters || strcmp(now_focus_mode_str, new_focus_mode_str))
		{
			mParameters.set(CameraParameters::KEY_FOCUS_MODE, new_focus_mode_str);
			mQueueElement[CMD_QUEUE_SET_FOCUS_MODE].cmd = CMD_QUEUE_SET_FOCUS_MODE;
			OSAL_QueueSetElem(&mQueueCommand, &mQueueElement[CMD_QUEUE_SET_FOCUS_MODE]);
		}

		// to do, check running??
		if (/*pV4L2Device->getThreadRunning()
			&&*/ strcmp(now_focus_areas_str, new_focus_areas_str))
		{
			mParameters.set(CameraParameters::KEY_FOCUS_AREAS, new_focus_areas_str);

#if 0
			strcpy(mFocusAreasStr, new_focus_areas_str);
			mQueueElement[CMD_QUEUE_SET_FOCUS_AREA].cmd = CMD_QUEUE_SET_FOCUS_AREA;
			mQueueElement[CMD_QUEUE_SET_FOCUS_AREA].data = (int)&mFocusAreasStr;
			OSAL_QueueSetElem(&mQueueCommand, &mQueueElement[CMD_QUEUE_SET_FOCUS_AREA]);
#else
			parse_focus_areas(new_focus_areas_str);
#endif
		}
	}
	else
	{
		const char *new_focus_mode_str = params.get(CameraParameters::KEY_FOCUS_MODE);
		if (strcmp(new_focus_mode_str, CameraParameters::FOCUS_MODE_FIXED))
		{
			LOGE("ERR(%s): invalid focus mode: %s", __FUNCTION__, new_focus_mode_str);
			return -EINVAL;
		}
		mParameters.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_FIXED);
	}

	// gps latitude
    const char *new_gps_latitude_str = params.get(CameraParameters::KEY_GPS_LATITUDE);
	if (new_gps_latitude_str) {
		mCallbackNotifier.setGPSLatitude(atof(new_gps_latitude_str));
        mParameters.set(CameraParameters::KEY_GPS_LATITUDE, new_gps_latitude_str);
    } else {
    	mCallbackNotifier.setGPSLatitude(0.0);
        mParameters.remove(CameraParameters::KEY_GPS_LATITUDE);
    }

    // gps longitude
    const char *new_gps_longitude_str = params.get(CameraParameters::KEY_GPS_LONGITUDE);
    if (new_gps_longitude_str) {
		mCallbackNotifier.setGPSLongitude(atof(new_gps_longitude_str));
        mParameters.set(CameraParameters::KEY_GPS_LONGITUDE, new_gps_longitude_str);
    } else {
    	mCallbackNotifier.setGPSLongitude(0.0);
        mParameters.remove(CameraParameters::KEY_GPS_LONGITUDE);
    }
  
    // gps altitude
    const char *new_gps_altitude_str = params.get(CameraParameters::KEY_GPS_ALTITUDE);
	if (new_gps_altitude_str) {
		mCallbackNotifier.setGPSAltitude(atol(new_gps_altitude_str));
        mParameters.set(CameraParameters::KEY_GPS_ALTITUDE, new_gps_altitude_str);
    } else {
		mCallbackNotifier.setGPSAltitude(0);
        mParameters.remove(CameraParameters::KEY_GPS_ALTITUDE);
    }

    // gps timestamp
    const char *new_gps_timestamp_str = params.get(CameraParameters::KEY_GPS_TIMESTAMP);
	if (new_gps_timestamp_str) {
		mCallbackNotifier.setGPSTimestamp(atol(new_gps_timestamp_str));
        mParameters.set(CameraParameters::KEY_GPS_TIMESTAMP, new_gps_timestamp_str);
    } else {
		mCallbackNotifier.setGPSTimestamp(0);
        mParameters.remove(CameraParameters::KEY_GPS_TIMESTAMP);
    }

    // gps processing method
    const char *new_gps_processing_method_str = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
	if (new_gps_processing_method_str) {
		mCallbackNotifier.setGPSMethod(new_gps_processing_method_str);
        mParameters.set(CameraParameters::KEY_GPS_PROCESSING_METHOD, new_gps_processing_method_str);
    } else {
		mCallbackNotifier.setGPSMethod("");
        mParameters.remove(CameraParameters::KEY_GPS_PROCESSING_METHOD);
    }
	
	// JPEG thumbnail size
	int new_jpeg_thumbnail_width = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
	int new_jpeg_thumbnail_height= params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
	LOGV("%s : new_jpeg_thumbnail_width: %d, new_jpeg_thumbnail_height: %d",
		__FUNCTION__, new_jpeg_thumbnail_width, new_jpeg_thumbnail_height);
	if (0 <= new_jpeg_thumbnail_width && 0 <= new_jpeg_thumbnail_height) {
		mCallbackNotifier.setJpegThumbnailSize(new_jpeg_thumbnail_width, new_jpeg_thumbnail_height);
		mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, new_jpeg_thumbnail_width);
		mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, new_jpeg_thumbnail_height);
	}

	mFirstSetParameters = false;
	pthread_cond_signal(&mCommandCond);
	
    return NO_ERROR;
}

/* A dumb variable indicating "no params" / error on the exit from
 * CameraHardware::getParameters(). */
static char lNoParam = '\0';
char* CameraHardware::getParameters()
{
	F_LOG;
    String8 params(mParameters.flatten());
    char* ret_str =
        reinterpret_cast<char*>(malloc(sizeof(char) * (params.length()+1)));
    memset(ret_str, 0, params.length()+1);
    if (ret_str != NULL) {
        strncpy(ret_str, params.string(), params.length()+1);
        return ret_str;
    } else {
        LOGE("%s: Unable to allocate string for %s", __FUNCTION__, params.string());
        /* Apparently, we can't return NULL fron this routine. */
        return &lNoParam;
    }
}

void CameraHardware::putParameters(char* params)
{
	F_LOG;
    /* This method simply frees parameters allocated in getParameters(). */
    if (params != NULL && params != &lNoParam) {
        free(params);
    }
}

void CameraHardware::setNewCrop(Rect * rect)
{
	F_LOG;
	memcpy(&mFrameRectCrop, rect, sizeof(Rect));
}

status_t CameraHardware::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    LOGV("%s: cmd = %x, arg1 = %d, arg2 = %d", __FUNCTION__, cmd, arg1, arg2);

    /* TODO: Future enhancements. */

	switch (cmd)
	{
	case CAMERA_CMD_SET_CEDARX_RECORDER:
		mUseHwEncoder = true;
		mV4L2CameraDevice->setHwEncoder(true);
		return OK;
	case CAMERA_CMD_START_FACE_DETECTION:
	{
		const char *face = mParameters.get(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
		if (face == NULL || (atoi(face) <= 0))
		{
			return -EINVAL;
		}
		mQueueElement[CMD_QUEUE_START_FACE_DETECTE].cmd = CMD_QUEUE_START_FACE_DETECTE;
		OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_START_FACE_DETECTE]);
		pthread_cond_signal(&mCommandCond);
		return OK;
	}
	case CAMERA_CMD_STOP_FACE_DETECTION:
		mQueueElement[CMD_QUEUE_STOP_FACE_DETECTE].cmd = CMD_QUEUE_STOP_FACE_DETECTE;
		OSAL_Queue(&mQueueCommand, &mQueueElement[CMD_QUEUE_STOP_FACE_DETECTE]);
		pthread_cond_signal(&mCommandCond);
		return OK;
	case CAMERA_CMD_PING:
		return OK;
	case CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG:
	{
		bool enable = static_cast<bool>(arg1);
        if (enable) {
			enableMsgType(CAMERA_MSG_FOCUS_MOVE);
        } else {
			disableMsgType(CAMERA_MSG_FOCUS_MOVE);
        }
		return OK;
	}
	case CAMERA_CMD_SET_DISPLAY_ORIENTATION:
		LOGD("CAMERA_CMD_SET_DISPLAY_ORIENTATION, to do ...");
		return OK;
	}

    return -EINVAL;
}

void CameraHardware::releaseCamera()
{
    LOGV("%s", __FUNCTION__);

    cleanupCamera();
}

status_t CameraHardware::dumpCamera(int fd)
{
    LOGV("%s", __FUNCTION__);

    /* TODO: Future enhancements. */
    return -EINVAL;
}

/****************************************************************************
 * Facedetection management
 ***************************************************************************/

int CameraHardware::getCurrentFaceFrame(void * frame)
{
	return mV4L2CameraDevice->getCurrentFaceFrame(frame);
}

int CameraHardware::faceDetection(camera_frame_metadata_t *face)
{
	if (mZoomRatio != 0)
	{
		int number_of_faces = face->number_of_faces;
		for(int i =0; i < number_of_faces; i++)
		{
			face->faces[i].rect[0] = (face->faces[i].rect[0] * mZoomRatio) / 100;
			face->faces[i].rect[1] = (face->faces[i].rect[1] * mZoomRatio) / 100;
			face->faces[i].rect[2] = (face->faces[i].rect[2] * mZoomRatio) / 100;
			face->faces[i].rect[3] = (face->faces[i].rect[3] * mZoomRatio) / 100;
		}
	}
	return mCallbackNotifier.faceDetectionMsg(face);
}

/****************************************************************************
 * Preview management.
 ***************************************************************************/

status_t CameraHardware::doStartPreview()
{
	F_LOG;
	
	V4L2CameraDevice* const camera_dev = mV4L2CameraDevice;
	camera_dev->setTakePictureState(TAKE_PICTURE_NULL);

	if (camera_dev->isStarted()
		&& mPreviewWindow.isPreviewEnabled())
	{
		LOGD("CameraHardware::doStartPreview has been already started");
		return NO_ERROR;
	}
	
	if (camera_dev->isStarted()) 
	{
        camera_dev->stopDeliveringFrames();
        camera_dev->stopDevice();
    }

	status_t res = mPreviewWindow.startPreview();
    if (res != NO_ERROR) 
	{
        return res;
    }

	// Make sure camera device is connected.
	if (!camera_dev->isConnected())
	{
		res = camera_dev->connectDevice(&mHalCameraInfo);
		if (res != NO_ERROR) 
		{
			mPreviewWindow.stopPreview();
			return res;
		}

		setAutoFocusCtrl(V4L2_AF_INIT, NULL);
	}

	const char * valstr = mParameters.get(CameraParameters::KEY_RECORDING_HINT);
	bool video_hint = (strcmp(valstr, CameraParameters::TRUE) == 0);

	// preview size
	int preview_width = 0, preview_height = 0;
	const char * preview_capture_width_str = mParameters.get(KEY_PREVIEW_CAPTURE_SIZE_WIDTH);
	const char * preview_capture_height_str = mParameters.get(KEY_PREVIEW_CAPTURE_SIZE_HEIGHT);
	if (preview_capture_width_str != NULL
		&& preview_capture_height_str != NULL)
	{
		preview_width  = atoi(preview_capture_width_str);
		preview_height = atoi(preview_capture_height_str);
	}

	// video size
	int video_width = 0, video_height = 0;
	mParameters.getVideoSize(&video_width, &video_height);

	// capture size
	if (video_hint)
	{
		mCaptureWidth = mVideoCaptureWidth;
		mCaptureHeight= mVideoCaptureHeight;
	}
	else
	{
		if (mHalCameraInfo.fast_picture_mode)
		{
			mCaptureWidth = mFullSizeWidth;
			mCaptureHeight= mFullSizeHeight;
		}
		else
		{
			mCaptureWidth = preview_width;
			mCaptureHeight= preview_height;
		}
	}

	// preview format and video format are the same
	uint32_t org_fmt = V4L2_PIX_FMT_NV21;		// android default
	const char* preview_format = mParameters.getPreviewFormat();
	if (preview_format != NULL) 
	{
		if (strcmp(preview_format, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
		{
#ifdef __SUNXI__
			org_fmt = V4L2_PIX_FMT_NV12;		// SGX support NV12
#else
			org_fmt = V4L2_PIX_FMT_NV21;		// MALI support NV21
#endif
		}
		else if (strcmp(preview_format, CameraParameters::PIXEL_FORMAT_YUV420P) == 0)
		{
			org_fmt = V4L2_PIX_FMT_YVU420;		        // YV12 tmp for cts
			//org_fmt = V4L2_PIX_FMT_YUV420;		    // YU12		
		}
		else
		{
			LOGE("unknown preview format");
		}
	}
	
	LOGD("Starting camera: %dx%d -> %.4s(%s)",
         mCaptureWidth, mCaptureHeight, reinterpret_cast<const char*>(&org_fmt), preview_format);
    res = camera_dev->startDevice(mCaptureWidth, mCaptureHeight, org_fmt, video_hint);
    if (res != NO_ERROR) 
	{
        mPreviewWindow.stopPreview();
        return res;
    }
	
	res = camera_dev->startDeliveringFrames();
    if (res != NO_ERROR) 
	{
        camera_dev->stopDevice();
        mPreviewWindow.stopPreview();
    }
	
    return res;
}

status_t CameraHardware::doStopPreview()
{
	F_LOG;
	
	status_t res = NO_ERROR;
	if (mPreviewWindow.isPreviewEnabled()) 
	{
		/* Stop the camera. */
		if (mV4L2CameraDevice->isStarted()) 
		{
			mV4L2CameraDevice->stopDeliveringFrames();
			res = mV4L2CameraDevice->stopDevice();
		}

		if (res == NO_ERROR) 
		{
			/* Disable preview as well. */
			mPreviewWindow.stopPreview();
		}
	}

    return NO_ERROR;
}

/****************************************************************************
 * Private API.
 ***************************************************************************/

status_t CameraHardware::cleanupCamera()
{
	F_LOG;

    status_t res = NO_ERROR;

	mParameters.set(KEY_SNAP_PATH, "");
	mCallbackNotifier.setSnapPath("");

	// reset preview format to yuv420sp
	mParameters.set(CameraParameters::KEY_PREVIEW_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);

	mV4L2CameraDevice->setHwEncoder(false);

	mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, 320);
	mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, 240);

	mParameters.set(CameraParameters::KEY_ZOOM, 0);

	mVideoCaptureWidth = 0;
	mVideoCaptureHeight = 0;
	mUseHwEncoder = false;
	
	// stop focus thread
	pthread_mutex_lock(&mAutoFocusMutex);
	if (mAutoFocusThread->isThreadStarted())
	{
		mAutoFocusThread->stopThread();
		pthread_cond_signal(&mAutoFocusCond);
	}
	pthread_mutex_unlock(&mAutoFocusMutex);
	
    /* If preview is running - stop it. */
    res = doStopPreview();
    if (res != NO_ERROR) {
        return -res;
    }
    usleep(100000);

    /* Stop and disconnect the camera device. */
    V4L2CameraDevice* const camera_dev = mV4L2CameraDevice;
    if (camera_dev != NULL) 
	  {
        if (camera_dev->isStarted()) 
		    {
            camera_dev->stopDeliveringFrames();
            res = camera_dev->stopDevice();
            if (res != NO_ERROR) {
                return -res;
            }
        }
        if (camera_dev->isConnected()) 
		    {
            res = camera_dev->disconnectDevice();
            if (res != NO_ERROR) {
                return -res;
            }
        }
    }
    usleep(50000);

    mCallbackNotifier.cleanupCBNotifier();

	{
		Mutex::Autolock locker(&mCameraIdleLock);
		mIsCameraIdle = true;
	}

    return NO_ERROR;
}

/****************************************************************************
 * Camera API callbacks as defined by camera_device_ops structure.
 *
 * Callbacks here simply dispatch the calls to an appropriate method inside
 * CameraHardware instance, defined by the 'dev' parameter.
 ***************************************************************************/

int CameraHardware::set_preview_window(struct camera_device* dev,
                                       struct preview_stream_ops* window)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->setPreviewWindow(window);
}

void CameraHardware::set_callbacks(
        struct camera_device* dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}

void CameraHardware::enable_msg_type(struct camera_device* dev, int32_t msg_type)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->enableMsgType(msg_type);
}

void CameraHardware::disable_msg_type(struct camera_device* dev, int32_t msg_type)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->disableMsgType(msg_type);
}

int CameraHardware::msg_type_enabled(struct camera_device* dev, int32_t msg_type)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->isMsgTypeEnabled(msg_type);
}

int CameraHardware::start_preview(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->startPreview();
}

void CameraHardware::stop_preview(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->stopPreview();
}

int CameraHardware::preview_enabled(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->isPreviewEnabled();
}

int CameraHardware::store_meta_data_in_buffers(struct camera_device* dev,
                                               int enable)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->storeMetaDataInBuffers(enable);
}

int CameraHardware::start_recording(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->startRecording();
}

void CameraHardware::stop_recording(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->stopRecording();
}

int CameraHardware::recording_enabled(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->isRecordingEnabled();
}

void CameraHardware::release_recording_frame(struct camera_device* dev,
                                             const void* opaque)
{
	CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->releaseRecordingFrame(opaque);
}

int CameraHardware::auto_focus(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->setAutoFocus();
}

int CameraHardware::cancel_auto_focus(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->cancelAutoFocus();
}

int CameraHardware::take_picture(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->takePicture();
}

int CameraHardware::cancel_picture(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->cancelPicture();
}

int CameraHardware::set_parameters(struct camera_device* dev, const char* parms)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }

	int64_t lasttime = systemTime();
	int ret = ec->setParameters(parms);
	LOGV("setParameters use time: %lld(ms)", (systemTime() - lasttime)/1000000);
	
    return ret;
}

char* CameraHardware::get_parameters(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return NULL;
    }
    return ec->getParameters();
}

void CameraHardware::put_parameters(struct camera_device* dev, char* params)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->putParameters(params);
}

int CameraHardware::send_command(struct camera_device* dev,
                                 int32_t cmd,
                                 int32_t arg1,
                                 int32_t arg2)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->sendCommand(cmd, arg1, arg2);
}

void CameraHardware::release(struct camera_device* dev)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return;
    }
    ec->releaseCamera();
}

int CameraHardware::dump(struct camera_device* dev, int fd)
{
    CameraHardware* ec = reinterpret_cast<CameraHardware*>(dev->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->dumpCamera(fd);
}

int CameraHardware::close(struct hw_device_t* device)
{
    CameraHardware* ec =
        reinterpret_cast<CameraHardware*>(reinterpret_cast<struct camera_device*>(device)->priv);
    if (ec == NULL) {
        LOGE("%s: Unexpected NULL camera device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->closeCamera();
}

// -------------------------------------------------------------------------
// extended interfaces here <***** star *****>
// -------------------------------------------------------------------------

/****************************************************************************
 * Static initializer for the camera callback API
 ****************************************************************************/

camera_device_ops_t CameraHardware::mDeviceOps = {
    CameraHardware::set_preview_window,
    CameraHardware::set_callbacks,
    CameraHardware::enable_msg_type,
    CameraHardware::disable_msg_type,
    CameraHardware::msg_type_enabled,
    CameraHardware::start_preview,
    CameraHardware::stop_preview,
    CameraHardware::preview_enabled,
    CameraHardware::store_meta_data_in_buffers,
    CameraHardware::start_recording,
    CameraHardware::stop_recording,
    CameraHardware::recording_enabled,
    CameraHardware::release_recording_frame,
    CameraHardware::auto_focus,
    CameraHardware::cancel_auto_focus,
    CameraHardware::take_picture,
    CameraHardware::cancel_picture,
    CameraHardware::set_parameters,
    CameraHardware::get_parameters,
    CameraHardware::put_parameters,
    CameraHardware::send_command,
    CameraHardware::release,
    CameraHardware::dump
};

/****************************************************************************
 * Common keys
 ***************************************************************************/

const char CameraHardware::FACING_KEY[]         = "prop-facing";
const char CameraHardware::ORIENTATION_KEY[]    = "prop-orientation";
const char CameraHardware::RECORDING_HINT_KEY[] = "recording-hint";

/****************************************************************************
 * Common string values
 ***************************************************************************/

const char CameraHardware::FACING_BACK[]      = "back";
const char CameraHardware::FACING_FRONT[]     = "front";

/****************************************************************************
 * Helper routines
 ***************************************************************************/

static char* AddValue(const char* param, const char* val)
{
    const size_t len1 = strlen(param);
    const size_t len2 = strlen(val);
    char* ret = reinterpret_cast<char*>(malloc(len1 + len2 + 2));
    LOGE_IF(ret == NULL, "%s: Memory failure", __FUNCTION__);
    if (ret != NULL) {
        memcpy(ret, param, len1);
        ret[len1] = ',';
        memcpy(ret + len1 + 1, val, len2);
        ret[len1 + len2 + 1] = '\0';
    }
    return ret;
}

/****************************************************************************
 * Parameter debugging helpers
 ***************************************************************************/

#if DEBUG_PARAM
static void PrintParamDiff(const CameraParameters& current,
                            const char* new_par)
{
    char tmp[2048];
    const char* wrk = new_par;

    /* Divided with ';' */
    const char* next = strchr(wrk, ';');
    while (next != NULL) {
        snprintf(tmp, sizeof(tmp), "%.*s", next-wrk, wrk);
        /* in the form key=value */
        char* val = strchr(tmp, '=');
        if (val != NULL) {
            *val = '\0'; val++;
            const char* in_current = current.get(tmp);
            if (in_current != NULL) {
                if (strcmp(in_current, val)) {
                    LOGD("=== Value changed: %s: %s -> %s", tmp, in_current, val);
                }
            } else {
                LOGD("+++ New parameter: %s=%s", tmp, val);
            }
        } else {
            LOGW("No value separator in %s", tmp);
        }
        wrk = next + 1;
        next = strchr(wrk, ';');
    }
}
#endif  /* DEBUG_PARAM */

}; /* namespace android */
