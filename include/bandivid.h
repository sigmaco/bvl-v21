////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// BandiVideoLibrary 2.1
/// 
/// Copyright(C) 2008-2014 BandiSoft.com All rights reserved.
///
/// Author: denny
///
/// Visit http://www.bandisoft.com for more information.
///
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BANDIVID_H
#define _BANDIVID_H

#include <windows.h>
#include <unknwn.h>

#ifndef ASSERT
#	include "assert.h"
#	define ASSERT(x) assert(x)
#endif

#ifndef _INC_TCHAR
#	ifdef _UNICODE
#		define _T(x)		L##x
#		define _stprintf	swprintf
#		define _stprintf_s  swprintf_s
#	else
#		define _T(x)	x
#		define _stprintf   sprintf
#		define _stprintf_s	sprintf_s
#	endif
#endif

#define _BVL_MAKE_VER(a,b,c,d)	((a<<24)+(b<<16)+(c<<8)+d)
#define BVL_VERSION				_BVL_MAKE_VER(2,1,5,19)		// 2.1.5.19


// pixel type of video in movie file
enum BVL_PIXEL_FORMAT
{
	BVLPF_NULL,						
	BVLPF_X8R8G8B8,					// use D3DFMT_X8R8G8B8 in D3D9	
	BVLPF_R8G8B8,					// use D3DFMT_R8G8B8 in D3D9	
	BVLPF_R5G6B5,					// use D3DFMT_R5G6B5 in D3D9	
	BVLPF_R5G5B5,					// use D3DFMT_X1R5G5B5 in D3D9	

	BVLPF_A8R8G8B8,					// use D3DFMT_A8R8G8B8 in D3D9	
};


// Seeking flags
enum BVL_SEEK_FLAG
{
	BVL_SEEK_TIME,					// time in 100-nanosecond
	BVL_SEEK_FRAME,					// video frame number
};


// Current status of Video player
enum BVL_STATUS
{
	BVL_STATUS_ERROR = -1,
	BVL_STATUS_CLOSED = 0,
	BVL_STATUS_OPENING,
	BVL_STATUS_READY,
	BVL_STATUS_PLAYING,
	BVL_STATUS_PAUSED,
	BVL_STATUS_PLAYEND	
};


// Frame data
struct BVL_FRAME
{
	void*				frame_buf;		// Pointer to the frame buffer
	INT					frame_buf_size;	// Number of bytes in a frame
	INT					pitch;			// Number of bytes in one row of the surface.
	INT					width;			// Frame width
	INT					height;			// Frame height
	BVL_PIXEL_FORMAT	pixel_format;	// Pixel format

	INT64				frame_time;		// 100nsec
	INT64				frame_number;
};


// Video infomation
struct BVL_VIDEO_INFO
{
	INT		width;
	INT		height;
	INT		fps_rate;
	INT		fps_scale;

	INT		samplerate;
	INT		channels;

	INT64	total_time;					// 100nsec
	INT64	total_frames;
};


// IBandiVideoFileReader interface
struct IBandiVideoFileReader : public IUnknown
{
	virtual HRESULT		Open(LPCWSTR path_name) PURE;
	virtual void		Close() PURE;
	virtual	INT			Read(BYTE* buf, INT bytes_to_read) PURE;
	virtual INT64		SetPosition(INT64 pos) PURE;
	virtual INT64		GetPosition() PURE;
	virtual INT64		GetFileSize() PURE;
};


// IBandiVideoSoundCB interface
struct IBandiVideoSoundCB : public IUnknown
{
	virtual INT GetPcmData(INT16* pcm_buf, INT req_samples) PURE;
};


// IBandiVideoSound interface
struct IBandiVideoSound : public IUnknown
{
	virtual HRESULT		Open(INT channels, INT samplerate, IBandiVideoSoundCB *callback) PURE;
	virtual void		Close() PURE;

	virtual HRESULT		Start() PURE;
	virtual HRESULT		Stop() PURE;
	virtual HRESULT		Pause(BOOL pause) PURE;

	virtual HRESULT		GetTime(INT64 &time) PURE;
	virtual HRESULT		SetVolume(INT volume) PURE;				// 0 ~ 255
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// IBandiVideo2 Interface
////////////////////////////////////////////////////////////////////////////////////////////////////
struct IBandiVideo2 : public IUnknown
{
	virtual HRESULT		Verify(LPCSTR id, LPCSTR key) PURE;		// Verify the license

	virtual INT			GetVersion() PURE;						// Version number

	// playback
	virtual HRESULT		Open(LPCWSTR path_name, BOOL async) PURE;
	virtual HRESULT		Close() PURE;
	virtual HRESULT		Play() PURE;
	virtual HRESULT		Stop() PURE;
	virtual HRESULT		Seek(INT64 value, BVL_SEEK_FLAG seek_flag) PURE;
	virtual HRESULT		Pause(BOOL pause) PURE;

	virtual BOOL		IsNextFrame() PURE;
	virtual HRESULT		GetFrame(BVL_FRAME &frame, BOOL can_skip) PURE;

	virtual HRESULT		GetVideoInfo(BVL_VIDEO_INFO &info) PURE;
	virtual HRESULT		GetStatus(BVL_STATUS &status) PURE;

	virtual HRESULT		SetVolume(INT volume) PURE;				// 0 ~ 255 
};


typedef HRESULT (*LPCREATEBANDIVIDEO2)(UINT sdk_version, void** pp, IBandiVideoFileReader* reader, IBandiVideoSound *sound);



////////////////////////////////////////////////////////////////////////////////////////////////////
// IBandiVideo2 error code
////////////////////////////////////////////////////////////////////////////////////////////////////
#define _FACBV  0x586
#define MAKE_BVHRESULT( code )				MAKE_HRESULT( 1, _FACBV, code )
#define MAKE_BVSTATUS( code )				MAKE_HRESULT( 0, _FACBV, code )

#define BV_OK								S_OK
#define BV_FALSE							S_FALSE

#define BVERR_FAIL							E_FAIL						 // Failed to appropriate operatio

#define BVERR_LOAD_LIBRARY_FAIL				MAKE_BVHRESULT(0x1020)		 // Faild to LoadLibrary()
#define BVERR_ALREADY_CREATED				MAKE_BVHRESULT(0x1021)		 // Video library instance is already created
#define BVERR_GET_PROC_ADDRESS_FAIL			MAKE_BVHRESULT(0x1022)		 // Failed to get proc address of CreateBandiVideo()
#define BVERR_LIBRARY_NOT_LOADED			MAKE_BVHRESULT(0x1023)		 // Video library is not loaded
#define BVERR_UNSUPPORTED_OS				MAKE_BVHRESULT(0x1024)		 // Unsupported operating system

#define BVERR_INVALID_VERSION				MAKE_BVHRESULT(0x1030)		 // Mismatch DLL version and header file version
#define BVERR_OUT_OF_MEMORY					MAKE_BVHRESULT(0x1031)		 //	Out of memory to create video library instance

#define BVERR_OPEN_FAILED					MAKE_BVHRESULT(0x1040)       // Unsupported file or not exist
#define BVERR_NOT_OPENED					MAKE_BVHRESULT(0x1041)		 // Unopened video file 

#endif // _BANDIVID_H
