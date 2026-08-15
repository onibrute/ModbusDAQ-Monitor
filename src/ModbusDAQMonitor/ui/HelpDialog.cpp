#include "pch.h"
#include "HelpDialog.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(CHelpDialog, CDialogEx)

CHelpDialog::CHelpDialog(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_HELP_DIALOG, pParent)
{
}

CHelpDialog::~CHelpDialog()
{
}

void CHelpDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_RICH_HELP_OVERVIEW, m_richHelp);
}

BEGIN_MESSAGE_MAP(CHelpDialog, CDialogEx)
    ON_NOTIFY(EN_LINK, IDC_RICH_HELP_OVERVIEW, &CHelpDialog::OnRichHelpLinkClick)
END_MESSAGE_MAP()

BOOL CHelpDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_richHelp.SetEventMask(m_richHelp.GetEventMask() | ENM_LINK);
    m_richHelp.SetOptions(ECOOP_OR, ECO_READONLY | ECO_AUTOVSCROLL);
    SetHelpText();

    return TRUE;
}

void CHelpDialog::SetHelpText()
{
    CString text =
        _T("- Start/Stop starts or stops data acquisition. Start details\n")
        _T("- Apply Interval changes the sampling period. Interval details\n")
        _T("- Reset clears the accumulated data and session state. Reset details\n")
        _T("- Export CSV saves the measurement history to a file. CSV details\n")
        _T("- Save Image exports the current chart as an image. Image details\n")
        _T("- Logging enables continuous CSV recording. Logging details\n")
        _T("- Channels select which traces are visible. Channel details\n")
        _T("- Auto-scale adjusts the Y-axis to recent values. Auto-scale details\n")
        _T("- Thresholds and peaks configure alarms and peak display. Peak details\n")
        _T("- Calibration corrects raw channel measurements. Calibration details\n");

    m_richHelp.SetWindowText(text);

    const CString tags[] = {
        _T("Start details"),
        _T("Interval details"),
        _T("Reset details"),
        _T("CSV details"),
        _T("Image details"),
        _T("Logging details"),
        _T("Channel details"),
        _T("Auto-scale details"),
        _T("Peak details"),
        _T("Calibration details")
    };

    for (const auto& tag : tags)
    {
        FINDTEXTEX findText = {};
        findText.chrg.cpMin = 0;
        findText.chrg.cpMax = -1;
        findText.lpstrText = const_cast<LPTSTR>(static_cast<LPCTSTR>(tag));

        if (m_richHelp.FindText(FR_DOWN, &findText) >= 0)
        {
            m_richHelp.SetSel(findText.chrgText);

            CHARFORMAT2 format = {};
            format.cbSize = sizeof(format);
            format.dwMask = CFM_COLOR | CFM_UNDERLINE | CFM_LINK;
            format.dwEffects = CFE_UNDERLINE | CFE_LINK;
            format.crTextColor = RGB(0, 0, 255);

            m_richHelp.SetSelectionCharFormat(format);
        }
    }

    m_richHelp.SetSel(-1, -1);
}

CHelpDialog::HelpTopic CHelpDialog::GetHelpTopicFromText(const CString& text)
{
    static const std::map<CString, HelpTopic> topicMap = {
        { _T("Start details"), HT_Start },
        { _T("Interval details"), HT_Interval },
        { _T("Reset details"), HT_Reset },
        { _T("CSV details"), HT_CSV },
        { _T("Image details"), HT_Image },
        { _T("Logging details"), HT_Log },
        { _T("Channel details"), HT_Checkbox },
        { _T("Auto-scale details"), HT_Auto },
        { _T("Peak details"), HT_Peak },
        { _T("Calibration details"), HT_Calibration }
    };

    const auto topic = topicMap.find(text);
    return topic != topicMap.end() ? topic->second : HT_None;
}

void CHelpDialog::OnRichHelpLinkClick(NMHDR* notification, LRESULT* result)
{
    ENLINK* link = reinterpret_cast<ENLINK*>(notification);
    if (link->msg == WM_LBUTTONDOWN)
    {
        CString clickedText;
        m_richHelp.GetTextRange(link->chrg.cpMin, link->chrg.cpMax, clickedText);

        switch (GetHelpTopicFromText(clickedText))
        {
        case HT_Start:
            ::MessageBoxW(
                m_hWnd,
                L"Start/Stop\n\n"
                L"Starts or stops periodic data acquisition.\n\n"
                L"- Start reads data from the selected serial port, or from the simulator, and updates the chart.\n"
                L"- Acquisition runs in the background so the interface remains responsive.\n"
                L"- Stop ends polling while leaving the collected data available for review and export.",
                L"Start/Stop",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Interval:
            ::MessageBoxW(
                m_hWnd,
                L"Apply Interval\n\n"
                L"Changes the sampling period, in milliseconds, between successive acquisition requests.\n\n"
                L"- A shorter period updates the display more frequently.\n"
                L"- A longer period reduces the polling rate.\n"
                L"- Accepted values are between 50 and 60000 ms.\n\n"
                L"The new period takes effect when you select Apply Interval.",
                L"Apply Interval",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Reset:
            ::MessageBoxW(
                m_hWnd,
                L"Reset\n\n"
                L"Clears the chart and measurement history and resets the sample counter.\n\n"
                L"It also resets calibration points, alarm states, zoom and pan settings, and the simulator state.",
                L"Reset",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_CSV:
            ::MessageBoxW(
                m_hWnd,
                L"Export CSV\n\n"
                L"Exports the collected measurement history to a CSV file compatible with spreadsheet and analysis tools.\n\n"
                L"The file contains timestamps and the values recorded for all four channels and is saved to the location you select.",
                L"Export CSV",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Image:
            ::MessageBoxW(
                m_hWnd,
                L"Save Image\n\n"
                L"Saves the current chart as a PNG or BMP image.\n\n"
                L"The exported image reflects the visible traces, scale, zoom, and current chart view.",
                L"Save Image",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Log:
            ::MessageBoxW(
                m_hWnd,
                L"Continuous Logging\n\n"
                L"Continuously records acquisition results to a CSV file.\n\n"
                L"- Samples are queued and written by a separate worker.\n"
                L"- Communication errors are recorded explicitly instead of being stored as 0 mA.\n"
                L"- Use this mode for longer monitoring sessions and traceable data capture.",
                L"Continuous Logging",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Checkbox:
            ::MessageBoxW(
                m_hWnd,
                L"Channels (CH1-CH4)\n\n"
                L"Shows or hides individual channel traces.\n\n"
                L"Hiding a channel affects only the chart. Acquisition and measurement history continue for every channel.",
                L"Channels",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Auto:
            ::MessageBoxW(
                m_hWnd,
                L"Auto-scale Y-axis\n\n"
                L"Automatically adjusts the chart's vertical scale to include the recent visible values.\n\n"
                L"When disabled, the chart uses the configured fixed range together with the current zoom settings.",
                L"Auto-scale Y-axis",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Peak:
            ::MessageBoxW(
                m_hWnd,
                L"Thresholds and Peaks\n\n"
                L"Minimum and maximum thresholds define the normal measurement range. Hysteresis prevents rapid alarm state changes near a threshold.\n\n"
                L"Show Peaks highlights local maxima in the visible channel traces. Peak detection and threshold alarms are separate features.",
                L"Thresholds and Peaks",
                MB_OK | MB_ICONINFORMATION);
            break;

        case HT_Calibration:
            ::MessageBoxW(
                m_hWnd,
                L"Two-point Calibration\n\n"
                L"1. Apply a stable first input, enter the measured reference directly in mA for each channel, and select Save Point 1.\n"
                L"2. Apply a different stable input, enter the common reference directly in mA, and select Save Point 2.\n"
                L"3. Select Calibrate to calculate a slope and offset for each channel.\n\n"
                L"Each saved point averages 10 shared acquisition samples across all four channels.\n\n"
                L"calibrated_value = slope x raw_value + offset\n\n"
                L"Calibration is applied once before display, alarm evaluation, and logging.",
                L"Two-point Calibration",
                MB_OK | MB_ICONINFORMATION);
            break;

        default:
            break;
        }
    }
    *result = 0;
}
