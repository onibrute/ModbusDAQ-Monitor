#pragma once
#include "afxdialogex.h"
#include "resource.h"
#include <afxrich.h>
#include <map>

class CHelpDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CHelpDialog)

public:
	explicit CHelpDialog(CWnd* pParent = nullptr);
	~CHelpDialog() override;

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HELP_DIALOG };
#endif

protected:
	void DoDataExchange(CDataExchange* pDX) override;
	BOOL OnInitDialog() override;
	afx_msg void OnRichHelpLinkClick(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	CRichEditCtrl m_richHelp;

	enum HelpTopic {
		HT_None,
		HT_Start,
		HT_Interval,
		HT_Reset,
		HT_CSV,
		HT_Image,
		HT_Log,
		HT_Checkbox,
		HT_Auto,
		HT_Peak,
		HT_Calibration
	};

	void SetHelpText();
	HelpTopic GetHelpTopicFromText(const CString& txt);
};
