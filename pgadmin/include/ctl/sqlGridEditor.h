//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// sqlGridEditor.h - SQL Data Editor classes
//
//////////////////////////////////////////////////////////////////////////

#ifndef SQLGRIDEDITOR_H
#define  SQLGRIDEDITOR_H

// wxWindows headers
#include <wx/wx.h>

class sqlGridTextEditor : public wxGridCellTextEditor
{
public:
	virtual wxGridCellEditor *Clone() const
	{
		return new sqlGridTextEditor();
	}
	void Create(wxWindow *parent, wxWindowID id, wxEvtHandler *evtHandler);
	void BeginEdit(int row, int col, wxGrid *grid);


#if wxCHECK_VERSION(2, 9, 0)
	void ApplyEdit(int row, int col, wxGrid *grid);
	bool EndEdit(int row, int col, const wxGrid *grid, const wxString &, wxString *);
#else
	bool EndEdit(int row, int col, wxGrid *grid);
#endif
	wxString GetValue() const;
	virtual void Reset()
	{
		DoReset(m_startValue);
	}
	void StartingKey(wxKeyEvent &event);

protected:
	void DoBeginEdit(const wxString &startValue);
	wxStyledTextCtrl *Text() const
	{
		return (wxStyledTextCtrl *)m_control;
	}
	void DoReset(const wxString &startValue);

	wxString m_startValue;
};



class sqlGridNumericEditor : public wxGridCellTextEditor
{
public:
	sqlGridNumericEditor(int len = -1, int prec = -1)
	{
		numlen = len;
		numprec = prec;
	}
	virtual wxGridCellEditor *Clone() const
	{
		return new sqlGridNumericEditor(numlen, numprec);
	}
	virtual void Create(wxWindow *parent, wxWindowID id, wxEvtHandler *evtHandler);

	virtual bool IsAcceptedKey(wxKeyEvent &event);
	virtual void BeginEdit(int row, int col, wxGrid *grid);
#if wxCHECK_VERSION(2, 9, 0)
	void ApplyEdit(int row, int col, wxGrid *grid);
	bool EndEdit(int row, int col, const wxGrid *grid, const wxString &, wxString *);
#else
	bool EndEdit(int row, int col, wxGrid *grid);
#endif

	virtual void Reset()
	{
		DoReset(m_startValue);
	}
	virtual void StartingKey(wxKeyEvent &event);
	virtual void SetParameters(const wxString &params);

protected:
	int numlen, numprec;
	wxString m_startValue;

};



class sqlGridBoolEditor : public wxGridCellEditor
{
public:
	sqlGridBoolEditor() { }

	virtual void Create(wxWindow *parent, wxWindowID id, wxEvtHandler *evtHandler);

	virtual void SetSize(const wxRect &rect);
	virtual void Show(bool show, wxGridCellAttr *attr = (wxGridCellAttr *)NULL);

	virtual bool IsAcceptedKey(wxKeyEvent &event);
	virtual void BeginEdit(int row, int col, wxGrid *grid);

#if wxCHECK_VERSION(2, 9, 0)
	virtual void ApplyEdit(int row, int col, wxGrid *grid); // pure virtual in wx 2.9+, doesn't exist in prior versions
	virtual bool EndEdit(int row, int col, const wxGrid *grid, const wxString &, wxString *);
#else
	virtual bool EndEdit(int row, int col, wxGrid *grid);
#endif

	virtual void Reset();
	virtual void StartingClick();
	virtual void StartingKey(wxKeyEvent &event);

	virtual wxGridCellEditor *Clone() const
	{
		return new sqlGridBoolEditor;
	}

	virtual wxString GetValue() const;

protected:
	wxCheckBox *CBox() const
	{
		return (wxCheckBox *)m_control;
	}

private:
	wxCheckBoxState m_startValue;

	DECLARE_NO_COPY_CLASS(sqlGridBoolEditor)
};


#endif
