
#ifndef __LIB__CAMERA__TYPE__H__
#define __LIB__CAMERA__TYPE__H__

typedef struct PREVIEWINFO_t
{
	int left;
	int top;
	int width;			// preview width
	int height;			// preview height
}PREVIEWINFO_t, RECT_t;

typedef struct V4L2BUF_t
{
	unsigned int	addrPhyY;		// physical Y address of this frame
	unsigned int	addrVirY;		// virtual Y address of this frame
	unsigned int	width;
	unsigned int	height;
	int 			index;			// DQUE id number
	long long		timeStamp;		// time stamp of this frame
	RECT_t			crop_rect;
	int				format;
	void*           overlay_info;
	
	// thumb 
	unsigned char	isThumbAvailable;
	unsigned char	thumbUsedForPreview;
	unsigned char	thumbUsedForPhoto;
	unsigned char	thumbUsedForVideo;
	unsigned int	thumbAddrPhyY;		// physical Y address of thumb buffer
	unsigned int	thumbAddrVirY;		// virtual Y address of thumb buffer
	unsigned int	thumbWidth;
	unsigned int	thumbHeight;
	RECT_t			thumb_crop_rect;
	int 			thumbFormat;
	
	int 			refCnt; 		// used for releasing this frame
}V4L2BUF_t;

typedef enum MEDIA_SRC_MODE
{
	MEDIA_SRC_PUSH_MODE,
	MEDIA_SRC_PULL_MODE
}MEDIA_SRC_MODE;

typedef struct VIDEOINFO_t
{
	int video_source;
	int src_height;
	int src_width;
	int height;			// camcorder video frame height
	int width;			// camcorder video frame width
	int frameRate;		// camcorder video frame rate
	int bitRate;		// camcorder video bitrate
	short profileIdc;
	short levelIdc;

	int geo_available;
	int latitudex10000;
	int longitudex10000;

	// rotate
	int rotate_degree;		// only support 0, 90, 180 and 270
}VIDEOINFO_t;

typedef enum AUDIO_ENCODER_TYPE
{
	AUDIO_ENCODER_AAC_TYPE,
	AUDIO_ENCODER_LPCM_TYPE
}AUDIO_ENCODER_TYPE;
typedef struct AUDIOINFO_t
{
	int sampleRate;
	int channels;
	int bitRate;
	int bitsPerSample;
	int audioEncType;  // 0: aac, 1: LPCM
}AUDIOINFO_t;

typedef struct ENCEXTRADATAINFO_t //don't touch it, because it also defined in type.h
{
	char *data;
	int length;
}ENCEXTRADATAINFO_t;

typedef struct ENC_BUFFER_t
{
    int addrY;
	int addrCb;
	int addrCr;
	int width;
	int height;
	RECT_t crop_rect;
	int force_keyframe;
	void*  overlay_info;
	int format;
}ENC_BUFFER_t;

typedef enum JPEG_COLOR_FORMAT
{
    JPEG_COLOR_YUV444,
    JPEG_COLOR_YUV422,
    JPEG_COLOR_YUV420,
    JPEG_COLOR_YUV411,
    JPEG_COLOR_YUV420_NV12,
    JPEG_COLOR_YUV420_NV21,
    JPEG_COLOR_TILE_32X32,
    JPEG_COLOR_CSIARGB,
    JPEG_COLOR_CSIRGBA,
    JPEG_COLOR_CSIABGR,
    JPEG_COLOR_CSIBGRA
}JPEG_COLOR_FORMAT;

typedef struct JPEG_ENC_t
{
	int				src_w;
	int				src_h;
	int				pic_w;
	int				pic_h;
	int				addrY;
	int				addrC;
	int				colorFormat;
	int				quality;
	int				rotate;

	int				scale_factor;
	double			focal_length;
	
	int				thumbWidth;
	int				thumbHeight;

	unsigned char	enable_crop;
	int				crop_x;	
	int				crop_y;
	int				crop_w;
	int				crop_h;

	// gps exif
	unsigned char	enable_gps;
	double      	gps_latitude;
	double			gps_longitude;
	long        	gps_altitude;  
	long        	gps_timestamp;
	char			gps_processing_method[100];
	int 			whitebalance;
	char  			CameraMake[64];//for the cameraMake name
	char  			CameraModel[64];//for the cameraMode
	char  			DateTime[21];//for the data and time
}JPEG_ENC_t;

#endif // __LIB__CAMERA__TYPE__H__

