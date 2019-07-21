////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// BandiVideoLibrary 2.1
/// 
/// Copyright(C) 2008-2014 BandiSoft.com All rights reserved.
///
/// Visit http://www.bandisoft.com for more information.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <process.h>
#include <math.h>
#include "BandiVideoSound_DS.h"

#define BVL_DSWND_CLASS_NAME	_T("CBandiVideoSound_DS")


CBandiVideoSound_DS::CBandiVideoSound_DS() : m_ref_count(0)
{
	m_hwnd = NULL;
	m_hinst = NULL;
	m_ds = NULL;
	m_dsb_primary = NULL;
	m_dsb_secondary = NULL;

	m_channels = m_samplerate = 0;	
	m_last_write_pos = 0;	
	m_inbuf_samples = m_total_samples = 0;

	m_cur_play_time = 0;

	memset(&m_crit_sec, NULL, sizeof(m_crit_sec));
	m_crit_sec_inited = FALSE;
	
	m_hevent = NULL;
	m_hthread = NULL;
	m_thread_id = 0;

	m_playing = FALSE;
	m_callback = NULL;

	m_outbuf_size_max = -1;
	m_outbuf_size_min = -1;

	memset(m_sample_block, NULL, PLAY_SAMPLE_BLOCK_LEN);
	m_sample_block_size = 0;
}


CBandiVideoSound_DS::~CBandiVideoSound_DS()
{
	Close();
}


STDMETHODIMP_(ULONG) CBandiVideoSound_DS::AddRef()
{                             
    LONG lRef = InterlockedIncrement(&m_ref_count);
	ASSERT(lRef >= 0);

    return m_ref_count;
}


STDMETHODIMP_(ULONG) CBandiVideoSound_DS::Release()
{
    LONG lRef = InterlockedDecrement(&m_ref_count);
    ASSERT(lRef >= 0);
   
	if(lRef == 0) 
	{
        delete this;
        return ULONG(0);
    }

	return m_ref_count;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Open sound device.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT		CBandiVideoSound_DS::Open(INT channels, INT samplerate, IBandiVideoSoundCB *callback)
{
	Close();

	HRESULT hr = E_FAIL;
	
	m_hinst = (HINSTANCE)GetModuleHandle(NULL);
	if(m_hinst == NULL) goto error;
	
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = BVL_DSWND_CLASS_NAME;
	RegisterClass(&wc);

	// 버퍼타임으로 버퍼 byte 크기 계산
	m_outbuf_size_max = MulDiv(samplerate * channels * 2, MAX_BUFFER_TIME , 1000) ;
	m_outbuf_size_min = MulDiv(samplerate * channels * 2,  MIN_BUFFER_TIME ,1000 );

	// Create dummy window
	m_hwnd = CreateWindow(BVL_DSWND_CLASS_NAME, _T(""), 0, 0, 0, 0, 0, NULL, NULL, m_hinst, 0L);
	if(m_hwnd == NULL)
		goto error;
	
	// Create Direct Sound Instance
	hr = DirectSoundCreate8(NULL, &m_ds, NULL);
	if(FAILED(hr)) 
		goto error;

	hr = m_ds->SetCooperativeLevel(m_hwnd, DSSCL_PRIORITY);
	if(FAILED(hr)) 
		goto error;
	
	// Create a primary buffer
	DSBUFFERDESC dsbdesc;
	ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	hr = m_ds->CreateSoundBuffer(&dsbdesc, &m_dsb_primary, NULL);
	if(FAILED(hr)) 
		goto error;

	// Set format of the primary buffer
	WAVEFORMATEX wfx;
	ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
	wfx.wFormatTag		= WAVE_FORMAT_PCM;
	wfx.nChannels = channels;
	wfx.nSamplesPerSec = samplerate;
	wfx.wBitsPerSample	= 16;
	wfx.nBlockAlign		= wfx.wBitsPerSample / 8 * wfx.nChannels;
	wfx.nAvgBytesPerSec	= wfx.nSamplesPerSec * wfx.nBlockAlign;
	hr = m_dsb_primary->SetFormat(&wfx);
	if(FAILED(hr)) 
		goto error;

	// Create secondary sound buffer
	ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS
					/*| DSBCAPS_LOCDEFER | DSBCAPS_CTRLFREQUENCY*/ | DSBCAPS_CTRLVOLUME;
	dsbdesc.dwBufferBytes = m_outbuf_size_max;
	dsbdesc.lpwfxFormat = &wfx;
	hr = m_ds->CreateSoundBuffer(&dsbdesc, &m_dsb_secondary, NULL);
	if(FAILED(hr)) 
		goto error;

	InitializeCriticalSection(&m_crit_sec);
	m_crit_sec_inited = TRUE;
	
	// Create an event object for periodical processing
	m_hevent = CreateEvent(NULL, FALSE, FALSE, NULL);	// auto reset event
	if(m_hevent == NULL)
		goto error;

	// Create thread for playback
	m_hthread = (HANDLE)_beginthreadex(NULL, 0, InitialThreadProc, this, CREATE_SUSPENDED, &m_thread_id);
	if(m_hthread == NULL)
		goto error;

	m_channels = channels;
	m_samplerate = samplerate;

	m_callback = callback;

	memset(m_sample_block, NULL, PLAY_SAMPLE_BLOCK_LEN);
	m_sample_block_size = 0;

	SetThreadPriority(m_hthread, THREAD_PRIORITY_TIME_CRITICAL);
	ResumeThread(m_hthread);

	return S_OK;

error:
	Close();

	return FAILED(hr) ? hr : E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Close sound device.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
void		CBandiVideoSound_DS::Close()
{
    HANDLE hThread = (HANDLE)InterlockedExchangePointer(&m_hthread, 0);
    if(hThread) 
	{
		if(m_hevent) SetEvent(m_hevent);

		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
	}
	
	if(m_hevent)
	{
		CloseHandle(m_hevent);
		m_hevent = NULL;
	}
	
	if(m_crit_sec_inited)
	{
		DeleteCriticalSection(&m_crit_sec);
		m_crit_sec_inited = FALSE;
	}
	
	if(m_dsb_secondary) 
	{
		m_dsb_secondary->Release();
		m_dsb_secondary = NULL;
	}

	if(m_dsb_primary)
	{
		m_dsb_primary->Release();
		m_dsb_primary = NULL;
	}

	if(m_ds)
	{
		m_ds->Release();
		m_ds = NULL;
	}

	if(m_hwnd)
	{
		DestroyWindow(m_hwnd);
		UnregisterClass(BVL_DSWND_CLASS_NAME, m_hinst);
		m_hwnd = NULL;
	}

	m_channels = m_samplerate = 0;	
	m_last_write_pos = 0;	
	m_inbuf_samples = m_total_samples = 0;
	m_cur_play_time = 0;

	m_playing = FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Start 
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT		CBandiVideoSound_DS::Start()
{
	EnterCriticalSection(&m_crit_sec);

	m_playing = TRUE;

	LeaveCriticalSection(&m_crit_sec);

	return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Stop
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT		CBandiVideoSound_DS::Stop()
{
	EnterCriticalSection(&m_crit_sec);

	m_playing = FALSE;

	if(m_dsb_secondary)
	{
		BYTE *pbData = NULL;
		BYTE *pbData2 = NULL;
		DWORD dwLength=0;
		DWORD dwLength2=0;

		m_dsb_secondary->Stop();
		m_dsb_secondary->SetCurrentPosition(0);

		// Lock DirectSound buffer
		m_dsb_secondary->Lock(0, m_outbuf_size_max, (void**)&pbData, &dwLength, (void**)&pbData2, &dwLength2, 0);	
		
		if(pbData) memset(pbData, 0, dwLength);
		if(pbData2) memset(pbData2, 0, dwLength2);

		// Unlock the buffer
		m_dsb_secondary->Unlock(pbData, dwLength, pbData2, dwLength2);	

		m_last_write_pos = 0;	
		m_total_samples = m_inbuf_samples = 0;
		m_cur_play_time = 0;

		memset(m_sample_block, NULL, PLAY_SAMPLE_BLOCK_LEN);
		m_sample_block_size = 0;
	}

	LeaveCriticalSection(&m_crit_sec);

	return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Stop
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT		CBandiVideoSound_DS::Pause(BOOL pause)
{
	EnterCriticalSection(&m_crit_sec);

	if(pause)
	{	
		m_playing = FALSE;
		if(m_dsb_secondary) m_dsb_secondary->Stop();
	}
	else
	{
		m_playing = TRUE;
	}

	LeaveCriticalSection(&m_crit_sec);

	return S_OK;
}


#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Get play time (100nsec units)
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT	CBandiVideoSound_DS::GetTime(INT64 &time)
{
	EnterCriticalSection(&m_crit_sec);

	time = m_cur_play_time;


	LeaveCriticalSection(&m_crit_sec);

	return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Set sound volume
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT		CBandiVideoSound_DS::SetVolume(INT volume)
{
	if(m_dsb_secondary == NULL)
		return E_POINTER;

	double lfPower10 = pow(10.0,-10.0); // 10^(-10);
	double lfVolume = (1 - lfPower10)*((double)volume/255) + lfPower10;
	
	lfVolume = 10*log10(lfVolume);
	lfVolume *= 300;
	
	if(lfVolume < -10000)
		lfVolume = -10000;

	return m_dsb_secondary->SetVolume((LONG)lfVolume);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// ThreadProc
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
UINT CBandiVideoSound_DS::ThreadProc()
{
	BOOL bWorking = FALSE;
	while(m_hthread)
	{
		EnterCriticalSection(&m_crit_sec);
		{
			if(m_playing)
				PlayDS();
		}
		LeaveCriticalSection(&m_crit_sec);
		
		WaitForSingleObject(m_hevent, 2);
	}
	
	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Play DirectSound
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CBandiVideoSound_DS::PlayDS()
{
	HRESULT hr;
	int sample_bytes = m_channels*sizeof(INT16);
	DWORD free_buffer_size = GetFreeBufferSize();

	//	Update play position
	m_inbuf_samples = (m_outbuf_size_max - free_buffer_size)/(m_channels*sizeof(INT16));
	m_cur_play_time = max((m_total_samples - m_inbuf_samples)*10000000i64/m_samplerate, 0);

	//	Transfer PCM data to Direct Sound Buffer
	if(m_callback)
	{
		void *lock1_ptr = NULL;
		void *lock2_ptr = NULL;
		DWORD lock1_bytes = 0;
		DWORD lock2_bytes = 0;

		if(m_sample_block_size < PLAY_SAMPLE_BLOCK_LEN)
		{
			int req_samples = (PLAY_SAMPLE_BLOCK_LEN-m_sample_block_size)/sample_bytes;
			int get_samples = m_callback->GetPcmData((INT16*)(&m_sample_block[m_sample_block_size]), req_samples);
			m_sample_block_size += get_samples*sample_bytes;
		}

		if(m_sample_block_size == PLAY_SAMPLE_BLOCK_LEN && m_sample_block_size <= free_buffer_size) 
		{
			hr = m_dsb_secondary->Lock(m_last_write_pos, m_sample_block_size,
											&lock1_ptr, &lock1_bytes,
											&lock2_ptr, &lock2_bytes, 0);

			if(hr == DSERR_BUFFERLOST) 
			{
				m_dsb_secondary->Restore();
				hr = m_dsb_secondary->Lock(m_last_write_pos, m_sample_block_size,
											&lock1_ptr, &lock1_bytes,
											&lock2_ptr, &lock2_bytes, 0);
			}

			if(FAILED(hr)) return hr;

			if(lock1_ptr) memcpy(lock1_ptr, m_sample_block, lock1_bytes);
			if(lock2_ptr) memcpy(lock2_ptr, m_sample_block+lock1_bytes, lock2_bytes);

			m_dsb_secondary->Unlock(lock1_ptr, lock1_bytes, lock2_ptr, lock2_bytes);
		
			//	Update Write Pointer
			m_total_samples += m_sample_block_size/sample_bytes;
			m_last_write_pos += m_sample_block_size;
			if (INT(m_last_write_pos) >= m_outbuf_size_max)
				m_last_write_pos = m_last_write_pos - m_outbuf_size_max;

			m_sample_block_size = 0;
		}
	}

	//	PLAYBACK control
	DWORD dsb_status;	
	m_dsb_secondary->GetStatus(&dsb_status);
	if((dsb_status & DSBSTATUS_PLAYING) == 0) 
	{ 
		if (m_inbuf_samples >= m_outbuf_size_min) 
		{
			m_dsb_secondary->Play(0, 0, DSBPLAY_LOOPING);
		}
	}

	return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Get amount of free data in the buffer
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD	CBandiVideoSound_DS::GetFreeBufferSize()
{
	if(m_dsb_secondary == NULL) return 0;

	if(m_total_samples == 0)
		return m_outbuf_size_max;

	DWORD play_pos;
	DWORD free_buffer_size = 0;
	if(SUCCEEDED(m_dsb_secondary->GetCurrentPosition(&play_pos, NULL)))
	{
		if(m_last_write_pos > play_pos)
			free_buffer_size = (m_outbuf_size_max - m_last_write_pos) + play_pos;
		else
			free_buffer_size = play_pos - m_last_write_pos;
	}
	else
	{
		free_buffer_size = m_outbuf_size_max;
	}

	return free_buffer_size;
}
