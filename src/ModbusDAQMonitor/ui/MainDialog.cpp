#include "pch.h"
#include "framework.h"
#include "ModbusDAQMonitor.h"
#include "MainDialog.h"
#include "resource.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <exception>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <atlimage.h>
#include <atlconv.h>
#include <Uxtheme.h>
#pragma comment(lib, "UxTheme.lib")
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <tchar.h>
#include "HelpDialog.h"
#pragma comment(lib, "setupapi.lib")
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
constexpr UINT kAsyncRequestCompleteMessage = WM_APP + 1;
}

BEGIN_MESSAGE_MAP(CMainDialog, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_TIMER()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_HSCROLL()
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
    ON_WM_DRAWITEM()
    ON_MESSAGE(kAsyncRequestCompleteMessage, &CMainDialog::OnAsyncRequestComplete)

    ON_BN_CLICKED(IDC_BUTTON_STARTSTOP, &CMainDialog::OnBnClickedButtonStartstop)
    ON_BN_CLICKED(IDC_BUTTON_HELP, &CMainDialog::OnBnClickedButtonHelp)
    ON_BN_CLICKED(IDC_BUTTON_APPLYINTERVAL, &CMainDialog::OnBnClickedButtonApplyInterval)
    ON_BN_CLICKED(IDC_BUTTON_RESET, &CMainDialog::OnBnClickedButtonReset)
    ON_BN_CLICKED(IDC_BUTTON_EXPORT_CSV, &CMainDialog::OnBnClickedExportCsv)
    ON_BN_CLICKED(IDC_BUTTON_SAVE_IMAGE, &CMainDialog::OnBnClickedSaveImage)
    ON_BN_CLICKED(IDC_BUTTON_LOG, &CMainDialog::OnBnClickedButtonLog)
    ON_BN_CLICKED(IDC_CHECK_CH0, &CMainDialog::OnBnClickedChannelVisibility)
    ON_BN_CLICKED(IDC_CHECK_CH1, &CMainDialog::OnBnClickedChannelVisibility)
    ON_BN_CLICKED(IDC_CHECK_CH2, &CMainDialog::OnBnClickedChannelVisibility)
    ON_BN_CLICKED(IDC_CHECK_CH3, &CMainDialog::OnBnClickedChannelVisibility)
    ON_BN_CLICKED(IDC_CHECK_DARKMODE, &CMainDialog::OnBnClickedCheckDarkMode)
    ON_BN_CLICKED(IDC_CHECK_AUTOSCALE_Y, &CMainDialog::OnBnClickedCheckAutoscaleY)
    ON_BN_CLICKED(IDC_CHECK_SHOWPEAKS, &CMainDialog::OnBnClickedCheckShowPeaks)
    ON_EN_CHANGE(IDC_EDIT_THRESH_MAX, &CMainDialog::OnEnChangeEditThreshMax)
    ON_EN_CHANGE(IDC_EDIT_THRESH_MIN, &CMainDialog::OnEnChangeEditThreshMin)
    ON_BN_CLICKED(IDC_SAVE_POINT_1, &CMainDialog::OnBnClickedSavePoint1)
    ON_BN_CLICKED(IDC_SAVE_POINT_2, &CMainDialog::OnBnClickedSavePoint2)
    ON_BN_CLICKED(IDC_BUTTON_SIMULATION, &CMainDialog::OnBnClickedButtonSimulation)
    ON_BN_CLICKED(IDC_CALIBRATE, &CMainDialog::OnBnClickedCalibrate)
    ON_CBN_SELCHANGE(IDC_COM_PORT_LIST, &CMainDialog::OnCbnSelchangeComPort)
END_MESSAGE_MAP()


COLORREF GetColorForLevel(int ch, AlarmLevel level)
{
    static const std::array<COLORREF, kChannelCount> normal = {
        RGB(0, 255, 255),
        RGB(128, 0, 128),
        RGB(0, 200, 0),
        RGB(200, 0, 200)
    };

    if (ch < 0 || ch >= static_cast<int>(kChannelCount))
        ch = 0;

    switch (level)
    {
    case AlarmLevel::Normal:   return normal[ch];
    case AlarmLevel::Warning:  return RGB(255, 255, 0);
    case AlarmLevel::Alarm:    return RGB(255, 0, 0);
    default:       return normal[ch];
    }
}


CMainDialog::CMainDialog(CWnd* pParent)
    : CDialogEx(IDD_MODBUS_DAQ_MONITOR_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    for (auto& points : m_chartPoints)
    {
        points.reserve(m_historySize);
    }
}

CMainDialog::~CMainDialog()
{
    if (::IsWindow(GetSafeHwnd()))
    {
        KillTimer(m_timerId);
    }
    StopAsyncRequest();
    m_csvLogger.Stop();
    m_acquisition.Disconnect();
    if (m_hbrBackground) ::DeleteObject(m_hbrBackground);
}

void CMainDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);


    DDX_Control(pDX, IDC_COM_PORT_LIST, m_comboComPorts);
    DDX_Control(pDX, IDC_STATIC_GRAPH, m_graphStatic);
    DDX_Control(pDX, IDC_SLIDER_HISTORY, m_sliderHistory);


    DDX_Control(pDX, IDC_RICH_WARNINGS, m_warnings);
    DDX_Control(pDX, IDC_RICH_ALARMS, m_alarms);


    DDX_Control(pDX, IDC_EDIT_THRESH_MIN, m_editThreshMin);
    DDX_Control(pDX, IDC_EDIT_THRESH_MAX, m_editThreshMax);


    DDX_Control(pDX, IDC_STATIC_AMPER_CH0, m_staticCurrent[0]);
    DDX_Control(pDX, IDC_STATIC_AVG_CH0, m_staticAverage[0]);
    DDX_Control(pDX, IDC_STATIC_AMPER_CH1, m_staticCurrent[1]);
    DDX_Control(pDX, IDC_STATIC_AVG_CH1, m_staticAverage[1]);
    DDX_Control(pDX, IDC_STATIC_AMPER_CH2, m_staticCurrent[2]);
    DDX_Control(pDX, IDC_STATIC_AVG_CH2, m_staticAverage[2]);
    DDX_Control(pDX, IDC_STATIC_AMPER_CH3, m_staticCurrent[3]);
    DDX_Control(pDX, IDC_STATIC_AVG_CH3, m_staticAverage[3]);


    DDX_Control(pDX, IDC_STATIC_HYSTERESIS_CH0, m_staticHystLabel[0]);
    DDX_Control(pDX, IDC_EDIT_HYSTERESIS_CH0, m_editHysteresis[0]);
    DDX_Control(pDX, IDC_STATIC_HYSTERESIS_CH1, m_staticHystLabel[1]);
    DDX_Control(pDX, IDC_EDIT_HYSTERESIS_CH1, m_editHysteresis[1]);
    DDX_Control(pDX, IDC_STATIC_HYSTERESIS_CH2, m_staticHystLabel[2]);
    DDX_Control(pDX, IDC_EDIT_HYSTERESIS_CH2, m_editHysteresis[2]);
    DDX_Control(pDX, IDC_STATIC_HYSTERESIS_CH3, m_staticHystLabel[3]);
    DDX_Control(pDX, IDC_EDIT_HYSTERESIS_CH3, m_editHysteresis[3]);


    DDX_Control(pDX, IDC_REF_CH0, m_editRefReal[0]);
    DDX_Control(pDX, IDC_REF_CH1, m_editRefReal[1]);
    DDX_Control(pDX, IDC_REF_CH2, m_editRefReal[2]);
    DDX_Control(pDX, IDC_REF_CH3, m_editRefReal[3]);
    DDX_Control(pDX, IDC_EDIT_COMMON, m_editRefCommon);
    DDX_Control(pDX, IDC_EDIT_SLAVE_ID, m_editSlaveId);


    DDX_Control(pDX, IDC_BUTTON_STARTSTOP, m_btnStartStop);
    DDX_Control(pDX, IDC_BUTTON_APPLYINTERVAL, m_btnApplyInterval);
    DDX_Control(pDX, IDC_BUTTON_RESET, m_btnReset);
    DDX_Control(pDX, IDC_BUTTON_EXPORT_CSV, m_btnExportCSV);
    DDX_Control(pDX, IDC_BUTTON_SAVE_IMAGE, m_btnSaveImage);
    DDX_Control(pDX, IDC_BUTTON_LOG, m_btnLog);
    DDX_Control(pDX, IDC_BUTTON_HELP, m_btnHelp);
    DDX_Control(pDX, IDC_BUTTON_SIMULATION, m_btnSimulation);
    DDX_Control(pDX, IDC_CALIBRATE, m_btnCalibrate);
    DDX_Control(pDX, IDC_SAVE_POINT_1, m_btnSavePoint1);
    DDX_Control(pDX, IDC_SAVE_POINT_2, m_btnSavePoint2);


    DDX_Control(pDX, IDC_CHECK_CH0, m_chkCh0);
    DDX_Control(pDX, IDC_CHECK_CH1, m_chkCh1);
    DDX_Control(pDX, IDC_CHECK_CH2, m_chkCh2);
    DDX_Control(pDX, IDC_CHECK_CH3, m_chkCh3);
    DDX_Control(pDX, IDC_CHECK_DARKMODE, m_chkDarkMode);
    DDX_Control(pDX, IDC_CHECK_AUTOSCALE_Y, m_chkAutoScaleY);
    DDX_Control(pDX, IDC_CHECK_SHOWPEAKS, m_chkShowPeaks);


    DDX_Control(pDX, IDC_STATIC_STATUS, m_staticStatus);
}


std::vector<CString> EnumerateCOMPorts()
{
    std::vector<CString> result;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS,
        nullptr,
        nullptr,
        DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE)
        return result;

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    TCHAR buffer[256];
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i)
    {
        if (SetupDiGetDeviceRegistryProperty(
            hDevInfo,
            &devInfoData,
            SPDRP_FRIENDLYNAME,
            nullptr,
            (PBYTE)buffer,
            sizeof(buffer),
            nullptr))
        {
            CString name(buffer);
            int pos = name.Find(_T("(COM"));
            if (pos >= 0)
            {
                CString port = name.Mid(pos + 1);
                port = port.Left(port.GetLength() - 1);
                result.push_back(port);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return result;
}


BOOL CMainDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    AfxInitRichEdit2();


    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);


    m_comboComPorts.ResetContent();
    auto ports = EnumerateCOMPorts();
    for (const auto& port : ports)
    {
        m_comboComPorts.AddString(port);
    }

    if (m_comboComPorts.GetCount() > 0)
    {
        m_comboComPorts.SetCurSel(0);
        m_comboComPorts.GetLBText(0, m_selectedPort);
    }

    m_graphStatic.ModifyStyle(0, SS_NOTIFY);
    m_sliderHistory.SetRange(0, 0);
    m_sliderHistory.SetPos(m_sliderPos);

    m_warnings.SetWindowText(_T(""));
    m_alarms.SetWindowText(_T(""));

    m_alarms.SetEventMask(m_alarms.GetEventMask() | ENM_SCROLL);
    m_alarms.SetOptions(ECOOP_OR, ECO_AUTOVSCROLL | ECO_AUTOHSCROLL);
    m_alarms.SetSel(-1, -1);

    CString tmp;
    tmp.Format(_T("%.2f"), m_threshMax);         m_editThreshMax.SetWindowText(tmp);
    tmp.Format(_T("%.2f"), m_threshMin);         m_editThreshMin.SetWindowText(tmp);
    SetDlgItemInt(IDC_EDIT_INTERVAL, m_samplingIntervalMs, FALSE);

    m_editSlaveId.SetWindowText(_T("2"));


    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        tmp.Format(_T("%.2f"), m_hysteresis[ch]);
        m_editHysteresis[ch].SetWindowText(tmp);
    }


    CheckDlgButton(IDC_CHECK_CH0, BST_CHECKED);
    CheckDlgButton(IDC_CHECK_CH1, BST_CHECKED);
    CheckDlgButton(IDC_CHECK_CH2, BST_CHECKED);
    CheckDlgButton(IDC_CHECK_CH3, BST_CHECKED);
    CheckDlgButton(IDC_CHECK_DARKMODE, BST_UNCHECKED);
    CheckDlgButton(IDC_CHECK_AUTOSCALE_Y, BST_UNCHECKED);
    CheckDlgButton(IDC_CHECK_SHOWPEAKS, BST_UNCHECKED);

    CString portsMessage;
    portsMessage.Format(
        _T("Detected COM ports: %d. Select a port before pressing Start."),
        m_comboComPorts.GetCount());
    AppendToAlarms(portsMessage, RGB(0, 90, 160));
    UpdateStatusText();


    DrawGraph();
    m_graphStatic.SetFocus();


    if (m_darkMode)
    {
        SetBackgroundColor(RGB(30, 30, 30));  
    }
    UpdateDarkModeUI();  

    return TRUE;
}



void CMainDialog::OnSysCommand(UINT nID, LPARAM lParam)
{
    CDialogEx::OnSysCommand(nID, lParam);
}

void CMainDialog::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, (WPARAM)dc.GetSafeHdc(), 0);
        int cx = GetSystemMetrics(SM_CXICON), cy = GetSystemMetrics(SM_CYICON);
        CRect rect; GetClientRect(&rect);
        int x = (rect.Width() - cx + 1) / 2;
        int y = (rect.Height() - cy + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
        DrawGraph();
    }
}



void CMainDialog::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
    if (!m_darkMode)
    {
        CDialogEx::OnDrawItem(nIDCtl, lpDIS);
        return;
    }

    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    CRect rc = lpDIS->rcItem;

    pDC->FillSolidRect(&rc, RGB(30, 30, 30));
    pDC->DrawEdge(&rc, EDGE_RAISED, BF_RECT);

    CString text;
    GetDlgItem(lpDIS->CtlID)->GetWindowText(text);
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(text, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}




HCURSOR CMainDialog::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}


HBRUSH CMainDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (!m_darkMode)
        return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    const int ctrlId = pWnd->GetDlgCtrlID();

    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->SetBkColor(RGB(30, 30, 30));
    pDC->SetBkMode(TRANSPARENT);

    switch (ctrlId)
    {
        // Group boxes
    case IDC_STATIC_MENU:
    case IDC_STATIC_CH:
    case IDC_STATIC_HYSTERESIS:
    case IDC_STATIC_PORTS:
    case IDC_STATIC_POINT1:
    case IDC_STATIC_POINT2:

        
    case IDC_CHECK_CH0:
    case IDC_CHECK_CH1:
    case IDC_CHECK_CH2:
    case IDC_CHECK_CH3:
    case IDC_CHECK_DARKMODE:
    case IDC_CHECK_AUTOSCALE_Y:
    case IDC_CHECK_SHOWPEAKS:

        return m_hbrBackground;

    default:
        break;
    }

    switch (nCtlColor)
    {
    case CTLCOLOR_DLG:
    case CTLCOLOR_STATIC:
    case CTLCOLOR_EDIT:
    case CTLCOLOR_BTN:
    case CTLCOLOR_LISTBOX:
    case CTLCOLOR_MSGBOX:
    case CTLCOLOR_SCROLLBAR:
        return m_hbrBackground;
    }

    return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}




BOOL CMainDialog::OnEraseBkgnd(CDC* pDC)
{
    if (m_darkMode)
    {
        CRect rect;
        GetClientRect(&rect);
        pDC->FillSolidRect(&rect, RGB(30, 30, 30));
        return TRUE;
    }

    return CDialogEx::OnEraseBkgnd(pDC);
}



BOOL CMainDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    double factor = 1.1;
    if (GetAsyncKeyState(VK_CONTROL) < 0)
        m_zoomFactorY = std::clamp(
            m_zoomFactorY * (zDelta > 0 ? factor : 1.0 / factor),
            0.1,
            10.0);
    else
        m_zoomFactorX = std::clamp(
            m_zoomFactorX * (zDelta > 0 ? factor : 1.0 / factor),
            0.1,
            10.0);
    DrawGraph();
    return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

void CMainDialog::OnLButtonDown(UINT nFlags, CPoint pt)
{
    CRect rc; m_graphStatic.GetWindowRect(&rc);
    ScreenToClient(&rc);
    if (rc.PtInRect(pt))
    {
        m_isPanning = true;
        m_lastMousePos = pt;
        SetCapture();
    }
    CDialogEx::OnLButtonDown(nFlags, pt);
}

void CMainDialog::OnLButtonUp(UINT nFlags, CPoint pt)
{
    if (m_isPanning)
    {
        m_isPanning = false;
        ReleaseCapture();
    }
    CDialogEx::OnLButtonUp(nFlags, pt);
}


void CMainDialog::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pSB)
{
    if (pSB->GetSafeHwnd() == m_sliderHistory.GetSafeHwnd())
    {
        m_sliderPos = m_sliderHistory.GetPos();
        DrawGraph();
    }
    CDialogEx::OnHScroll(nSBCode, nPos, pSB);
}


void CMainDialog::OnBnClickedButtonStartstop()
{
    if (!m_isRunning)
    {
        if (m_asyncRequestInFlight || m_pendingAsyncRequest.has_value())
        {
            AppendToAlarms(
                _T("Wait for the calibration capture to finish before starting."),
                RGB(200, 100, 0));
            return;
        }

        if (!m_useSimulation && !ConnectSelectedPort())
        {
            return;
        }

        m_isRunning = true;
        m_sampleCount = 0;
        ResetGraph();
        m_alarmEvaluator.Reset();
        m_simulator.Reset();
        m_consecutiveCommunicationErrors = 0;
        SetTimer(m_timerId, m_samplingIntervalMs, nullptr);
        SetDlgItemText(IDC_BUTTON_STARTSTOP, _T("Stop"));
    }
    else
    {
        m_isRunning = false;
        KillTimer(m_timerId);
        StopAsyncRequest();
        SetDlgItemText(IDC_BUTTON_STARTSTOP, _T("Start"));
    }
    UpdateStatusText();
}

void CMainDialog::OnBnClickedButtonLog()
{
    if (!m_csvLogger.IsRunning())
    {
        CFileDialog dialog(
            FALSE,
            _T("csv"),
            _T("measurement-log.csv"),
            OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
            _T("CSV Files (*.csv)|*.csv||"),
            this);
        if (dialog.DoModal() != IDOK)
        {
            return;
        }

        if (!m_isRunning)
        {
            OnBnClickedButtonStartstop();
            if (!m_isRunning)
            {
                return;
            }
        }

        std::string errorMessage;
        const std::filesystem::path path(
            static_cast<LPCTSTR>(dialog.GetPathName()));
        if (!m_csvLogger.Start(path, errorMessage))
        {
            CA2T convertedError(errorMessage.c_str());
            AfxMessageBox(CString(convertedError));
            return;
        }
        SetDlgItemText(IDC_BUTTON_LOG, _T("Stop Logging"));
    }
    else
    {
        m_csvLogger.Stop();
        SetDlgItemText(IDC_BUTTON_LOG, _T("Start Logging"));
    }
}

void CMainDialog::CheckCsvLoggerError()
{
    const std::string errorMessage = m_csvLogger.TakeErrorMessage();
    if (errorMessage.empty())
    {
        return;
    }

    SetDlgItemText(IDC_BUTTON_LOG, _T("Start Logging"));
    CA2T convertedError(errorMessage.c_str());
    const CString convertedMessage(convertedError);
    CString message;
    message.Format(
        _T("CSV logging stopped: %s"),
        static_cast<LPCTSTR>(convertedMessage));
    AppendToAlarms(message, RGB(220, 0, 0));
    SetDlgItemText(IDC_STATIC_STATUS, message);
}


void CMainDialog::OnBnClickedExportCsv()
{
    if (m_history.Empty())
    {
        AfxMessageBox(_T("There are no valid measurements to export."));
        return;
    }

    CFileDialog dlg(FALSE, _T("csv"), _T("measurement-export.csv"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("CSV Files (*.csv)|*.csv||"), this);
    if (dlg.DoModal() != IDOK) return;

    std::string errorMessage;
    const std::filesystem::path path(static_cast<LPCTSTR>(dlg.GetPathName()));
    if (!CsvLogger::ExportHistory(path, m_history, errorMessage))
    {
        CA2T convertedError(errorMessage.c_str());
        AfxMessageBox(CString(convertedError));
    }
}


void CMainDialog::OnBnClickedSaveImage()
{
    CFileDialog dlg(FALSE, _T("png"), _T("graph.png"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("PNG Files (*.png)|*.png||BMP Files (*.bmp)|*.bmp||"),
        this);
    if (dlg.DoModal() != IDOK) return;

    CString path = dlg.GetPathName();
    DrawGraph(path); 
}



void CMainDialog::OnBnClickedButtonHelp()
{
    CHelpDialog dlg(this);
    dlg.DoModal();
}


void CMainDialog::OnBnClickedChannelVisibility()
{
    m_channelEnabled[0] = IsDlgButtonChecked(IDC_CHECK_CH0) == BST_CHECKED;
    m_channelEnabled[1] = IsDlgButtonChecked(IDC_CHECK_CH1) == BST_CHECKED;
    m_channelEnabled[2] = IsDlgButtonChecked(IDC_CHECK_CH2) == BST_CHECKED;
    m_channelEnabled[3] = IsDlgButtonChecked(IDC_CHECK_CH3) == BST_CHECKED;
    DrawGraph();
}

void CMainDialog::OnBnClickedCheckDarkMode()
{
    m_darkMode = (IsDlgButtonChecked(IDC_CHECK_DARKMODE) == BST_CHECKED);
    UpdateDarkModeUI();
}


void CMainDialog::UpdateDarkModeUI()
{
    
    if (m_hbrBackground)
        ::DeleteObject(m_hbrBackground);
    m_hbrBackground = ::CreateSolidBrush(m_darkMode ? RGB(30, 30, 30) : GetSysColor(COLOR_BTNFACE));

   
    SetClassLongPtr(m_hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)m_hbrBackground);

    
    for (CWnd* c = GetWindow(GW_CHILD); c; c = c->GetNextWindow())
        ::SetWindowTheme(c->GetSafeHwnd(), m_darkMode ? L"" : L"Explorer", nullptr);

    
    COLORREF bg = m_darkMode ? RGB(30, 30, 30) : GetSysColor(COLOR_WINDOW);
    COLORREF fg = m_darkMode ? RGB(255, 255, 255) : GetSysColor(COLOR_WINDOWTEXT);
    COLORREF face = m_darkMode ? RGB(30, 30, 30) : GetSysColor(COLOR_BTNFACE);

   
    m_warnings.SetBackgroundColor(FALSE, bg);
    m_alarms.SetBackgroundColor(FALSE, bg);

    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    cf.crTextColor = fg;
    cf.crBackColor = bg;

    m_warnings.SetDefaultCharFormat(cf);
    m_alarms.SetDefaultCharFormat(cf);

    
    auto themeButton = [&](CMFCButton& b) {
        b.SetFaceColor(face, TRUE);  
        b.SetTextColor(fg);          
        b.RedrawWindow();
        Invalidate(); 
        UpdateWindow();

        };


    
    themeButton(m_btnStartStop);
    themeButton(m_btnApplyInterval);
    themeButton(m_btnReset);
    themeButton(m_btnExportCSV);
    themeButton(m_btnSaveImage);
    themeButton(m_btnLog);
    themeButton(m_btnHelp);
    themeButton(m_btnSimulation);
    themeButton(m_btnCalibrate);
    themeButton(m_btnSavePoint1);  
    themeButton(m_btnSavePoint2);  

    
    for (CWnd* pWnd = GetWindow(GW_CHILD); pWnd; pWnd = pWnd->GetNextWindow()) {
        pWnd->Invalidate();
        pWnd->UpdateWindow();
    }

    RedrawWindow(nullptr, nullptr,
        RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

    DrawGraph();
}




void CMainDialog::OnBnClickedCheckAutoscaleY()
{
    m_autoScaleY = !m_autoScaleY; DrawGraph();
}
void CMainDialog::OnBnClickedCheckShowPeaks()
{
    m_showPeaks = (IsDlgButtonChecked(IDC_CHECK_SHOWPEAKS) == BST_CHECKED); DrawGraph();
}

void CMainDialog::OnEnChangeEditThreshMax()
{
    CString s; m_editThreshMax.GetWindowText(s); m_threshMax = _tstof(s);
}
void CMainDialog::OnEnChangeEditThreshMin()
{
    CString s; m_editThreshMin.GetWindowText(s); m_threshMin = _tstof(s);
}



void CMainDialog::OnBnClickedButtonApplyInterval()
{
    BOOL ok; int v = GetDlgItemInt(IDC_EDIT_INTERVAL, &ok, TRUE);
    if (ok && v >= 50 && v <= 60000)
    {
        m_samplingIntervalMs = v;
        if (m_isRunning)
        {
            KillTimer(m_timerId);
            SetTimer(m_timerId, m_samplingIntervalMs, nullptr);
        }
    }
    else
    {
        AfxMessageBox(_T("The sampling interval must be between 50 and 60000 ms."));
    }
}


void CMainDialog::OnBnClickedButtonReset()
{
    StopAsyncRequest();
    ResetGraph();
    m_calibration.Reset();
    m_alarmEvaluator.Reset();
    m_simulator.Reset();
    m_sampleCount = 0;
    m_lastCurrentMilliamp.fill(0.0);
    SetDlgItemText(IDC_STATIC_SAMPLES, _T("Samples: 0"));
    m_panOffsetX_px = 0.0;
    m_zoomFactorX = m_zoomFactorY = 1.0;
    m_sliderPos = 0;
    m_sliderHistory.SetRange(0, 0);
    m_sliderHistory.SetPos(m_sliderPos);
    m_warnings.SetWindowText(_T(""));
    m_alarms.SetWindowText(_T(""));
    DrawGraph();
}
void CMainDialog::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_timerId && m_isRunning)
    {
        if (m_asyncRequestInFlight || m_pendingAsyncRequest.has_value())
        {
            CDialogEx::OnTimer(nIDEvent);
            return;
        }

        std::array<AlarmConfiguration, kChannelCount> alarmConfiguration;
        if (!ReadAlarmConfiguration(alarmConfiguration))
        {
            CDialogEx::OnTimer(nIDEvent);
            return;
        }

        std::uint8_t slaveId = 1;
        if (!m_useSimulation && !ReadSlaveId(slaveId, false))
        {
            SetDlgItemText(IDC_STATIC_STATUS, _T("Status: Slave ID invalid (1-247)"));
            CDialogEx::OnTimer(nIDEvent);
            return;
        }

        AsyncRequest request;
        request.kind = AsyncRequestKind::Poll;
        request.useSimulation = m_useSimulation;
        request.slaveId = slaveId;
        request.alarmConfiguration = alarmConfiguration;
        BeginAsyncRequest(std::move(request));
        CDialogEx::OnTimer(nIDEvent);
        return;
    }

    CDialogEx::OnTimer(nIDEvent);
}

bool CMainDialog::BeginAsyncRequest(AsyncRequest request)
{
    if (m_asyncRequestInFlight || m_requestWorker.joinable())
    {
        return false;
    }

    ++m_nextAsyncRequestToken;
    if (m_nextAsyncRequestToken == 0)
    {
        ++m_nextAsyncRequestToken;
    }

    const std::uint32_t requestToken = m_nextAsyncRequestToken;
    const HWND targetWindow = GetSafeHwnd();
    m_activeAsyncRequestToken = requestToken;
    m_asyncRequestInFlight = true;
    m_cancelAsyncRequest.store(false);

    if (request.kind != AsyncRequestKind::Poll)
    {
        SetDlgItemText(IDC_STATIC_STATUS, _T("Status: capturing calibration samples..."));
    }

    try
    {
        m_requestWorker = std::thread(
            [this, request = std::move(request), requestToken, targetWindow]() mutable
            {
                AsyncResult result;
                try
                {
                    result = ExecuteAsyncRequest(request);
                }
                catch (const std::exception&)
                {
                    result.kind = request.kind;
                    result.alarmConfiguration = request.alarmConfiguration;
                    result.references = request.references;
                    result.batch.error = CommunicationError::TransportError;
                }
                if (m_cancelAsyncRequest.load())
                {
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(m_asyncResultMutex);
                    m_completedAsyncResult = std::move(result);
                }
                ::PostMessage(
                    targetWindow,
                    kAsyncRequestCompleteMessage,
                    static_cast<WPARAM>(requestToken),
                    0);
            });
    }
    catch (const std::exception&)
    {
        m_asyncRequestInFlight = false;
        m_activeAsyncRequestToken = 0;
        SetDlgItemText(IDC_STATIC_STATUS, _T("Status: could not start acquisition worker"));
        return false;
    }
    return true;
}

CMainDialog::AsyncResult CMainDialog::ExecuteAsyncRequest(
    const AsyncRequest& request)
{
    AsyncResult result;
    result.kind = request.kind;
    result.alarmConfiguration = request.alarmConfiguration;
    result.references = request.references;

    if (!request.useSimulation)
    {
        m_acquisition.SetSlaveId(request.slaveId);
    }

    if (request.kind == AsyncRequestKind::Poll)
    {
        result.batch = request.useSimulation
            ? m_simulator.Generate()
            : m_acquisition.Poll();
        return result;
    }

    std::array<double, kChannelCount> sums{};
    MeasurementBatch sample;
    for (std::size_t sampleIndex = 0;
        sampleIndex < request.sampleCount && !m_cancelAsyncRequest.load();
        ++sampleIndex)
    {
        sample = request.useSimulation
            ? m_simulator.Generate()
            : m_acquisition.Poll();
        if (!sample.IsValid())
        {
            result.batch = sample;
            return result;
        }

        for (std::size_t channel = 0; channel < kChannelCount; ++channel)
        {
            sums[channel] += sample.channels[channel].rawMilliamp;
        }

        if (sampleIndex + 1 < request.sampleCount &&
            request.delayBetweenSamplesMs > 0)
        {
            const auto delay = std::chrono::milliseconds(
                request.delayBetweenSamplesMs);
            std::this_thread::sleep_for(delay);
        }
    }

    result.batch = sample;
    if (!m_cancelAsyncRequest.load() && request.sampleCount > 0)
    {
        for (std::size_t channel = 0; channel < kChannelCount; ++channel)
        {
            result.averageRawMilliamp[channel] =
                sums[channel] / static_cast<double>(request.sampleCount);
        }
        result.averagesValid = true;
    }
    return result;
}

void CMainDialog::StopAsyncRequest()
{
    m_pendingAsyncRequest.reset();
    m_cancelAsyncRequest.store(true);
    if (m_requestWorker.joinable())
    {
        m_requestWorker.join();
    }
    {
        std::lock_guard<std::mutex> lock(m_asyncResultMutex);
        m_completedAsyncResult.reset();
    }
    m_asyncRequestInFlight = false;
    m_activeAsyncRequestToken = 0;
}

LRESULT CMainDialog::OnAsyncRequestComplete(WPARAM requestToken, LPARAM)
{
    if (!m_asyncRequestInFlight ||
        static_cast<std::uint32_t>(requestToken) != m_activeAsyncRequestToken)
    {
        return 0;
    }

    if (m_requestWorker.joinable())
    {
        m_requestWorker.join();
    }

    std::optional<AsyncResult> result;
    {
        std::lock_guard<std::mutex> lock(m_asyncResultMutex);
        result = std::move(m_completedAsyncResult);
        m_completedAsyncResult.reset();
    }
    m_asyncRequestInFlight = false;
    m_activeAsyncRequestToken = 0;

    if (result.has_value())
    {
        if (result->kind == AsyncRequestKind::Poll)
        {
            HandleMeasurementResult(std::move(*result));
        }
        else
        {
            HandleCalibrationResult(*result);
        }
        CheckCsvLoggerError();
    }

    if (m_pendingAsyncRequest.has_value())
    {
        AsyncRequest pending = std::move(*m_pendingAsyncRequest);
        m_pendingAsyncRequest.reset();
        BeginAsyncRequest(std::move(pending));
    }
    return 0;
}

void CMainDialog::HandleMeasurementResult(AsyncResult result)
{
    MeasurementBatch& batch = result.batch;
    if (!batch.IsValid())
    {
        if (m_csvLogger.IsRunning())
        {
            m_csvLogger.Enqueue(batch);
        }
        HandleAcquisitionError(batch);
        return;
    }

    m_consecutiveCommunicationErrors = 0;
    UpdateStatusText();
    ApplyCalibration(batch);
    const int oldMaximumSlider = (std::max)(
        0,
        static_cast<int>(m_chartPoints[0].size()) - m_windowSize);
    const bool wasFollowingLatest = m_sliderPos >= oldMaximumSlider;

    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        const double value = batch.channels[channel].calibratedMilliamp;
        m_lastCurrentMilliamp[channel] = value;
        const auto previous = m_alarmEvaluator.CurrentLevel(channel);
        const auto current = m_alarmEvaluator.Update(
            channel,
            value,
            result.alarmConfiguration[channel]);
        if (current == AlarmLevel::Alarm && previous != AlarmLevel::Alarm)
        {
            const CString timestamp = CTime::GetCurrentTime().Format(
                _T("%Y-%m-%d %H:%M:%S"));
            CString message;
            message.Format(
                _T("Alarm CH%zu: %.2f mA @ %s"),
                channel + 1,
                value,
                static_cast<LPCTSTR>(timestamp));
            AppendToAlarms(message, RGB(255, 0, 0));
        }

        auto& points = m_chartPoints[channel];
        if (points.size() >= m_historySize)
        {
            points.erase(points.begin());
        }
        points.push_back(ChartPoint{ m_sampleCount, value });
    }

    m_history.Push(batch);
    if (m_csvLogger.IsRunning())
    {
        m_csvLogger.Enqueue(batch);
    }

    const int newMaximumSlider = (std::max)(
        0,
        static_cast<int>(m_chartPoints[0].size()) - m_windowSize);
    m_sliderHistory.SetRange(0, newMaximumSlider);
    if (wasFollowingLatest)
    {
        m_sliderPos = newMaximumSlider;
        m_sliderHistory.SetPos(m_sliderPos);
    }
    DrawGraph();
    ++m_sampleCount;
    CString sampleText;
    sampleText.Format(_T("Samples: %zu"), m_sampleCount);
    SetDlgItemText(IDC_STATIC_SAMPLES, sampleText);
}



void CMainDialog::ResetGraph()
{
    for (auto& buf : m_chartPoints)
    {
        buf.clear();
    }
    m_history.Clear();
    m_peaksIdx.clear();
    m_peaksCh.clear();

    if (CWnd* p = GetDlgItem(IDC_STATIC_GRAPH))
    {
        p->Invalidate(FALSE);
        p->UpdateWindow();
    }
}


void CMainDialog::ComputePeaks()
{
    m_peaksIdx.clear();
    m_peaksCh.clear();

    auto detect = [&](int ch, const std::vector<ChartPoint>& buf){
        if (!IsChannelEnabled(static_cast<std::size_t>(ch)))
            return;

        for (size_t i = 1; i + 1 < buf.size(); ++i)
        {
            const double y0 = buf[i - 1].milliamp;
            const double y1 = buf[i].milliamp;
            const double y2 = buf[i + 1].milliamp;
            if ((y1 >= y0 && y1 >= y2) && (y1 > y0 || y1 > y2)) {
                m_peaksIdx.push_back((int)i);
                m_peaksCh.push_back(ch);
            }
        }
    };

    for (int ch = 0; ch < kNumChannels; ++ch)
        detect(ch, m_chartPoints[ch]);
}


void CMainDialog::DrawGraph(const CString& savePath)
{
    CWnd* pg = GetDlgItem(IDC_STATIC_GRAPH);
    if (!pg) return;


    if (m_showPeaks)
        ComputePeaks();


    CClientDC dc(pg);
    CRect r; pg->GetClientRect(&r);
    CDC mdc; mdc.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, r.Width(), r.Height());
    CBitmap* oldBmp = mdc.SelectObject(&bmp);


    COLORREF bg = m_darkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
    mdc.FillSolidRect(r, bg);
    mdc.SetTextColor(m_darkMode ? RGB(220, 220, 220) : RGB(0, 0, 0));
    mdc.SetBkMode(TRANSPARENT);


    const int ml = 40, mr = 10, mt = 10, mb = 25;
    int ox = ml, oy = r.Height() - mb;
    int plotW = r.Width() - ml - mr, plotH = r.Height() - mt - mb;

    double yMin = 4.0, yMax = 20.0;
    if (m_autoScaleY)
    {
        bool hasValue = false;
        yMin = (std::numeric_limits<double>::max)();
        yMax = std::numeric_limits<double>::lowest();
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            if (!IsChannelEnabled(static_cast<std::size_t>(ch)))
                continue;
            for (auto& p : m_chartPoints[ch]) {
                yMin = (std::min)(yMin, p.milliamp);
                yMax = (std::max)(yMax, p.milliamp);
                hasValue = true;
            }
        }
        if (!hasValue)
        {
            yMin = 4.0;
            yMax = 20.0;
        }
        else if (yMin >= yMax)
        {
            yMin -= 0.5;
            yMax += 0.5;
        }
    }
    double yRange = yMax - yMin;
    double xScale = (plotW / (double)m_windowSize) * m_zoomFactorX;
    double yScale = (plotH / yRange) * m_zoomFactorY;


    COLORREF gc = m_darkMode ? RGB(80, 80, 80) : RGB(220, 220, 220);
    CPen gridPen(PS_DOT, 1, gc), * oldPen;
    for (int i = 0; i <= 10; ++i)
    {
        int y = oy - int(i * (yRange / 10.0) * yScale);
        CString lbl; lbl.Format(_T("%.1f mA"), yMin + i * (yRange / 10.0));
        mdc.TextOutW(5, y - 7, lbl);
        oldPen = mdc.SelectObject(&gridPen);
        mdc.MoveTo(ox, y); mdc.LineTo(r.Width() - mr, y);
        mdc.SelectObject(oldPen);
    }
    for (int i = 0; i <= m_windowSize; i += 20)
    {
        int x = ox + int(i * xScale);
        CString lbl; lbl.Format(_T("%d"), i);
        mdc.TextOutW(x - 10, oy + 5, lbl);
        oldPen = mdc.SelectObject(&gridPen);
        mdc.MoveTo(x, oy); mdc.LineTo(x, mt);
        mdc.SelectObject(oldPen);
    }


    int savedDC = mdc.SaveDC();
    CRgn clip; clip.CreateRectRgn(ox, mt, r.Width() - mr, oy);
    mdc.SelectClipRgn(&clip);


    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        if (!IsChannelEnabled(static_cast<std::size_t>(ch))) continue;
        const AlarmLevel lvl = m_alarmEvaluator.CurrentLevel(ch);
        CPen tracePen(PS_SOLID, 2, GetColorForLevel(ch, lvl));
        oldPen = mdc.SelectObject(&tracePen);
        if (!oldPen) continue;

        auto& buf = m_chartPoints[ch];
        int cnt = (int)buf.size();
        if (cnt > 0)
        {
            const int maximumBase = (std::max)(0, cnt - m_windowSize);
            const int base = std::clamp(m_sliderPos, 0, maximumBase);
            const std::size_t s0 = buf[base].sampleIndex;
            for (int i = base; i < (std::min)(cnt, base + m_windowSize); ++i)
            {
                const auto dx = buf[i].sampleIndex - s0;
                int xx = ox + int(static_cast<double>(dx) * xScale) + int(m_panOffsetX_px);
                int yy = oy - int((buf[i].milliamp - yMin) * yScale);
                if (i == base) mdc.MoveTo(xx, yy);
                else         mdc.LineTo(xx, yy);
            }
        }
        mdc.SelectObject(oldPen);
    }


    if (m_showPeaks)
    {
        std::array<int, kChannelCount> base{};
        std::array<std::size_t, kChannelCount> s0{};
        for (int ch = 0; ch < kNumChannels; ++ch)
        {
            int cnt = (int)m_chartPoints[ch].size();
            const int maximumBase = (std::max)(0, cnt - m_windowSize);
            base[ch] = std::clamp(m_sliderPos, 0, maximumBase);
            s0[ch] = cnt > 0 ? m_chartPoints[ch][base[ch]].sampleIndex : 0;
        }

        for (size_t i = 0; i < m_peaksIdx.size(); ++i)
        {

            int ch = m_peaksCh[i];
            if (!IsChannelEnabled(static_cast<std::size_t>(ch))) continue;


            int idx = m_peaksIdx[i];
            if (idx < base[ch] || idx >= base[ch] + m_windowSize) continue;


            CPen peakPen(
                PS_SOLID,
                2,
                GetColorForLevel(ch, m_alarmEvaluator.CurrentLevel(ch)));
            CPen* oldPen = mdc.SelectObject(&peakPen);
            if (!oldPen) continue;

            const auto dx = m_chartPoints[ch][idx].sampleIndex - s0[ch];
            int xx = ox + int(static_cast<double>(dx) * xScale) + int(m_panOffsetX_px);
            int yy = oy - int((m_chartPoints[ch][idx].milliamp - yMin) * yScale);
            mdc.Ellipse(xx - 4, yy - 4, xx + 4, yy + 4);

            mdc.SelectObject(oldPen);
        }

        if (m_darkMode)
            mdc.SetBkColor(RGB(30, 30, 30));
        else
            mdc.SetBkColor(RGB(255, 255, 255));

        if (m_darkMode)
            mdc.SetTextColor(RGB(220, 220, 220));
        else
            mdc.SetTextColor(RGB(0, 0, 0));

    }

    mdc.RestoreDC(savedDC);
    dc.BitBlt(0, 0, r.Width(), r.Height(), &mdc, 0, 0, SRCCOPY);
    mdc.SelectObject(oldBmp);

    if (!savePath.IsEmpty())
    {
        CImage img;
        img.Attach((HBITMAP)bmp.Detach());
        img.Save(savePath);
        img.Destroy();
    }


    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        CString itxt, atxt;

        double current = m_lastCurrentMilliamp[ch];
        itxt.Format(_T("Current CH%d: %.2f mA"), ch + 1, current);

        double sum = 0;
        for (const auto& p : m_chartPoints[ch]) sum += p.milliamp;
        double avg = m_chartPoints[ch].empty() ? 0.0 : (sum / m_chartPoints[ch].size());
        atxt.Format(_T("Average CH%d: %.2f mA"), ch + 1, avg);

        m_staticCurrent[ch].SetWindowText(itxt);
        m_staticAverage[ch].SetWindowText(atxt);
    }


    if (m_showPeaks)
    {
       
        int scrollPos = m_warnings.GetScrollPos(SB_VERT);       
        m_warnings.SetRedraw(FALSE);      
        m_warnings.SetWindowText(_T(""));
        size_t count = 1;

        for (size_t i = 0; i < m_peaksIdx.size(); ++i)
        {
            int ch = m_peaksCh[i];
            if (!IsChannelEnabled(static_cast<std::size_t>(ch)))
                continue;

            int idx = m_peaksIdx[i];
            double v = m_chartPoints[ch][idx].milliamp;

            CString line;
            line.Format(_T("Peak %zu (CH%d): %.2f mA\r\n"), count++, ch + 1, v);
            m_warnings.ReplaceSel(line);
        }

        // Restore the scroll position and redraw.
        m_warnings.SetScrollPos(SB_VERT, scrollPos);
        m_warnings.SetRedraw(TRUE);
        m_warnings.Invalidate();
    }
    else
    {
        m_warnings.SetWindowText(_T(""));
    }


}


void CMainDialog::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_isPanning)
    {
        int dx = point.x - m_lastMousePos.x;
        m_panOffsetX_px += dx;
        m_lastMousePos = point;
        DrawGraph();
    }
    else
    {
        CRect rc;
        m_graphStatic.GetWindowRect(&rc);
        ScreenToClient(&rc);
        if (rc.PtInRect(point) && !m_chartPoints[0].empty())
        {

            const int ml = 40, mr = 10;
            int plotW = rc.Width() - ml - mr;
            double xScale = (plotW / (double)m_windowSize) * m_zoomFactorX;

            int avail = (std::max)(0, (int)m_chartPoints[0].size() - m_windowSize);
            int base = std::clamp(m_sliderPos, 0, avail);
            int dxPts = point.x - (rc.left + ml) - int(m_panOffsetX_px);
            int idx = base + int(dxPts / xScale + 0.5);

            if (idx >= 0 && idx < (int)m_chartPoints[0].size())
            {
                auto it = std::find(m_peaksIdx.begin(), m_peaksIdx.end(), idx);
                CString msg;
                if (m_showPeaks && it != m_peaksIdx.end())
                {
                    const auto peakPosition = static_cast<std::size_t>(
                        it - m_peaksIdx.begin());
                    const int peakChannel = m_peaksCh[peakPosition];
                    const double peakMilliamp =
                        m_chartPoints[peakChannel][idx].milliamp;
                    msg.Format(
                        _T("Peak %zu (CH%d): %.2f mA"),
                        peakPosition + 1,
                        peakChannel + 1,
                        peakMilliamp);
                }
                else
                {
                    double v1 = m_chartPoints[0][idx].milliamp;
                    double v2 = m_chartPoints[1][idx].milliamp;
                    double v3 = m_chartPoints[2][idx].milliamp;
                    double v4 = m_chartPoints[3][idx].milliamp;
                    msg.Format(_T("Sample %d: C1=%.2f mA C2=%.2f mA C3=%.2f mA C4=%.2f mA"),
                        idx, v1, v2, v3, v4);
                }
                m_warnings.SetWindowText(msg);
            }
            else
            {
                m_warnings.SetWindowText(_T(""));
            }
        }
        else
        {
            m_warnings.SetWindowText(_T(""));
        }
    }

    CDialogEx::OnMouseMove(nFlags, point);
}

void CMainDialog::OnBnClickedButtonSimulation()
{
    StopAsyncRequest();
    m_useSimulation = !m_useSimulation;
    if (m_useSimulation)
    {
        m_acquisition.Disconnect();
        m_simulator.Reset();
    }
    else if (m_isRunning && !ConnectSelectedPort())
    {
        m_useSimulation = true;
    }

    CString label = m_useSimulation ? _T("Stop Simulation") : _T("Simulation");
    SetDlgItemText(IDC_BUTTON_SIMULATION, label);
    UpdateStatusText();
}

void CMainDialog::OnCbnSelchangeComPort()
{
    const int selection = m_comboComPorts.GetCurSel();
    if (selection == CB_ERR)
    {
        return;
    }

    StopAsyncRequest();
    m_comboComPorts.GetLBText(selection, m_selectedPort);
    m_acquisition.Disconnect();
    if (m_isRunning && !m_useSimulation && !ConnectSelectedPort())
    {
        m_isRunning = false;
        KillTimer(m_timerId);
        SetDlgItemText(IDC_BUTTON_STARTSTOP, _T("Start"));
    }
    UpdateStatusText();
}

void CMainDialog::QueueCalibrationCapture(
    AsyncRequestKind kind,
    const std::array<std::optional<double>, kChannelCount>& references,
    std::size_t sampleCount,
    unsigned int delayBetweenSamplesMs)
{
    if (!m_useSimulation && !m_asyncRequestInFlight &&
        !m_acquisition.IsConnected() && !ConnectSelectedPort())
    {
        return;
    }

    std::uint8_t slaveId = 1;
    if (!m_useSimulation && !ReadSlaveId(slaveId))
    {
        return;
    }

    AsyncRequest request;
    request.kind = kind;
    request.useSimulation = m_useSimulation;
    request.slaveId = slaveId;
    request.sampleCount = sampleCount;
    request.delayBetweenSamplesMs = delayBetweenSamplesMs;
    request.references = references;

    if (m_asyncRequestInFlight)
    {
        if (m_pendingAsyncRequest.has_value())
        {
            AppendToAlarms(
                _T("A calibration capture is already queued."),
                RGB(200, 100, 0));
            return;
        }
        m_pendingAsyncRequest = std::move(request);
        SetDlgItemText(IDC_STATIC_STATUS, _T("Status: calibration capture queued..."));
        return;
    }

    BeginAsyncRequest(std::move(request));
}

void CMainDialog::HandleCalibrationResult(const AsyncResult& result)
{
    if (!result.averagesValid || !result.batch.IsValid())
    {
        AppendToAlarms(CommunicationErrorText(result.batch), RGB(220, 0, 0));
        UpdateStatusText();
        return;
    }

    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        if (!result.references[channel].has_value())
        {
            continue;
        }

        const double raw = result.averageRawMilliamp[channel];
        const double reference = *result.references[channel];
        if (result.kind == AsyncRequestKind::CaptureCalibrationPoint1)
        {
            m_calibration.SetFirstPoint(channel, raw, reference);
        }
        else
        {
            m_calibration.SetSecondPoint(channel, raw, reference);
        }

        CString message;
        message.Format(
            _T("CH%zu: calibration point saved - raw=%.3f mA, reference=%.3f mA"),
            channel + 1,
            raw,
            reference);
        AppendToAlarms(
            message,
            result.kind == AsyncRequestKind::CaptureCalibrationPoint1
                ? RGB(0, 0, 160)
                : RGB(0, 128, 0));
    }
    UpdateStatusText();
}

void CMainDialog::OnBnClickedSavePoint2()
{
    CString s;
    m_editRefCommon.GetWindowText(s);
    s.Trim();
    TCHAR* end = nullptr;
    const double realVal = _tcstod(s, &end);
    if (s.IsEmpty() || end == static_cast<LPCTSTR>(s) || *end != _T('\0') ||
        !std::isfinite(realVal) || realVal < 0.0 || realVal > 25.0)
    {
        AfxMessageBox(_T("Enter a common reference between 0 and 25 mA."));
        return;
    }

    std::array<std::optional<double>, kChannelCount> references;
    for (auto& reference : references)
    {
        reference = realVal;
    }
    QueueCalibrationCapture(
        AsyncRequestKind::CaptureCalibrationPoint2,
        references);
}


void CMainDialog::OnBnClickedSavePoint1()
{
    std::array<std::optional<double>, kChannelCount> references;
    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        CString s;
        m_editRefReal[ch].GetWindowText(s);
        s.Trim();
        if (s.IsEmpty()) continue;

        TCHAR* end = nullptr;
        const double realVal = _tcstod(s, &end);
        if (end == static_cast<LPCTSTR>(s) || *end != _T('\0') ||
            !std::isfinite(realVal) || realVal < 0.0 || realVal > 25.0)
        {
            CString error;
            error.Format(_T("CH%d: the reference must be between 0 and 25 mA."), ch + 1);
            AppendToAlarms(error, RGB(220, 0, 0));
            continue;
        }

        references[ch] = realVal;
    }

    const bool hasReference = std::any_of(
        references.begin(),
        references.end(),
        [](const auto& reference) { return reference.has_value(); });
    if (hasReference)
    {
        QueueCalibrationCapture(
            AsyncRequestKind::CaptureCalibrationPoint1,
            references);
    }
}



void CMainDialog::OnBnClickedCalibrate()
{
    if (m_asyncRequestInFlight || m_pendingAsyncRequest.has_value())
    {
        AppendToAlarms(
            _T("Wait for the sample capture to finish before calculating calibration."),
            RGB(200, 100, 0));
        return;
    }

    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        if (!m_calibration.Calculate(ch))
        {
            CString err;
            err.Format(
                _T("CH%d: calibration requires two valid, distinct points."),
                ch + 1);
            AppendToAlarms(err, RGB(200, 0, 0));
            continue;
        }

        const double slope = m_calibration.Slope(ch);
        const double offset = m_calibration.Offset(ch);
        if (slope <= 0.0 || slope > 100.0 || std::abs(offset) > 100.0)
        {
            CString err;
            err.Format(
                _T("CH%d: coefficients are outside the allowed range - slope=%.4f, offset=%.4f"),
                ch + 1,
                slope,
                offset);
            m_calibration.Reset(ch);
            AppendToAlarms(err, RGB(255, 0, 0));
            continue;
        }

        CString msg;
        msg.Format(
            _T("CH%d calibrated: slope=%.4f, offset=%.4f mA"),
            ch + 1,
            slope,
            offset);
        AppendToAlarms(msg, RGB(0, 128, 0));
    }

    DrawGraph();
}

bool CMainDialog::ConnectSelectedPort()
{
    const int selection = m_comboComPorts.GetCurSel();
    if (selection == CB_ERR)
    {
        AfxMessageBox(_T("No COM port is available. Enable simulation or connect the serial adapter."));
        return false;
    }

    std::uint8_t slaveId = 0;
    if (!ReadSlaveId(slaveId))
    {
        return false;
    }

    m_comboComPorts.GetLBText(selection, m_selectedPort);
    CT2A asciiPort(m_selectedPort);
    std::string errorMessage;
    if (!m_acquisition.Connect(
        std::string(asciiPort),
        9600,
        slaveId,
        errorMessage))
    {
        CA2T convertedError(errorMessage.c_str());
        const CString convertedMessage(convertedError);
        CString message;
        message.Format(
            _T("Connection to %s failed: %s"),
            static_cast<LPCTSTR>(m_selectedPort),
            static_cast<LPCTSTR>(convertedMessage));
        AppendToAlarms(message, RGB(220, 0, 0));
        AfxMessageBox(message);
        return false;
    }

    CString message;
    message.Format(
        _T("Connected to %s, 9600 8-N-1, slave %u."),
        static_cast<LPCTSTR>(m_selectedPort),
        static_cast<unsigned int>(slaveId));
    AppendToAlarms(message, RGB(0, 128, 0));
    return true;
}

bool CMainDialog::ReadSlaveId(std::uint8_t& slaveId, bool showMessage)
{
    CString text;
    m_editSlaveId.GetWindowText(text);
    text.Trim();
    TCHAR* end = nullptr;
    const long value = _tcstol(text, &end, 10);
    const bool valid = !text.IsEmpty() &&
        end != static_cast<LPCTSTR>(text) &&
        *end == _T('\0') &&
        value >= 1 && value <= 247;
    if (!valid)
    {
        if (showMessage)
        {
            AfxMessageBox(_T("Slave ID must be a number between 1 and 247."));
        }
        return false;
    }
    slaveId = static_cast<std::uint8_t>(value);
    return true;
}

bool CMainDialog::ReadAlarmConfiguration(
    std::array<AlarmConfiguration, kChannelCount>& configuration)
{
    CString minimumText;
    CString maximumText;
    m_editThreshMin.GetWindowText(minimumText);
    m_editThreshMax.GetWindowText(maximumText);
    minimumText.Trim();
    maximumText.Trim();

    TCHAR* minimumEnd = nullptr;
    TCHAR* maximumEnd = nullptr;
    const double minimum = _tcstod(minimumText, &minimumEnd);
    const double maximum = _tcstod(maximumText, &maximumEnd);
    if (minimumText.IsEmpty() || maximumText.IsEmpty() ||
        minimumEnd == static_cast<LPCTSTR>(minimumText) ||
        maximumEnd == static_cast<LPCTSTR>(maximumText) ||
        *minimumEnd != _T('\0') || *maximumEnd != _T('\0') ||
        !std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum)
    {
        SetDlgItemText(IDC_STATIC_STATUS, _T("Status: invalid minimum/maximum thresholds"));
        return false;
    }

    const double range = maximum - minimum;
    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        CString hysteresisText;
        m_editHysteresis[channel].GetWindowText(hysteresisText);
        hysteresisText.Trim();
        TCHAR* hysteresisEnd = nullptr;
        const double hysteresis = _tcstod(hysteresisText, &hysteresisEnd);
        if (hysteresisText.IsEmpty() ||
            hysteresisEnd == static_cast<LPCTSTR>(hysteresisText) ||
            *hysteresisEnd != _T('\0') ||
            !std::isfinite(hysteresis) || hysteresis < 0.0 || hysteresis >= range / 2.0)
        {
            SetDlgItemText(IDC_STATIC_STATUS, _T("Status: invalid hysteresis"));
            return false;
        }
        m_hysteresis[channel] = hysteresis;
        configuration[channel] = AlarmConfiguration{
            minimum,
            maximum,
            hysteresis,
            0.1
        };
    }

    m_threshMin = minimum;
    m_threshMax = maximum;
    return true;
}

void CMainDialog::ApplyCalibration(MeasurementBatch& batch) const
{
    for (std::size_t channel = 0; channel < kChannelCount; ++channel)
    {
        if (!batch.channels[channel].valid)
        {
            continue;
        }
        batch.channels[channel].calibratedMilliamp = m_calibration.Apply(
            channel,
            batch.channels[channel].rawMilliamp);
    }
}

void CMainDialog::HandleAcquisitionError(const MeasurementBatch& batch)
{
    ++m_consecutiveCommunicationErrors;
    const CString errorText = CommunicationErrorText(batch);
    CString status;
    status.Format(_T("Status: communication error - %s"), static_cast<LPCTSTR>(errorText));
    SetDlgItemText(IDC_STATIC_STATUS, status);

    if (m_consecutiveCommunicationErrors == 1 ||
        m_consecutiveCommunicationErrors % 10 == 0)
    {
        AppendToAlarms(errorText, RGB(220, 0, 0));
    }
}

void CMainDialog::UpdateStatusText()
{
    CString status;
    if (m_isRunning)
    {
        if (m_useSimulation)
        {
            status = _T("Status: running in simulation mode");
        }
        else
        {
            status.Format(
                _T("Status: running Modbus on %s"),
                static_cast<LPCTSTR>(m_selectedPort));
        }
    }
    else
    {
        status = m_useSimulation
            ? _T("Status: stopped - simulation selected")
            : _T("Status: stopped - select a port and press Start");
    }
    SetDlgItemText(IDC_STATIC_STATUS, status);
}

bool CMainDialog::IsChannelEnabled(std::size_t channel) const noexcept
{
    return channel < kChannelCount && m_channelEnabled[channel];
}

CString CMainDialog::CommunicationErrorText(const MeasurementBatch& batch)
{
    switch (batch.error)
    {
    case CommunicationError::Timeout:
        return _T("Timeout: the module did not return a complete frame.");
    case CommunicationError::CrcMismatch:
        return _T("CRC mismatch: the Modbus frame was rejected.");
    case CommunicationError::InvalidSlave:
        return _T("Unexpected Slave ID: the Modbus frame was rejected.");
    case CommunicationError::InvalidFunction:
        return _T("Unexpected Modbus function code.");
    case CommunicationError::InvalidByteCount:
        return _T("Invalid Modbus byte count.");
    case CommunicationError::ModbusException:
    {
        CString message;
        message.Format(
            _T("Modbus exception 0x%02X."),
            static_cast<unsigned int>(batch.exceptionCode));
        return message;
    }
    case CommunicationError::PortClosed:
        return _T("The serial port is closed.");
    case CommunicationError::TransportError:
        return _T("Serial transport error.");
    case CommunicationError::None:
    default:
        return _T("Unknown communication error.");
    }
}

void CMainDialog::AppendToAlarms(const CString& msg, COLORREF color)
{
    m_alarms.SetSel(-1, -1);
    CHARFORMAT cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    m_alarms.SetSelectionCharFormat(cf);
    m_alarms.ReplaceSel(msg + _T("\r\n"));
    m_alarms.LineScroll(m_alarms.GetLineCount());
}
