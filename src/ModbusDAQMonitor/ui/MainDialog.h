#pragma once

#include "AcquisitionService.h"
#include "AlarmEvaluator.h"
#include "CalibrationModel.h"
#include "CsvLogger.h"
#include "MeasurementHistory.h"
#include "SignalSimulator.h"
#include "resource.h"

#include <afxcmn.h>
#include <afxrich.h>
#include <afxwin.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

struct ChartPoint
{
    std::size_t sampleIndex = 0;
    double milliamp = 0.0;
};

COLORREF GetColorForLevel(int channel, AlarmLevel level);

class CMainDialog : public CDialogEx
{
public:
    explicit CMainDialog(CWnd* parent = nullptr);
    ~CMainDialog() override;

    enum { IDD = IDD_MODBUS_DAQ_MONITOR_DIALOG };

protected:
    void DoDataExchange(CDataExchange* dataExchange) override;
    BOOL OnInitDialog() override;
    DECLARE_MESSAGE_MAP()

private:
    static constexpr int kNumChannels = static_cast<int>(kChannelCount);

    enum class AsyncRequestKind
    {
        Poll,
        CaptureCalibrationPoint1,
        CaptureCalibrationPoint2
    };

    struct AsyncRequest
    {
        AsyncRequestKind kind = AsyncRequestKind::Poll;
        bool useSimulation = false;
        std::uint8_t slaveId = 1;
        std::size_t sampleCount = 1;
        unsigned int delayBetweenSamplesMs = 0;
        std::array<AlarmConfiguration, kChannelCount> alarmConfiguration{};
        std::array<std::optional<double>, kChannelCount> references{};
    };

    struct AsyncResult
    {
        AsyncRequestKind kind = AsyncRequestKind::Poll;
        MeasurementBatch batch;
        std::array<AlarmConfiguration, kChannelCount> alarmConfiguration{};
        std::array<double, kChannelCount> averageRawMilliamp{};
        std::array<std::optional<double>, kChannelCount> references{};
        bool averagesValid = false;
    };

    HICON m_hIcon = nullptr;
    HBRUSH m_hbrBackground = nullptr;

    CStatic m_graphStatic;
    CSliderCtrl m_sliderHistory;
    CRichEditCtrl m_warnings;
    CRichEditCtrl m_alarms;
    CEdit m_editThreshMin;
    CEdit m_editThreshMax;
    std::array<CStatic, kChannelCount> m_staticCurrent;
    std::array<CStatic, kChannelCount> m_staticAverage;
    std::array<CStatic, kChannelCount> m_staticHystLabel;
    std::array<CEdit, kChannelCount> m_editHysteresis;
    CEdit m_editRefCommon;
    std::array<CEdit, kChannelCount> m_editRefReal;
    CComboBox m_comboComPorts;
    CString m_selectedPort;
    CEdit m_editSlaveId;

    CMFCButton m_btnStartStop;
    CMFCButton m_btnApplyInterval;
    CMFCButton m_btnReset;
    CMFCButton m_btnExportCSV;
    CMFCButton m_btnSaveImage;
    CMFCButton m_btnLog;
    CMFCButton m_btnHelp;
    CMFCButton m_btnSimulation;
    CMFCButton m_btnCalibrate;
    CMFCButton m_btnSavePoint1;
    CMFCButton m_btnSavePoint2;

    CButton m_chkShowPeaks;
    CButton m_chkAutoScaleY;
    CButton m_chkCh0;
    CButton m_chkCh1;
    CButton m_chkCh2;
    CButton m_chkCh3;
    CButton m_chkDarkMode;
    CStatic m_staticStatus;

    std::array<std::vector<ChartPoint>, kChannelCount> m_chartPoints;
    std::vector<int> m_peaksIdx;
    std::vector<int> m_peaksCh;
    std::array<double, kChannelCount> m_lastCurrentMilliamp{};
    std::array<bool, kChannelCount> m_channelEnabled{ true, true, true, true };

    double m_threshMax = 20.0;
    double m_threshMin = 4.0;
    std::array<double, kChannelCount> m_hysteresis{ 0.2, 0.2, 0.2, 0.2 };

    bool m_isRunning = false;
    bool m_useSimulation = false;
    UINT_PTR m_timerId = 1;
    std::size_t m_sampleCount = 0;
    int m_samplingIntervalMs = 500;
    std::size_t m_historySize = 1000;
    int m_windowSize = 100;
    int m_sliderPos = 0;

    bool m_darkMode = false;
    bool m_autoScaleY = false;
    bool m_showPeaks = false;
    double m_zoomFactorX = 1.0;
    double m_zoomFactorY = 1.0;
    double m_panOffsetX_px = 0.0;
    bool m_isPanning = false;
    CPoint m_lastMousePos;

    AcquisitionService m_acquisition;
    CalibrationModel m_calibration;
    AlarmEvaluator m_alarmEvaluator;
    MeasurementHistory m_history{ m_historySize };
    SignalSimulator m_simulator;
    CsvLogger m_csvLogger;
    unsigned int m_consecutiveCommunicationErrors = 0;

    std::thread m_requestWorker;
    std::mutex m_asyncResultMutex;
    std::optional<AsyncResult> m_completedAsyncResult;
    std::optional<AsyncRequest> m_pendingAsyncRequest;
    std::atomic<bool> m_cancelAsyncRequest{ false };
    bool m_asyncRequestInFlight = false;
    std::uint32_t m_nextAsyncRequestToken = 0;
    std::uint32_t m_activeAsyncRequestToken = 0;

    void ResetGraph();
    void ComputePeaks();
    void DrawGraph(const CString& savePath = _T(""));
    void UpdateDarkModeUI();
    bool ConnectSelectedPort();
    bool ReadSlaveId(std::uint8_t& slaveId, bool showMessage = true);
    bool ReadAlarmConfiguration(std::array<AlarmConfiguration, kChannelCount>& configuration);
    void ApplyCalibration(MeasurementBatch& batch) const;
    void HandleAcquisitionError(const MeasurementBatch& batch);
    void HandleMeasurementResult(AsyncResult result);
    void HandleCalibrationResult(const AsyncResult& result);
    void CheckCsvLoggerError();
    void UpdateStatusText();
    bool IsChannelEnabled(std::size_t channel) const noexcept;
    bool BeginAsyncRequest(AsyncRequest request);
    void QueueCalibrationCapture(
        AsyncRequestKind kind,
        const std::array<std::optional<double>, kChannelCount>& references,
        std::size_t sampleCount = 10,
        unsigned int delayBetweenSamplesMs = 50);
    AsyncResult ExecuteAsyncRequest(const AsyncRequest& request);
    void StopAsyncRequest();
    static CString CommunicationErrorText(const MeasurementBatch& batch);

    afx_msg void OnSysCommand(UINT, LPARAM);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC*, CWnd*, UINT);
    afx_msg BOOL OnMouseWheel(UINT, short, CPoint);
    afx_msg void OnLButtonDown(UINT, CPoint);
    afx_msg void OnLButtonUp(UINT, CPoint);
    afx_msg void OnMouseMove(UINT, CPoint);
    afx_msg void OnHScroll(UINT, UINT, CScrollBar*);
    afx_msg void OnTimer(UINT_PTR);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT drawItem);
    afx_msg LRESULT OnAsyncRequestComplete(WPARAM requestToken, LPARAM);

    afx_msg void OnBnClickedButtonStartstop();
    afx_msg void OnBnClickedButtonHelp();
    afx_msg void OnBnClickedButtonApplyInterval();
    afx_msg void OnBnClickedButtonReset();
    afx_msg void OnBnClickedExportCsv();
    afx_msg void OnBnClickedSaveImage();
    afx_msg void OnBnClickedButtonLog();
    afx_msg void OnBnClickedButtonSimulation();
    afx_msg void OnBnClickedChannelVisibility();
    afx_msg void OnBnClickedCheckDarkMode();
    afx_msg void OnBnClickedCheckAutoscaleY();
    afx_msg void OnBnClickedCheckShowPeaks();
    afx_msg void OnEnChangeEditThreshMax();
    afx_msg void OnEnChangeEditThreshMin();
    afx_msg void OnCbnSelchangeComPort();
    afx_msg void OnBnClickedCalibrate();
    afx_msg void OnBnClickedSavePoint1();
    afx_msg void OnBnClickedSavePoint2();

    void AppendToAlarms(const CString& message, COLORREF color = RGB(0, 0, 0));
};
