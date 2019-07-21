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

#include "bandivid.h"

class CBandiVideoFileReader : public IBandiVideoFileReader
{
public:
	CBandiVideoFileReader();
	virtual ~CBandiVideoFileReader();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv) { return E_NOTIMPL; };
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IBandiVideoFileReader
	HRESULT	Open(LPCWSTR path_name);
	void	Close();
	INT		Read(BYTE* buf, INT bytes_to_read);
	INT64	SetPosition(INT64 pos);
	INT64	GetPosition();
	INT64	GetFileSize();

protected:
	LONG	m_ref_count;
	HANDLE	m_file_handle;
};

