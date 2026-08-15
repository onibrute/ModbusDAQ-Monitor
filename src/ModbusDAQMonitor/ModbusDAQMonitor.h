#pragma once

#ifndef __AFXWIN_H__
#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"

class CModbusDaqMonitorApp final : public CWinApp
{
public:
    CModbusDaqMonitorApp();
    BOOL InitInstance() override;

    DECLARE_MESSAGE_MAP()
};

extern CModbusDaqMonitorApp theApp;
