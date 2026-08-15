#include "pch.h"
#include "framework.h"
#include "ModbusDAQMonitor.h"
#include "MainDialog.h"

#include <memory>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CModbusDaqMonitorApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CModbusDaqMonitorApp::CModbusDaqMonitorApp()
{
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}

CModbusDaqMonitorApp theApp;

BOOL CModbusDaqMonitorApp::InitInstance()
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);

    CWinApp::InitInstance();
    AfxInitRichEdit2();
    AfxEnableControlContainer();

    auto shellManager = std::make_unique<CShellManager>();
    CMFCVisualManager::SetDefaultManager(
        RUNTIME_CLASS(CMFCVisualManagerWindows));
    SetRegistryKey(_T("Preda Robert Constantin\\ModbusDAQ Monitor"));

    CMainDialog dialog;
    m_pMainWnd = &dialog;
    const INT_PTR response = dialog.DoModal();
    if (response == -1)
    {
        TRACE(
            traceAppMsg,
            0,
            "Warning: main dialog creation failed; application will terminate.\n");
    }

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
    ControlBarCleanUp();
#endif

    return FALSE;
}
