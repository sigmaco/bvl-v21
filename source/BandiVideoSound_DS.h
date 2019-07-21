////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// BandiVideoLibrary 2.1
/// 
/// Copyright(C) 2008-2014 BandiSoft.com All rights reserved.
///
/// Visit http://www.bandisoft.com for more information.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#define DIRECTSOUND_VERSION 0x0800

#include <dsound.h>
#include "bandivid.h"


class CBandiVideoSound_DS : public IBandiVideoSound
{
public:
	static const UINT MAX_BUFFER_TIME	= 200;	// ms
	static const UINT MIN_BUFFER_TIME	= 20;	// ms

	static const UINT PLAY_SAMPLE_BLOCK = 1000;
	static const UINT PLAY_SAMPLE_BLOCK_LEN = PLAY_SAMPLE_BLOCK*4;

	CBandiVideoSound_DS();
	virtual ~CBandiVideoSound_DS();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv) { return E_NOTIMPL; };
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IBandiVideoSound
	HRESULT		Open(INT channels, INT samplerate, IBandiVideoSoundCB *callback);
	void		Close();
	HRESULT		Start();
	HRESULT		Stop();
	HRESULT		Pause(BOOL pause);
	HRESULT		GetTime(INT64 &time);
	HRESULT		SetVolume(INT volume);

protected:
	static unsigned WINAPI InitialThreadProc(LPVOID param)
	{
		CBandiVideoSound_DS *p = (CBandiVideoSound_DS*)param;
		return p->ThreadProc();		
	};
	UINT	ThreadProc();
	HRESULT	PlayDS();
	DWORD	GetFreeBufferSize();

	LONG				m_ref_count;

	HWND				m_hwnd;				// Window Handle
	HINSTANCE			m_hinst;			// Instance Handle
	LPDIRECTSOUND8		m_ds;				// DirectSound Interface
	LPDIRECTSOUNDBUFFER	m_dsb_primary;		// Primary buffer Interface
	LPDIRECTSOUNDBUFFER m_dsb_secondary;	// DirectSound secondary sound buffer

	UINT				m_channels;			// Number of Channnel
	UINT				m_samplerate;		// Sample rate;

	INT					m_outbuf_size_max;	
	INT					m_outbuf_size_min;
	
	UINT				m_last_write_pos;	// Last write offset (in BYTE)
	
	INT					m_inbuf_samples;	// Number of samples in the buffer
	INT					m_total_samples;	// Total number of written samples

	LONGLONG			m_cur_play_time;	// 100nsec

	CRITICAL_SECTION	m_crit_sec;
	BOOL				m_crit_sec_inited;
	
	HANDLE				m_hevent;
	HANDLE				m_hthread;
	UINT				m_thread_id;

	BOOL				m_playing;

	BYTE				m_sample_block[PLAY_SAMPLE_BLOCK_LEN];
	UINT				m_sample_block_size;

	IBandiVideoSoundCB*	m_callback;
};
