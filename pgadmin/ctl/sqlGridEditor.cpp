//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// sqlGridEditor.cpp - SQL Data Editor classes
//
//////////////////////////////////////////////////////////////////////////

#include "pgAdmin3.h"

// wxWindows headers
#include <wx/wx.h>

#include "ctl/sqlGridEditor.h"


void sqlGridTextEditor::Create(wxWindow *parent, wxWindowID id, wxEvtHandler *evtHandler)
{
	int flags = wxSTC_WRAP_NONE;

	m_control = new wxStyledTextCtrl(parent, id,
	                                 wxDefaultPosition, wxDefaultSize, flags
	                                );

	wxGridCellEditor::Create(parent, id, evtHandler);
}

void sqlGridTextEditor::BeginEdit(int row, int col, wxGrid *grid)
{
	m_startValue = grid->GetTable()->GetValue(row, col);
	DoBeginEdit(m_startValue);
	((ctlSQLEditGrid *)grid)->ResizeEditor(row, col);
}

void sqlGridTextEditor::DoBeginEdit(const wxString &startValue)
{
	Text()->SetMarginWidth(1, 0);
	Text()->SetText(startValue);
	Text()->SetCurrentPos(Text()->GetTextLength());
	Text()->SetUseHorizontalScrollBar(false);
	Text()->SetUseVerticalScrollBar(false);
	Text()->SetSelection(0, -1);
	Text()->SetFocus();
}

#if wxCHECK_VERSION(2, 9, 0)
void sqlGridTextEditor::ApplyEdit(int row, int col, wxGrid *grid)
{
	wxString value = Text()->GetText();
	grid->GetTable()->SetValue(row, col, value);
}
#endif

#if wxCHECK_VERSION(2, 9, 0)
bool sqlGridTextEditor::EndEdit(int row, int col, const wxGrid *grid, const wxString &, wxString *)
#else
bool sqlGridTextEditor::EndEdit(int row, int col, wxGrid *grid)
#endif
{
	bool changed = false;
	wxString value = Text()->GetText();

	if (value != m_startValue)
		changed = true;

#if !wxCHECK_VERSION(2, 9, 0)
	if (changed)
		grid->GetTable()->SetValue(row, col, value);
#endif

	return changed;
}

wxString sqlGridTextEditor::GetValue() const
{
	return Text()->GetText();
}

void sqlGridTextEditor::DoReset(const wxString &startValue)
{
	Text()->SetText(startValue);
	Text()->SetSelection(-1, -1);
}

void sqlGridTextEditor::StartingKey(wxKeyEvent &event)
{
	wxChar ch;

#if wxUSE_UNICODE
	ch = event.GetUnicodeKey();
	if (ch <= 127)
		ch = (wxChar)event.GetKeyCode();
#else
	ch = (wxChar)event.GetKeyCode();
#endif

	if (ch != (wxChar)WXK_BACK)
	{
		Text()->SetText(ch);
		Text()->GotoPos(Text()->GetLength());
	}
}



void sqlGridNumericEditor::StartingKey(wxKeyEvent &event)
{
	int keycode = event.GetKeyCode();
	bool allowed = false;

	switch (keycode)
	{
		case WXK_DECIMAL:
		case WXK_NUMPAD_DECIMAL:
		case '.':
			if (numprec)
				allowed = true;
			break;
		case '+':
		case WXK_ADD:
		case WXK_NUMPAD_ADD:
		case '-':
		case WXK_SUBTRACT:
		case WXK_NUMPAD_SUBTRACT:

		case WXK_NUMPAD0:
		case WXK_NUMPAD1:
		case WXK_NUMPAD2:
		case WXK_NUMPAD3:
		case WXK_NUMPAD4:
		case WXK_NUMPAD5:
		case WXK_NUMPAD6:
		case WXK_NUMPAD7:
		case WXK_NUMPAD8:
		case WXK_NUMPAD9:
			allowed = true;
			break;
		default:
			if (wxIsdigit(keycode))
				allowed = true;
			break;

	}
	if (allowed)
		wxGridCellTextEditor::StartingKey(event);
	else
		event.Skip();
}

bool sqlGridNumericEditor::IsAcceptedKey(wxKeyEvent &event)
{
	if ( wxGridCellEditor::IsAcceptedKey(event) )
	{
		int keycode = event.GetKeyCode();
		switch ( keycode )
		{
			case WXK_DECIMAL:
			case WXK_NUMPAD_DECIMAL:
				return (numprec != 0);

			case '+':
			case WXK_ADD:
			case WXK_NUMPAD_ADD:
			case '-':
			case WXK_SUBTRACT:
			case WXK_NUMPAD_SUBTRACT:

			case WXK_NUMPAD0:
			case WXK_NUMPAD1:
			case WXK_NUMPAD2:
			case WXK_NUMPAD3:
			case WXK_NUMPAD4:
			case WXK_NUMPAD5:
			case WXK_NUMPAD6:
			case WXK_NUMPAD7:
			case WXK_NUMPAD8:
			case WXK_NUMPAD9:
				return true;
			default:
				return wxIsdigit(keycode) != 0;
		}
	}

	return false;
}

void sqlGridNumericEditor::BeginEdit(int row, int col, wxGrid *grid)
{
	m_startValue = grid->GetTable()->GetValue(row, col);


	wxString value = m_startValue;
	// localize value here

	DoBeginEdit(value);
}

#if wxCHECK_VERSION(2, 9, 0)
void sqlGridNumericEditor::ApplyEdit(int row, int col, wxGrid *grid)
{
	wxString value = Text()->GetValue();
	grid->GetTable()->SetValue(row, col, value);
	m_startValue = wxEmptyString;
	Text()->SetValue(m_startValue);
}
#endif

#if wxCHECK_VERSION(2, 9, 0)
bool sqlGridNumericEditor::EndEdit(int row, int col, const wxGrid *grid, const wxString &, wxString *)
#else
bool sqlGridNumericEditor::EndEdit(int row, int col, wxGrid *grid)
#endif
{
	wxASSERT_MSG(m_control,
	             wxT("The sqlGridNumericEditor must be Created first!"));

	bool changed = false;
	wxString value = Text()->GetValue();

	// de-localize value here

	if (value != m_startValue)
		changed = true;

#if !wxCHECK_VERSION(2, 9, 0)
	if (changed)
		grid->GetTable()->SetValue(row, col, value);

	m_startValue = wxEmptyString;
	Text()->SetValue(m_startValue);
#endif
	return changed;
}

void sqlGridNumericEditor::SetParameters(const wxString &params)
{
	if ( !params )
	{
		// reset to default
		numlen = -1;
		numprec = -1;
	}
	else
	{
		long tmp;
		if ( params.BeforeFirst(wxT(',')).ToLong(&tmp) )
		{
			numlen = (int)tmp;

			if ( params.AfterFirst(wxT(',')).ToLong(&tmp) )
			{
				numprec = (int)tmp;

				// skip the error message below
				return;
			}
		}
	}
}

void sqlGridNumericEditor::Create(wxWindow *parent, wxWindowID id, wxEvtHandler *evtHandler)
{
	m_control = new wxTextCtrl(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize);

	wxGridCellEditor::Create(parent, id, evtHandler);
	Text()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
}



//////////////////////////////////////////////////////////////////////
// Bool editor
//////////////////////////////////////////////////////////////////////

void sqlGridBoolEditor::Create(wxWindow *parent, wxWindowID id, wxEvtHandler *evtHandler)
{
	m_control = new wxCheckBox(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxNO_BORDER | wxCHK_3STATE | wxCHK_ALLOW_3RD_STATE_FOR_USER);

	wxGridCellEditor::Create(parent, id, evtHandler);
}

void sqlGridBoolEditor::SetSize(const wxRect &r)
{
	bool resize = false;
	wxSize size = m_control->GetSize();
	wxCoord minSize = wxMin(r.width, r.height);

	// check if the checkbox is not too big/small for this cell
	wxSize sizeBest = m_control->GetBestSize();
	if ( !(size == sizeBest) )
	{
		// reset to default size if it had been made smaller
		size = sizeBest;

		resize = true;
	}

	if ( size.x >= minSize || size.y >= minSize )
	{
		// leave 1 pixel margin
		size.x = size.y = minSize - 2;

		resize = true;
	}

	if ( resize )
	{
		m_control->SetSize(size);
	}

	// position it in the centre of the rectangle (TODO: support alignment?)

#if defined(__WXGTK__)
	// the checkbox without label still has some space to the right in wxGTK,
	// so shift it to the right
	size.x -= 8;
#elif defined(__WXMSW__)
	// here too, but in other way
	size.x += 1;
	size.y -= 2;
#endif

	int hAlign = wxALIGN_CENTRE;
	int vAlign = wxALIGN_CENTRE;
	if (GetCellAttr())
		GetCellAttr()->GetAlignment(& hAlign, & vAlign);

	int x = 0, y = 0;
	if (hAlign == wxALIGN_LEFT)
	{
		x = r.x + 2;
#ifdef __WXMSW__
		x += 2;
#endif
		y = r.y + r.height / 2 - size.y / 2;
	}
	else if (hAlign == wxALIGN_RIGHT)
	{
		x = r.x + r.width - size.x - 2;
		y = r.y + r.height / 2 - size.y / 2;
	}
	else if (hAlign == wxALIGN_CENTRE)
	{
		x = r.x + r.width / 2 - size.x / 2;
		y = r.y + r.height / 2 - size.y / 2;
	}

	m_control->Move(x, y);
}

void sqlGridBoolEditor::Show(bool show, wxGridCellAttr *attr)
{
	m_control->Show(show);

	if ( show )
	{
		wxColour colBg = attr ? attr->GetBackgroundColour() : *wxLIGHT_GREY;
		CBox()->SetBackgroundColour(colBg);
	}
}

void sqlGridBoolEditor::BeginEdit(int row, int col, wxGrid *grid)
{
	wxASSERT_MSG(m_control, wxT("The sqlGridBoolEditor must be Created first!"));

	wxString value = grid->GetTable()->GetValue(row, col);
	if (value == wxT("TRUE"))
		m_startValue = wxCHK_CHECKED;
	else if (value == wxT("FALSE"))
		m_startValue = wxCHK_UNCHECKED;
	else
		m_startValue = wxCHK_UNDETERMINED;

	CBox()->Set3StateValue(m_startValue);
	CBox()->SetFocus();
}

#define BOOL_EDIT_SWITCH switch (value) \
{ \
			case wxCHK_UNCHECKED:\
				grid->GetTable()->SetValue(row, col, wxT("FALSE"));\
				break;\
			case wxCHK_CHECKED:\
				grid->GetTable()->SetValue(row, col, wxT("TRUE"));\
				break;\
			case wxCHK_UNDETERMINED:\
				grid->GetTable()->SetValue(row, col, wxEmptyString);\
				break;\
}\

#if wxCHECK_VERSION(2, 9, 0)
// pure virtual in 2.9+, doesn't exist in prior versions
void sqlGridBoolEditor::ApplyEdit(int row, int col, wxGrid *grid)
{
	wxCheckBoxState value = CBox()->Get3StateValue();
	BOOL_EDIT_SWITCH
}
#endif

#if wxCHECK_VERSION(2, 9, 0)
bool sqlGridBoolEditor::EndEdit(int row, int col, const wxGrid *grid, const wxString &, wxString *)
#else
bool sqlGridBoolEditor::EndEdit(int row, int col, wxGrid *grid)
#endif
{
	wxASSERT_MSG(m_control, wxT("The sqlGridBoolEditor must be Created first!"));

	bool changed = false;
	wxCheckBoxState value = CBox()->Get3StateValue();
	if ( value != m_startValue )
		changed = true;

#if !wxCHECK_VERSION(2, 9, 0)
	if ( changed )
	{
		BOOL_EDIT_SWITCH
	}
#endif

	return changed;
}

void sqlGridBoolEditor::Reset()
{
	wxASSERT_MSG(m_control, wxT("The wxGridCellEditor must be Created first!"));

	CBox()->Set3StateValue(m_startValue);
}

void sqlGridBoolEditor::StartingClick()
{
	// We used to cycle the value on click here but
	// that can lead to odd behaviour of the cell.
	// Without cycling here, the checkbox is displayed
	// but the user must toggle the box itself - she
	// cannot just keep clicking the cell.
}

bool sqlGridBoolEditor::IsAcceptedKey(wxKeyEvent &event)
{
	if ( wxGridCellEditor::IsAcceptedKey(event) )
	{
		int keycode = event.GetKeyCode();
		switch ( keycode )
		{
			case WXK_SPACE:
			case '+':
			case '-':
			case 'n':
			case 'N':
				return true;
		}
	}

	return false;
}

void sqlGridBoolEditor::StartingKey(wxKeyEvent &event)
{
	int keycode = event.GetKeyCode();
	wxCheckBoxState value = CBox()->Get3StateValue();

	switch ( keycode )
	{
		case WXK_SPACE:
			switch (value)
			{
				case wxCHK_UNCHECKED:
				case wxCHK_UNDETERMINED:
					CBox()->Set3StateValue(wxCHK_CHECKED);
					break;
				case wxCHK_CHECKED:
					CBox()->Set3StateValue(wxCHK_UNCHECKED);
					break;
			}
			break;

		case '+':
			CBox()->Set3StateValue(wxCHK_CHECKED);
			break;

		case '-':
			CBox()->Set3StateValue(wxCHK_UNCHECKED);
			break;

		case 'n':
		case 'N':
			CBox()->Set3StateValue(wxCHK_UNDETERMINED);
			break;
	}
}

// return the value as "1" for true and the empty string for false
wxString sqlGridBoolEditor::GetValue() const
{

	wxCheckBoxState value = CBox()->Get3StateValue();

	switch (value)
	{
		case wxCHK_UNCHECKED:
			return wxT("FALSE");
			break;
		case wxCHK_CHECKED:
			return wxT("TRUE");
			break;
		case wxCHK_UNDETERMINED:
			return wxEmptyString;
			break;
	}
	return wxEmptyString;
}

//////////////////////////////////////////////////////////////////////
// End Bool editor
//////////////////////////////////////////////////////////////////////

