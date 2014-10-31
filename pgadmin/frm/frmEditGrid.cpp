//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// frmEditGrid.cpp - Edit Grid Box
//
//////////////////////////////////////////////////////////////////////////

// wxWindows headers
#include <wx/wx.h>
#include <wx/grid.h>

// App headers
#include "pgAdmin3.h"
#include "utils/pgDefs.h"
#include "frm/frmMain.h"
#include "frm/menu.h"
#include "db/pgQueryThread.h"

#include <wx/generic/gridctrl.h>
#include <wx/clipbrd.h>

#include "frm/frmAbout.h"
#include "frm/frmEditGrid.h"
#include "dlg/dlgEditGridOptions.h"
#include "frm/frmHint.h"
#include "schema/pgCatalogObject.h"
#include "schema/pgTable.h"
#include "schema/pgForeignTable.h"
#include "schema/pgView.h"
#include "schema/gpExtTable.h"

// wxAUI
#include <wx/aui/aui.h>

// Icons
#include "images/viewdata.pngc"
#include "images/storedata.pngc"
#include "images/readdata.pngc"
#include "images/delete.pngc"
#include "images/edit_undo.pngc"
#include "images/sortfilter.pngc"
#include "images/help.pngc"
#include "images/clip_copy.pngc"
#include "images/clip_paste.pngc"
#include "images/warning.pngc" // taken from http://www.iconarchive.com/show/basic-icons-by-pixelmixer/warning-icon.html (freeware)

#define CTRLID_LIMITCOMBO       4226


BEGIN_EVENT_TABLE(frmEditGrid, pgFrame)
	EVT_ERASE_BACKGROUND(       frmEditGrid::OnEraseBackground)
	EVT_SIZE(                   frmEditGrid::OnSize)
	EVT_MENU(MNU_REFRESH,       frmEditGrid::OnRefresh)
	EVT_MENU(MNU_DELETE,        frmEditGrid::OnDelete)
	EVT_MENU(MNU_SAVE,          frmEditGrid::OnSave)
	EVT_MENU(MNU_INCLUDEFILTER, frmEditGrid::OnIncludeFilter)
	EVT_MENU(MNU_EXCLUDEFILTER, frmEditGrid::OnExcludeFilter)
	EVT_MENU(MNU_REMOVEFILTERS, frmEditGrid::OnRemoveFilters)
	EVT_MENU(MNU_ASCSORT,       frmEditGrid::OnAscSort)
	EVT_MENU(MNU_DESCSORT,      frmEditGrid::OnDescSort)
	EVT_MENU(MNU_REMOVESORT,    frmEditGrid::OnRemoveSort)
	EVT_MENU(MNU_UNDO,          frmEditGrid::OnUndo)
	EVT_MENU(MNU_OPTIONS,       frmEditGrid::OnOptions)
	EVT_MENU(MNU_HELP,          frmEditGrid::OnHelp)
	EVT_MENU(MNU_CONTENTS,      frmEditGrid::OnContents)
	EVT_MENU(MNU_COPY,          frmEditGrid::OnCopy)
	EVT_MENU(MNU_PASTE,         frmEditGrid::OnPaste)
	EVT_MENU(MNU_LIMITBAR,      frmEditGrid::OnToggleLimitBar)
	EVT_MENU(MNU_TOOLBAR,       frmEditGrid::OnToggleToolBar)
	EVT_MENU(MNU_SCRATCHPAD,    frmEditGrid::OnToggleScratchPad)
	EVT_MENU(MNU_DEFAULTVIEW,   frmEditGrid::OnDefaultView)
	EVT_MENU(MNU_CLOSE,         frmEditGrid::OnClose)
	EVT_CLOSE(                  frmEditGrid::OnCloseWindow)
	EVT_KEY_DOWN(               frmEditGrid::OnKey)
	EVT_GRID_RANGE_SELECT(      frmEditGrid::OnGridSelectCells)
	EVT_GRID_SELECT_CELL(       frmEditGrid::OnCellChange)
	EVT_GRID_EDITOR_SHOWN(      frmEditGrid::OnEditorShown)
	EVT_GRID_EDITOR_HIDDEN(     frmEditGrid::OnEditorHidden)
	EVT_GRID_CELL_RIGHT_CLICK(  frmEditGrid::OnCellRightClick)
	EVT_GRID_LABEL_RIGHT_CLICK( frmEditGrid::OnLabelRightClick)
	EVT_AUI_PANE_BUTTON(        frmEditGrid::OnAuiUpdate)
END_EVENT_TABLE()


frmEditGrid::frmEditGrid(frmMain *form, const wxString &_title, pgConn *_conn, pgSchemaObject *obj, bool pkAscending)
	: pgFrame(NULL, _title)
{
	closing = false;

	SetIcon(*viewdata_png_ico);
	SetFont(settings->GetSystemFont());
	dlgName = wxT("frmEditGrid");
	RestorePosition(-1, -1, 600, 500, 300, 350);
	connection = _conn;
	mainForm = form;
	thread = 0;
	relkind = 0;
	limit = 0;
	relid = (Oid)obj->GetOid();
	editorCell = new sqlCell();

	// notify wxAUI which frame to use
	manager.SetManagedWindow(this);
	manager.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_TRANSPARENT_DRAG);

	SetMinSize(wxSize(300, 200));

	int iWidths[EGSTATUSPOS_COUNT] = {20, 150, -1};
	wxStatusBar *pBar = CreateStatusBar(EGSTATUSPOS_COUNT);
	SetStatusBarPane(EGSTATUSPOS_MSGS);
	pBar->SetStatusWidths(EGSTATUSPOS_COUNT, iWidths);
	mStatusBitmap = new wxStaticBitmap(pBar, -1, wxBitmap(*warning_png_img));
	mStatusBitmap->SetToolTip(_("Some rows may have not been fetched due to a filtering condition or a rows limit!"));
	mStatusBitmap->Show(false);

	sqlGrid = new ctlSQLEditGrid(this, CTL_EDITGRID, wxDefaultPosition, wxDefaultSize);
	sqlGrid->SetTable(0);
#ifdef __WXMSW__
	sqlGrid->SetDefaultRowSize(sqlGrid->GetDefaultRowSize() + 2, true);
#endif

	// Set up toolbar
	toolBar = new ctlMenuToolbar(this, -1, wxDefaultPosition, wxDefaultSize, wxTB_FLAT | wxTB_NODIVIDER);
	toolBar->SetToolBitmapSize(wxSize(16, 16));

	toolBar->AddTool(MNU_SAVE, wxEmptyString, *storedata_png_bmp, _("Save the changed row."), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_REFRESH, wxEmptyString, *readdata_png_bmp, _("Refresh."), wxITEM_NORMAL);
	toolBar->AddTool(MNU_UNDO, wxEmptyString, *edit_undo_png_bmp, _("Undo change of data."), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_COPY, wxEmptyString, *clip_copy_png_bmp, _("Copy selected lines to clipboard."), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_PASTE, wxEmptyString, *clip_paste_png_bmp, _("Paste data from the clipboard."), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_DELETE, wxEmptyString, *delete_png_bmp, _("Delete selected rows."), wxITEM_NORMAL);
	toolBar->AddSeparator();

	toolBar->AddTool(MNU_OPTIONS, wxEmptyString, *sortfilter_png_bmp, _("Sort/filter options."), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_HELP, wxEmptyString, *help_png_bmp, _("Display help on this window."));

	toolBar->Realize();
	toolBar->EnableTool(MNU_SAVE, false);
	toolBar->EnableTool(MNU_UNDO, false);
	toolBar->EnableTool(MNU_DELETE, false);

	// Setup the limit bar
#ifndef __WXMAC__
	cbLimit = new wxComboBox(this, CTRLID_LIMITCOMBO, wxEmptyString, wxPoint(0, 0), wxSize(GetCharWidth() * 12, -1), wxArrayString(), wxCB_DROPDOWN);
#else
	cbLimit = new wxComboBox(this, CTRLID_LIMITCOMBO, wxEmptyString, wxPoint(0, 0), wxSize(GetCharWidth() * 24, -1), wxArrayString(), wxCB_DROPDOWN);
#endif
	cbLimit->Append(_("No limit"));
	cbLimit->Append(_("1000 rows"));
	cbLimit->Append(_("500 rows"));
	cbLimit->Append(_("100 rows"));
	cbLimit->SetValue(_("No limit"));
	cbLimit->SetToolTip(_("Maximum number of rows to be fetched"));

	// Finally, the scratchpad
	scratchPad = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxHSCROLL);

	// Menus

	// File menu
	fileMenu = new wxMenu();
	fileMenu->Append(MNU_SAVE, _("&Save\tCtrl-S"), _("Save the changed row."));
	fileMenu->AppendSeparator();
	fileMenu->Append(MNU_CLOSE, _("&Close\tCtrl-W"), _("Close this window."));
	fileMenu->Enable(MNU_SAVE, false);

	// Edit menu
	editMenu = new wxMenu();
	editMenu->Append(MNU_UNDO, _("&Undo\tCtrl-Z"), _("Undo change of data."));
	editMenu->AppendSeparator();
	editMenu->Append(MNU_COPY, _("&Copy\tCtrl-C"), _("Copy selected cells to clipboard."));
	editMenu->Append(MNU_PASTE, _("&Paste\tCtrl-V"), _("Paste data from the clipboard."));
	editMenu->Append(MNU_DELETE, _("&Delete"), _("Delete selected rows."));
	editMenu->Enable(MNU_UNDO, false);
	editMenu->Enable(MNU_DELETE, false);


	// View menu
	viewMenu = new wxMenu();
	viewMenu->Append(MNU_REFRESH, _("&Refresh\tF5"), _("Refresh."));
	viewMenu->AppendSeparator();
	viewMenu->Append(MNU_LIMITBAR, _("&Limit bar\tCtrl-Alt-L"), _("Show or hide the row limit options bar."), wxITEM_CHECK);
	viewMenu->Append(MNU_SCRATCHPAD, _("S&cratch pad\tCtrl-Alt-S"), _("Show or hide the scratch pad."), wxITEM_CHECK);
	viewMenu->Append(MNU_TOOLBAR, _("&Tool bar\tCtrl-Alt-T"), _("Show or hide the tool bar."), wxITEM_CHECK);
	viewMenu->AppendSeparator();
	viewMenu->Append(MNU_DEFAULTVIEW, _("&Default view\tCtrl-Alt-V"),     _("Restore the default view."));


	// Tools menu
	toolsMenu = new wxMenu();
	toolsMenu->Append(MNU_OPTIONS, _("&Sort / Filter ..."), _("Sort / Filter options."));
	toolsMenu->AppendSeparator();
	toolsMenu->Append(MNU_INCLUDEFILTER, _("Filter By &Selection"), _("Display only those rows that have this value in this column."));
	toolsMenu->Append(MNU_EXCLUDEFILTER, _("Filter E&xcluding Selection"), _("Display only those rows that do not have this value in this column."));
	toolsMenu->Append(MNU_REMOVEFILTERS, _("&Remove Filter"), _("Remove all filters on this table"));
	toolsMenu->AppendSeparator();
	toolsMenu->Append(MNU_ASCSORT, _("Sort &Ascending"), _("Append an ASCENDING sort condition based on this column"));
	toolsMenu->Append(MNU_DESCSORT, _("Sort &Descending"), _("Append a DESCENDING sort condition based on this column"));
	toolsMenu->Append(MNU_REMOVESORT, _("&Remove Sort"), _("Remove all sort conditions"));

	// Help menu
	helpMenu = new wxMenu();
	helpMenu->Append(MNU_CONTENTS, _("&Help contents"), _("Open the helpfile."));
	helpMenu->Append(MNU_HELP, _("&Edit grid help"), _("Display help on this window."));

#ifdef __WXMAC__
	menuFactories = new menuFactoryList();
	aboutFactory *af = new aboutFactory(menuFactories, helpMenu, 0);
	wxApp::s_macAboutMenuItemId = af->GetId();
	menuFactories->RegisterMenu(this, wxCommandEventHandler(pgFrame::OnAction));
#endif

	menuBar = new wxMenuBar();
	menuBar->Append(fileMenu, _("&File"));
	menuBar->Append(editMenu, _("&Edit"));
	menuBar->Append(viewMenu, _("&View"));
	menuBar->Append(toolsMenu, _("&Tools"));
	menuBar->Append(helpMenu, _("&Help"));
	SetMenuBar(menuBar);

	// Accelerators
	wxAcceleratorEntry entries[8];

	entries[0].Set(wxACCEL_CTRL,                (int)'S',      MNU_SAVE);
	entries[1].Set(wxACCEL_NORMAL,              WXK_F5,        MNU_REFRESH);
	entries[2].Set(wxACCEL_CTRL,                (int)'Z',      MNU_UNDO);
	entries[3].Set(wxACCEL_NORMAL,              WXK_F1,        MNU_HELP);
	entries[4].Set(wxACCEL_CTRL,                (int)'C',      MNU_COPY);
	entries[5].Set(wxACCEL_CTRL,                (int)'V',      MNU_PASTE);
	entries[6].Set(wxACCEL_NORMAL,              WXK_DELETE,    MNU_DELETE);
	entries[7].Set(wxACCEL_CTRL,                (int)'W',      MNU_CLOSE);

	wxAcceleratorTable accel(8, entries);
	SetAcceleratorTable(accel);
	sqlGrid->SetAcceleratorTable(accel);

	// Kickstart wxAUI
	manager.AddPane(toolBar, wxAuiPaneInfo().Name(wxT("toolBar")).Caption(_("Tool bar")).ToolbarPane().Top().LeftDockable(false).RightDockable(false));
	manager.AddPane(cbLimit, wxAuiPaneInfo().Name(wxT("limitBar")).Caption(_("Limit bar")).ToolbarPane().Top().LeftDockable(false).RightDockable(false));
	manager.AddPane(sqlGrid, wxAuiPaneInfo().Name(wxT("sqlGrid")).Caption(_("Data grid")).Center().CaptionVisible(false).CloseButton(false).MinSize(wxSize(200, 100)).BestSize(wxSize(300, 200)));
	manager.AddPane(scratchPad, wxAuiPaneInfo().Name(wxT("scratchPad")).Caption(_("Scratch pad")).Bottom().MinSize(wxSize(200, 100)).BestSize(wxSize(300, 150)));

	// Now load the layout
	wxString perspective;
	settings->Read(wxT("frmEditGrid/Perspective-") + wxString(FRMEDITGRID_PERSPECTIVE_VER), &perspective, FRMEDITGRID_DEFAULT_PERSPECTIVE);
	manager.LoadPerspective(perspective, true);

	// and reset the captions for the current language
	manager.GetPane(wxT("toolBar")).Caption(_("Tool bar"));
	manager.GetPane(wxT("limitBar")).Caption(_("Limit bar"));
	manager.GetPane(wxT("sqlGrid")).Caption(_("Data grid"));
	manager.GetPane(wxT("scratchPad")).Caption(_("Scratch pad"));

	// Sync the View menu options
	viewMenu->Check(MNU_LIMITBAR, manager.GetPane(wxT("limitBar")).IsShown());
	viewMenu->Check(MNU_TOOLBAR, manager.GetPane(wxT("toolBar")).IsShown());
	viewMenu->Check(MNU_SCRATCHPAD, manager.GetPane(wxT("scratchPad")).IsShown());

	// tell the manager to "commit" all the changes just made
	manager.Update();

	autoOrderBy = false;
	if (obj->GetMetaType() == PGM_TABLE || obj->GetMetaType() == GP_PARTITION)
	{
		pgTable *table = (pgTable *)obj;

		relkind = 'r';
		hasOids = table->GetHasOids();
		tableName = table->GetSchema()->GetQuotedFullIdentifier() + wxT(".") + table->GetQuotedIdentifier();
		primaryKeyColNumbers = table->GetPrimaryKeyColNumbers();
		autoOrderBy = true; // this orderBy will be dismissed if a user defines his order
		orderBy = table->GetQuotedPrimaryKey();
		if (orderBy.IsEmpty() && hasOids)
			orderBy = wxT("oid");
		if (!orderBy.IsEmpty())
		{
			if (pkAscending)
			{
				orderBy.Replace(wxT(","), wxT(" ASC,"));
				orderBy += wxT(" ASC");
			}
			else
			{
				orderBy.Replace(wxT(","), wxT(" DESC,"));
				orderBy += wxT(" DESC");
			}
		}
	}
	else if (obj->GetMetaType() == PGM_VIEW)
	{
		pgView *view = (pgView *)obj;

		relkind = 'v';
		hasOids = false;
		tableName = view->GetSchema()->GetQuotedFullIdentifier() + wxT(".") + view->GetQuotedIdentifier();
	}
	else if (obj->GetMetaType() == PGM_FOREIGNTABLE)
	{
		pgForeignTable *foreigntable = (pgForeignTable *)obj;

		relkind = 'f';
		hasOids = false;
		tableName = foreigntable->GetSchema()->GetQuotedFullIdentifier() + wxT(".") + foreigntable->GetQuotedIdentifier();
	}
	else if (obj->GetMetaType() == GP_EXTTABLE)
	{
		gpExtTable *exttable = (gpExtTable *)obj;

		relkind = 'x';
		hasOids = false;
		tableName = exttable->GetSchema()->GetQuotedFullIdentifier() + wxT(".") + exttable->GetQuotedIdentifier();
	}
	else if (obj->GetMetaType() == PGM_CATALOGOBJECT)
	{
		pgCatalogObject *catobj = (pgCatalogObject *)obj;

		relkind = 'v';
		hasOids = false;
		tableName = catobj->GetSchema()->GetQuotedFullIdentifier() + wxT(".") + catobj->GetQuotedIdentifier();
	}
}

void frmEditGrid::OnEraseBackground(wxEraseEvent &event)
{
	event.Skip();
}

void frmEditGrid::OnSize(wxSizeEvent &event)
{
	// position statusbar icon over the appropriate field
	wxStatusBar* sbar = GetStatusBar();
	if( sbar && mStatusBitmap )
	{
		wxRect r;
		int iconH, iconW;
		sbar->GetFieldRect(EGSTATUSPOS_ICON, r);
		mStatusBitmap->GetSize(&iconH, &iconW);
		mStatusBitmap->Move(r.x + r.width/2 - iconW/2, r.y + r.height/2 - iconH/2);
	}
	event.Skip();
}

void frmEditGrid::OnToggleLimitBar(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_LIMITBAR))
		manager.GetPane(wxT("limitBar")).Show(true);
	else
		manager.GetPane(wxT("limitBar")).Show(false);
	manager.Update();
}

void frmEditGrid::OnToggleToolBar(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_TOOLBAR))
		manager.GetPane(wxT("toolBar")).Show(true);
	else
		manager.GetPane(wxT("toolBar")).Show(false);
	manager.Update();
}

void frmEditGrid::OnToggleScratchPad(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_SCRATCHPAD))
		manager.GetPane(wxT("scratchPad")).Show(true);
	else
		manager.GetPane(wxT("scratchPad")).Show(false);
	manager.Update();
}

void frmEditGrid::OnAuiUpdate(wxAuiManagerEvent &event)
{
	if(event.pane->name == wxT("limitBar"))
	{
		viewMenu->Check(MNU_LIMITBAR, false);
	}
	else if(event.pane->name == wxT("toolBar"))
	{
		viewMenu->Check(MNU_TOOLBAR, false);
	}
	else if(event.pane->name == wxT("scratchPad"))
	{
		viewMenu->Check(MNU_SCRATCHPAD, false);
	}
	event.Skip();
}

void frmEditGrid::OnDefaultView(wxCommandEvent &event)
{
	manager.LoadPerspective(FRMEDITGRID_DEFAULT_PERSPECTIVE, true);

	// Reset the captions for the current language
	manager.GetPane(wxT("toolBar")).Caption(_("Tool bar"));
	manager.GetPane(wxT("limitBar")).Caption(_("Limit bar"));
	manager.GetPane(wxT("sqlGrid")).Caption(_("Data grid"));
	manager.GetPane(wxT("scratchPad")).Caption(_("Scratch pad"));

	// tell the manager to "commit" all the changes just made
	manager.Update();

	// Sync the View menu options
	viewMenu->Check(MNU_LIMITBAR, manager.GetPane(wxT("limitBar")).IsShown());
	viewMenu->Check(MNU_TOOLBAR, manager.GetPane(wxT("toolBar")).IsShown());
	viewMenu->Check(MNU_SCRATCHPAD, manager.GetPane(wxT("scratchPad")).IsShown());
}

void frmEditGrid::SetSortCols(const wxString &cols)
{
	if (orderBy != cols)
	{
		orderBy = cols;
	}
}

void frmEditGrid::SetFilter(const wxString &filter)
{
	if (rowFilter != filter)
	{
		rowFilter = filter;
	}
}

void frmEditGrid::SetLimit(const int rowlimit)
{
	if (rowlimit != limit)
	{
		limit = rowlimit;

		if (limit <= 0)
			cbLimit->SetValue(_("No limit"));
		else
			cbLimit->SetValue(wxString::Format(wxPLURAL("%i row", "%i rows", limit), limit));
	}
}

void frmEditGrid::SetStatusTextRows(const int numRows)
{
	wxString status = wxString::Format(wxPLURAL("%d row", "%d rows", numRows), numRows);
	bool showWarn = false;

	if (GetFilter().Trim().Len() > 0) {
		status += (showWarn) ? wxT(", "): wxT(" (");
		status += _("Filter");
		showWarn = true;
	}

	if (limit > 0 && numRows == limit) {
		status += (showWarn) ? wxT(", "): wxT(" (");
		status += _("Limit");
		showWarn = true;
	}

	mStatusBitmap->Show(showWarn);
	if (showWarn) status += wxT(")");
	SetStatusText(status, EGSTATUSPOS_ROWS);
}

void frmEditGrid::OnLabelRightClick(wxGridEvent &event)
{
	wxMenu *xmenu = new wxMenu();
	wxArrayInt rows = sqlGrid->GetSelectedRows();
	xmenu->Append(MNU_COPY, _("&Copy"), _("Copy selected cells to clipboard."));
	xmenu->Append(MNU_PASTE, _("&Paste"), _("Paste data from the clipboard."));
	xmenu->Append(MNU_DELETE, _("&Delete"), _("Delete selected rows."));

	if ((rows.GetCount()) && (!sqlGrid->IsCurrentCellReadOnly()))
	{
		xmenu->Enable(MNU_COPY, true);
		xmenu->Enable(MNU_DELETE, true);
		xmenu->Enable(MNU_PASTE, true);
	}
	else
	{
		xmenu->Enable(MNU_COPY, false);
		xmenu->Enable(MNU_DELETE, false);
		xmenu->Enable(MNU_PASTE, false);
	}
	sqlGrid->PopupMenu(xmenu);
}


void frmEditGrid::OnCellRightClick(wxGridEvent &event)
{
	wxMenu *xmenu = new wxMenu();

	// If we cannot refresh, assume there is a data thread running. We cannot
	// check thread->IsRunning() as it can crash if the thread is in some
	// states :-(
	if (!toolBar->GetToolEnabled(MNU_REFRESH))
		return;

	sqlGrid->SetGridCursor(event.GetRow(), event.GetCol());

	xmenu->Append(MNU_INCLUDEFILTER, _("Filter By &Selection"), _("Display only those rows that have this value in this column."));
	xmenu->Append(MNU_EXCLUDEFILTER, _("Filter E&xcluding Selection"), _("Display only those rows that do not have this value in this column."));
	xmenu->Append(MNU_REMOVEFILTERS, _("&Remove Filter"), _("Remove all filters on this table"));
	xmenu->InsertSeparator(3);
	xmenu->Append(MNU_ASCSORT, _("Sort &Ascending"), _("Append an ASCENDING sort condition based on this column"));
	xmenu->Append(MNU_DESCSORT, _("Sort &Descending"), _("Append a DESCENDING sort condition based on this column"));
	xmenu->Append(MNU_REMOVESORT, _("&Remove Sort"), _("Remove all sort conditions"));

	xmenu->Enable(MNU_INCLUDEFILTER, true);
	xmenu->Enable(MNU_EXCLUDEFILTER, true);
	xmenu->Enable(MNU_REMOVEFILTERS, true);
	xmenu->Enable(MNU_ASCSORT, true);
	xmenu->Enable(MNU_DESCSORT, true);
	xmenu->Enable(MNU_REMOVESORT, true);

	sqlGrid->PopupMenu(xmenu);
}


void frmEditGrid::OnCellChange(wxGridEvent &event)
{
	sqlTable *table = sqlGrid->GetTable();
	bool doSkip = true;

	if (table)
	{
		if (table->LastRow() >= 0)
		{
			if (table->LastRow() != event.GetRow())
			{
				doSkip = DoSave();
			}
		}
		else if (sqlGrid->GetGridCursorRow() != event.GetRow())
		{
			toolBar->EnableTool(MNU_SAVE, false);
			toolBar->EnableTool(MNU_UNDO, false);
			fileMenu->Enable(MNU_SAVE, false);
			editMenu->Enable(MNU_UNDO, false);
		}
		SetStatusText(table->GetColDescription(event.GetCol()), EGSTATUSPOS_MSGS);
	}

	if (doSkip)
		event.Skip();
}


void frmEditGrid::OnIncludeFilter(wxCommandEvent &event)
{
	int curcol = sqlGrid->GetGridCursorCol();
	int currow = sqlGrid->GetGridCursorRow();

	if (curcol == -1 || currow == -1)
		return;

	sqlTable *table = sqlGrid->GetTable();
	wxString column_label = qtIdent(table->GetColLabelValueUnformatted(curcol));
	wxString new_filter_string;

	size_t old_filter_string_length = GetFilter().Trim().Len();

	if (old_filter_string_length > 0)
	{
		new_filter_string = GetFilter().Trim() + wxT(" \n    AND ");
	}

	if (table->IsColText(curcol))
	{

		if (sqlGrid->GetCellValue(currow, curcol).IsNull())
		{
			new_filter_string += column_label + wxT(" IS NULL ");
		}
		else
		{

			if (sqlGrid->GetCellValue(currow, curcol) == wxT("\'\'"))
			{
				new_filter_string += column_label + wxT(" = ''");
			}
			else
			{
				new_filter_string += column_label + wxT(" = ") + connection->qtDbString(sqlGrid->GetCellValue(currow, curcol)) + wxT(" ");
			}
		}
	}
	else
	{

		if (sqlGrid->GetCellValue(currow, curcol).IsNull())
		{
			new_filter_string += column_label + wxT(" IS NULL ");
		}
		else
		{
			new_filter_string += column_label + wxT(" = ") + sqlGrid->GetCellValue(currow, curcol);
		}
	}

	SetFilter(new_filter_string);

	Go();
}


void frmEditGrid::OnExcludeFilter(wxCommandEvent &event)
{
	int curcol = sqlGrid->GetGridCursorCol();
	int currow = sqlGrid->GetGridCursorRow();

	if (curcol == -1 || currow == -1)
		return;

	sqlTable *table = sqlGrid->GetTable();
	wxString column_label = qtIdent(table->GetColLabelValueUnformatted(curcol));
	wxString new_filter_string;

	size_t old_filter_string_length = GetFilter().Trim().Len();

	if (old_filter_string_length > 0)
	{
		new_filter_string = GetFilter().Trim() + wxT(" \n    AND ");
	}

	if (table->IsColText(curcol))
	{
		if (sqlGrid->GetCellValue(currow, curcol).IsNull())
		{
			new_filter_string += column_label + wxT(" IS NOT NULL ");
		}
		else
		{

			if (sqlGrid->GetCellValue(currow, curcol) == wxT("\'\'"))
			{
				new_filter_string += column_label + wxString::Format(wxT(" IS DISTINCT FROM '' ")) ;
			}
			else
			{
				new_filter_string += column_label + wxT(" IS DISTINCT FROM ") + connection->qtDbString(sqlGrid->GetCellValue(currow, curcol)) + wxT(" ");
			}
		}
	}
	else
	{

		if (sqlGrid->GetCellValue(currow, curcol).IsNull())
		{
			new_filter_string += column_label + wxT(" IS NOT NULL ") ;
		}
		else
		{
			new_filter_string += column_label + wxT(" IS DISTINCT FROM ") + sqlGrid->GetCellValue(currow, curcol);
		}
	}

	SetFilter(new_filter_string);

	Go();
}


void frmEditGrid::OnRemoveFilters(wxCommandEvent &event)
{
	SetFilter(wxT(""));

	Go();
}


void frmEditGrid::OnAscSort(wxCommandEvent &ev)
{
	int curcol = sqlGrid->GetGridCursorCol();

	if (curcol == -1)
		return;

	sqlTable *table = sqlGrid->GetTable();
	wxString column_label = qtIdent(table->GetColLabelValueUnformatted(curcol));
	wxString old_sort_string, new_sort_string;

	if (autoOrderBy) {
		autoOrderBy = false;
		old_sort_string = wxT("");
	}
	else
		old_sort_string = GetSortCols().Trim();

	if (old_sort_string.Find(column_label) == wxNOT_FOUND)
	{
		if (old_sort_string.Len() > 0)
			new_sort_string = old_sort_string + wxT(" , ");

		new_sort_string += column_label + wxT(" ASC ");
	}
	else
	{
		if (old_sort_string.Find(column_label + wxT(" ASC")) == wxNOT_FOUND)
		{
			// Previous occurrence was for DESCENDING sort
			new_sort_string = old_sort_string;
			new_sort_string.Replace(column_label + wxT(" DESC"), column_label + wxT(" ASC"));
		}
		else
		{
			// Previous occurrence was for ASCENDING sort. Nothing to do
			new_sort_string = old_sort_string;
		}
	}

	SetSortCols(new_sort_string);

	Go();
}


void frmEditGrid::OnDescSort(wxCommandEvent &ev)
{
	int curcol = sqlGrid->GetGridCursorCol();

	if (curcol == -1)
		return;

	sqlTable *table = sqlGrid->GetTable();
	wxString column_label = qtIdent(table->GetColLabelValueUnformatted(curcol));
	wxString old_sort_string, new_sort_string;

	if (autoOrderBy) {
		autoOrderBy = false;
		old_sort_string = wxT("");
	}
	else
		old_sort_string = GetSortCols().Trim();

	if (old_sort_string.Find(column_label) == wxNOT_FOUND)
	{
		if (old_sort_string.Len() > 0)
			new_sort_string = old_sort_string + wxT(" , ");

		new_sort_string += column_label + wxT(" DESC ");
	}
	else
	{
		if (old_sort_string.Find(column_label + wxT(" DESC")) == wxNOT_FOUND)
		{
			// Previous occurrence was for ASCENDING sort
			new_sort_string = old_sort_string;
			new_sort_string.Replace(column_label + wxT(" ASC"), column_label + wxT(" DESC"));
		}
		else
		{
			// Previous occurrence was for DESCENDING sort. Nothing to do
			new_sort_string = old_sort_string;
		}
	}

	SetSortCols(new_sort_string);

	Go();
}


void frmEditGrid::OnRemoveSort(wxCommandEvent &ev)
{
	SetSortCols(wxT(""));

	Go();
}


void frmEditGrid::OnCopy(wxCommandEvent &ev)
{
	wxWindow *wnd = FindFocus();
	if (wnd == scratchPad)
	{
		scratchPad->Copy();
	}
	else
	{
		if (editorCell->IsSet())
		{
			if (wxTheClipboard->Open())
			{
				if (sqlGrid->GetTable()->IsColText(sqlGrid->GetGridCursorCol()))
				{
					wxStyledTextCtrl *text = (wxStyledTextCtrl *)sqlGrid->GetCellEditor(sqlGrid->GetGridCursorRow(), sqlGrid->GetGridCursorCol())->GetControl();
					if (text && !text->GetSelectedText().IsEmpty())
					{
						wxTheClipboard->SetData(new wxTextDataObject(text->GetSelectedText()));
						SetStatusText(_("Data from one cell copied to clipboard."), EGSTATUSPOS_MSGS);
					}
				}
				else
				{
					wxTextCtrl *text = (wxTextCtrl *)sqlGrid->GetCellEditor(sqlGrid->GetGridCursorRow(), sqlGrid->GetGridCursorCol())->GetControl();
					if (text && !text->GetStringSelection().IsEmpty())
					{
						wxTheClipboard->SetData(new wxTextDataObject(text->GetStringSelection()));
						SetStatusText(_("Data from one cell copied to clipboard."), EGSTATUSPOS_MSGS);
					}
				}

				wxTheClipboard->Close();
			}
		}
		else if(sqlGrid->GetNumberRows() > 0)
		{
			int copied;
			copied = sqlGrid->Copy();
			SetStatusText(wxString::Format(
			                  wxPLURAL("Data from %d row copied to clipboard.", "Data from %d rows copied to clipboard.", copied),
			                  copied), EGSTATUSPOS_MSGS);
		}
	}
}


void frmEditGrid::OnPaste(wxCommandEvent &ev)
{
	wxWindow *wnd = FindFocus();
	if (wnd == scratchPad)
	{
		scratchPad->Paste();
	}
	else if (editorCell->IsSet())
	{
		if (wxTheClipboard->Open())
		{
			if (wxTheClipboard->IsSupported(wxDF_TEXT))
			{
				wxTextDataObject data;
				wxTheClipboard->GetData(data);
				wxControl *ed = sqlGrid->GetCellEditor(editorCell->GetRow(), editorCell->GetCol())->GetControl();
				if (ed->IsKindOf(CLASSINFO(wxStyledTextCtrl)))
				{
					wxStyledTextCtrl *txtEd = (wxStyledTextCtrl *)ed;
					txtEd->ReplaceSelection(data.GetText());
				}
				else if (ed->IsKindOf(CLASSINFO(wxCheckBox)))
				{
					wxCheckBox *boolEd = (wxCheckBox *)ed;

					if (data.GetText().Lower() == wxT("true"))
						boolEd->Set3StateValue(wxCHK_CHECKED);
					else if (data.GetText().Lower() == wxT("false"))
						boolEd->Set3StateValue(wxCHK_UNCHECKED);
					else
						boolEd->Set3StateValue(wxCHK_UNDETERMINED);
				}
				else if (ed->IsKindOf(CLASSINFO(wxTextCtrl)))
				{
					wxTextCtrl *txtEd = (wxTextCtrl *)ed;
					long x, y;
					txtEd->GetSelection(&x, &y);
					txtEd->Replace(x, y, data.GetText());
					//txtEd->SetValue(data.GetText());
				}
			}
			wxTheClipboard->Close();
		}
	}
	else if(sqlGrid->GetNumberRows() > 0)
	{
		if (toolBar->GetToolEnabled(MNU_SAVE))
		{
			wxMessageDialog msg(this, _("There is unsaved data in a row.\nDo you want to store to the database?"), _("Unsaved data"),
			                    wxYES_NO | wxICON_QUESTION | wxCANCEL);
			switch (msg.ShowModal())
			{
				case wxID_YES:
					if (!DoSave())
						return;
					break;

				case wxID_CANCEL:
					return;
					break;

				case wxID_NO:
					CancelChange();
					break;
			}
		}

		if (sqlGrid->GetTable()->Paste())
		{
			toolBar->EnableTool(MNU_SAVE, true);
			toolBar->EnableTool(MNU_UNDO, true);
			fileMenu->Enable(MNU_SAVE, true);
			editMenu->Enable(MNU_UNDO, true);
		}
	}
}

void frmEditGrid::OnHelp(wxCommandEvent &ev)
{
	DisplayHelp(wxT("editgrid"), HELP_PGADMIN);
}

void frmEditGrid::OnContents(wxCommandEvent &ev)
{
	DisplayHelp(wxT("index"), HELP_PGADMIN);
}

void frmEditGrid::OnKey(wxKeyEvent &event)
{
	int curcol = sqlGrid->GetGridCursorCol();
	int currow = sqlGrid->GetGridCursorRow();

	if (curcol == -1 || currow == -1)
		return;

	int keycode = event.GetKeyCode();
	wxCommandEvent ev;

	switch (keycode)
	{
		case WXK_DELETE:
		{
			if (editorCell->IsSet() || !toolBar->GetToolEnabled(MNU_DELETE))
			{
				if (!sqlGrid->IsCurrentCellReadOnly())
				{
					sqlGrid->EnableCellEditControl();
					sqlGrid->ShowCellEditControl();

					wxGridCellEditor *edit = sqlGrid->GetCellEditor(currow, curcol);
					if (edit)
					{
						wxControl *ctl = edit->GetControl();
						if (ctl)
						{
							wxStyledTextCtrl *txt = wxDynamicCast(ctl, wxStyledTextCtrl);
							if (txt)
								txt->SetText(wxEmptyString);
						}
						edit->DecRef();
					}
				}
			}
			else
			{
				OnDelete(ev);
			}
			return;
		}
		case WXK_RETURN:
		case WXK_NUMPAD_ENTER:
			// check for shift etc.
			if (event.ControlDown() || event.ShiftDown())
			{
				// Inject a RETURN into the control
				wxGridCellEditor *edit = sqlGrid->GetCellEditor(currow, curcol);
				if (edit)
				{
					wxControl *ctl = edit->GetControl();
					if (ctl)
					{
						wxStyledTextCtrl *txt = wxDynamicCast(ctl, wxStyledTextCtrl);
						if (txt)
							txt->ReplaceSelection(END_OF_LINE);
					}
					edit->DecRef();
				}
				return;
			}
			else
			{
				if( keycode != WXK_NUMPAD_ENTER )
				{
					// if we are at the end of the row
					if (curcol == sqlGrid->GetNumberCols() - 1)
					{
						// we first get to the first column of the next row
						curcol = 0;
						currow++;

						// * if the displayed object is a table,
						//   we first need to make sure that the new selected
						//   cell is read/write, otherwise we need to select
						//   the next one
						// * if the displayed object is not a table (for
						//   example, a view), all cells are readonly, so
						//   we skip that part
						if (relkind == 'r')
						{
							// locate first editable column
							while (curcol < sqlGrid->GetNumberCols() && sqlGrid->IsReadOnly(currow, curcol))
								curcol++;
							// next line is completely read-only
							if (curcol == sqlGrid->GetNumberCols())
								return;
						}
					}
					else
						curcol++;
				}
				else // ( keycode==WXK_NUMPAD_ENTER )
				{
					currow++;
				}

				OnSave(ev);
				sqlGrid->SetGridCursor(currow, curcol);

				return;
			}

		case WXK_TAB:
			if (event.ControlDown())
			{
				wxStyledTextCtrl *text = (wxStyledTextCtrl *)sqlGrid->GetCellEditor(sqlGrid->GetGridCursorRow(), sqlGrid->GetGridCursorCol())->GetControl();
				if (text)
					text->SetText(wxT("\t"));
				return;
			}

			break;

		case WXK_ESCAPE:
			CancelChange();
			break;

		default:
			if (sqlGrid->IsEditable() && keycode >= WXK_SPACE && keycode < WXK_START)
			{
				if (sqlGrid->IsCurrentCellReadOnly())
					return;

				toolBar->EnableTool(MNU_SAVE, true);
				toolBar->EnableTool(MNU_UNDO, true);
				fileMenu->Enable(MNU_SAVE, true);
				editMenu->Enable(MNU_UNDO, true);
			}
			break;
	}
	event.Skip();
}

void frmEditGrid::OnClose(wxCommandEvent &event)
{
	this->Close();
}

void frmEditGrid::OnCloseWindow(wxCloseEvent &event)
{
	// We need to call OnCellChange to check if some cells for the row have been changed
	wxGridEvent evt;
	OnCellChange(evt);

	// If MNU_SAVE item is still enabled, we need to ask about the unsaved data
	if (toolBar->GetToolEnabled(MNU_SAVE))
	{
		int flag = wxYES_NO | wxICON_QUESTION;
		if (event.CanVeto())
			flag |= wxCANCEL;

		wxMessageDialog msg(this, _("There is unsaved data in a row.\nDo you want to store to the database?"), _("Unsaved data"),
		                    flag);
		switch (msg.ShowModal())
		{
			case wxID_YES:
			{
				if (!DoSave())
				{
					event.Veto();
					return;
				}
				break;
			}
			case wxID_CANCEL:
				event.Veto();
				return;
		}
	}
	Abort();
	Destroy();
}


void frmEditGrid::OnUndo(wxCommandEvent &event)
{
	sqlGrid->DisableCellEditControl();
	sqlGrid->GetTable()->UndoLine(sqlGrid->GetGridCursorRow());
	sqlGrid->ForceRefresh();

	toolBar->EnableTool(MNU_SAVE, false);
	toolBar->EnableTool(MNU_UNDO, false);
	fileMenu->Enable(MNU_SAVE, false);
	editMenu->Enable(MNU_UNDO, false);
}


void frmEditGrid::OnRefresh(wxCommandEvent &event)
{
	if (!toolBar->GetToolEnabled(MNU_REFRESH))
		return;

	if (toolBar->GetToolEnabled(MNU_SAVE))
	{
		wxMessageDialog msg(this, _("There is unsaved data in a row.\nDo you want to store to the database?"), _("Unsaved data"),
		                    wxYES_NO | wxICON_QUESTION | wxCANCEL);
		switch (msg.ShowModal())
		{
			case wxID_YES:
			{
				if (!DoSave())
					return;
				break;
			}
			case wxID_CANCEL:
				return;
		}
	}

	sqlGrid->DisableCellEditControl();
	Go();
}


void frmEditGrid::OnSave(wxCommandEvent &event)
{
	if (sqlGrid->GetBatchCount() == 0)
		DoSave();
}

bool frmEditGrid::DoSave()
{
	sqlGrid->HideCellEditControl();
	sqlGrid->SaveEditControlValue();
	sqlGrid->DisableCellEditControl();

	if (!sqlGrid->GetTable()->StoreLine())
		return false;

	toolBar->EnableTool(MNU_SAVE, false);
	toolBar->EnableTool(MNU_UNDO, false);
	fileMenu->Enable(MNU_SAVE, false);
	editMenu->Enable(MNU_UNDO, false);

	return true;
}

void frmEditGrid::CancelChange()
{
	sqlGrid->HideCellEditControl();
	sqlGrid->SaveEditControlValue();
	sqlGrid->DisableCellEditControl();

	toolBar->EnableTool(MNU_SAVE, false);
	toolBar->EnableTool(MNU_UNDO, false);
	fileMenu->Enable(MNU_SAVE, false);
	editMenu->Enable(MNU_UNDO, false);

	sqlGrid->GetTable()->UndoLine(sqlGrid->GetGridCursorRow());
	sqlGrid->ForceRefresh();
}


void frmEditGrid::OnOptions(wxCommandEvent &event)
{
	if (toolBar->GetToolEnabled(MNU_SAVE))
	{
		wxMessageDialog msg(this, _("There is unsaved data in a row.\nDo you want to store to the database?"), _("Unsaved data"),
		                    wxYES_NO | wxICON_QUESTION | wxCANCEL);
		switch (msg.ShowModal())
		{
			case wxID_YES:
			{
				if (!DoSave())
					return;
				break;
			}
			case wxID_CANCEL:
				return;
			case wxID_NO:
				CancelChange();
		}
	}

	dlgEditGridOptions *winOptions = new dlgEditGridOptions(this, connection, tableName, sqlGrid);
	if (winOptions->ShowModal())
		Go();
}


template < class T >
int ArrayCmp(T *a, T *b)
{
	if (*a == *b)
		return 0;

	if (*a > *b)
		return 1;
	else
		return -1;
}

void frmEditGrid::OnDelete(wxCommandEvent &event)
{
	// Don't bugger about with keypresses to the scratch pad.
	if (FindFocus() == scratchPad)
	{
		event.Skip();
		return;
	}

	if (editorCell->IsSet())
	{
		if (sqlGrid->GetTable()->IsColBoolean(sqlGrid->GetGridCursorCol()))
			return;

		if (sqlGrid->GetTable()->IsColText(sqlGrid->GetGridCursorCol()))
		{
			wxStyledTextCtrl *text = (wxStyledTextCtrl *)sqlGrid->GetCellEditor(sqlGrid->GetGridCursorRow(), sqlGrid->GetGridCursorCol())->GetControl();
			if (text && text->GetCurrentPos() <= text->GetTextLength())
			{
				if (text->GetSelectionStart() == text->GetSelectionEnd())
					text->SetSelection(text->GetSelectionStart(), text->GetSelectionStart() + 1);
				text->Clear();
			}
		}
		else
		{
			wxTextCtrl *text = (wxTextCtrl *)sqlGrid->GetCellEditor(sqlGrid->GetGridCursorRow(), sqlGrid->GetGridCursorCol())->GetControl();
			if (text)
			{
				long x, y;
				text->GetSelection(&x, &y);
				if (x != y)
					text->Remove(x, x + y + 1);
				else
					text->Remove(x, x + 1);
			}
		}
		return;
	}

	// If the delete button is disabled, don't try to delete anything
	if (!toolBar->GetToolEnabled(MNU_DELETE))
		return;

	wxArrayInt delrows = sqlGrid->GetSelectedRows();
	int i = delrows.GetCount();

	if (i == 0)
		return;

	wxString prompt;
	if (i == 1)
		prompt = _("Are you sure you wish to delete the selected row?");
	else
		prompt.Printf(_("Are you sure you wish to delete the %d selected rows?"), i);

	wxMessageDialog msg(this, prompt, _("Delete rows?"), wxYES_NO | wxICON_QUESTION);
	if (msg.ShowModal() != wxID_YES)
		return;

	sqlGrid->BeginBatch();

	// Sort the grid so we always delete last->first, otherwise we
	// could end up deleting anything because the array returned by
	// GetSelectedRows is in the order that rows were selected by
	// the user.
	delrows.Sort(ArrayCmp);

	// don't care a lot about optimizing here; doing it line by line
	// just as sqlTable::DeleteRows does
	bool show_continue_message = true;
	while (i--)
	{
		if (!sqlGrid->DeleteRows(delrows.Item(i), 1) &&
		        i > 0 &&
		        show_continue_message)
		{
			wxMessageDialog msg(this, wxString::Format(wxPLURAL(
			                        "There was an error deleting the previous record.\nAre you sure you wish to delete the remaining %d row?",
			                        "There was an error deleting the previous record.\nAre you sure you wish to delete the remaining %d rows?",
			                        i), i), _("Delete more records ?"), wxYES_NO | wxICON_QUESTION);
			if (msg.ShowModal() != wxID_YES)
				break;
			else
				show_continue_message = false;
		}
	}


	sqlGrid->EndBatch();

	SetStatusTextRows(sqlGrid->GetTable()->GetNumberStoredRows());
}


void frmEditGrid::OnEditorShown(wxGridEvent &event)
{
	toolBar->EnableTool(MNU_SAVE, true);
	toolBar->EnableTool(MNU_UNDO, true);
	fileMenu->Enable(MNU_SAVE, true);
	editMenu->Enable(MNU_UNDO, true);
	editorCell->SetCell(event.GetRow(), event.GetCol());

	event.Skip();
}


void frmEditGrid::OnEditorHidden(wxGridEvent &event)
{
	editorCell->ClearCell();
}


void frmEditGrid::OnGridSelectCells(wxGridRangeSelectEvent &event)
{
	if (sqlGrid->GetEditable())
	{
		wxArrayInt rows = sqlGrid->GetSelectedRows();

		bool enable = rows.GetCount() > 0;
		if (enable)
		{
			wxCommandEvent nullEvent;
			OnSave(event);

			// check if a readonly line is selected
			int row, col;
			size_t i;

			for (i = 0 ; i < rows.GetCount() ; i++)
			{
				row = rows.Item(i);
				bool lineEnabled = false;

				if (row == sqlGrid->GetNumberRows() - 1)
				{
					// the (*) line may not be deleted/copied
					enable = false;
					break;
				}
				for (col = 0 ; col < sqlGrid->GetNumberCols() ; col++)
				{
					if (!sqlGrid->IsReadOnly(row, col))
					{
						lineEnabled = true;
						break;
					}
				}

				if (!lineEnabled)
				{
					enable = false;
					break;
				}
			}
		}
		toolBar->EnableTool(MNU_DELETE, enable);
		editMenu->Enable(MNU_DELETE, enable);
	}
	event.Skip();
}


void frmEditGrid::ShowForm(bool filter)
{
	bool abort = false;

	if (relkind == 'r' || relkind == 'v' || relkind == 'x' || relkind == 'f')
	{
		if (filter)
		{
			dlgEditGridOptions *winOptions = new dlgEditGridOptions(this, connection, tableName, sqlGrid);
			abort = !(winOptions->ShowModal());
		}
		if (abort)
		{
			// Hack to ensure there's a table for ~wxGrid() to delete
			sqlGrid->CreateGrid(0, 0);
			sqlGrid->SetTable(0);
			Close();
			Destroy();
		}
		else
		{
			Show(true);
			Go();
		}
	}
	else
	{
		wxLogError(__("No Table or view."));
		// Hack to ensure there's a table for ~wxGrid() to delete
		sqlGrid->CreateGrid(0, 0);
		Close();
		Destroy();
	}
}

void frmEditGrid::Go()
{
	long templong;

	if (cbLimit->GetValue() != wxT("") &&
	        cbLimit->GetValue() != _("No limit") &&
	        !cbLimit->GetValue().BeforeFirst(' ').ToLong(&templong))
	{
		wxLogError(_("The row limit must be an integer number or 'No limit'"));
		return;
	}

	if (cbLimit->GetValue() == _("No limit"))
		SetLimit(0);
	else
	{
		cbLimit->GetValue().BeforeFirst(' ').ToLong(&templong);
		SetLimit(templong);
	}

	// Check we have access
	if (connection->ExecuteScalar(wxT("SELECT count(*) FROM ") + tableName + wxT(" WHERE false")) == wxT(""))
		return;

	SetStatusText(_(""), EGSTATUSPOS_ROWS);
	SetStatusText(_("Refreshing data, please wait."), EGSTATUSPOS_MSGS);

	toolBar->EnableTool(MNU_REFRESH, false);
	viewMenu->Enable(MNU_REFRESH, false);
	toolBar->EnableTool(MNU_OPTIONS, false);
	toolsMenu->Enable(MNU_OPTIONS, false);
	toolsMenu->Enable(MNU_INCLUDEFILTER, false);
	toolsMenu->Enable(MNU_EXCLUDEFILTER, false);
	toolsMenu->Enable(MNU_REMOVEFILTERS, false);
	toolsMenu->Enable(MNU_ASCSORT, false);
	toolsMenu->Enable(MNU_DESCSORT, false);
	toolsMenu->Enable(MNU_REMOVESORT, false);

	wxString qry = wxT("SELECT ");
	if (hasOids)
		qry += wxT("oid, ");
	qry += wxT("* FROM ") + tableName;
	if (!rowFilter.IsEmpty())
	{
		qry += wxT(" WHERE ") + rowFilter;
	}
	if (!orderBy.IsEmpty())
	{
		qry += wxT("\n ORDER BY ") + orderBy;
	}
	if (limit > 0)
		qry += wxT(" LIMIT ") + wxString::Format(wxT("%i"), limit);

	thread = new pgQueryThread(connection, qry);
	if (thread->Create() != wxTHREAD_NO_ERROR)
	{
		Abort();
		toolBar->EnableTool(MNU_REFRESH, true);
		viewMenu->Enable(MNU_REFRESH, true);
		toolBar->EnableTool(MNU_OPTIONS, true);
		toolsMenu->Enable(MNU_OPTIONS, true);
		toolsMenu->Enable(MNU_INCLUDEFILTER, true);
		toolsMenu->Enable(MNU_EXCLUDEFILTER, true);
		toolsMenu->Enable(MNU_REMOVEFILTERS, true);
		toolsMenu->Enable(MNU_ASCSORT, true);
		toolsMenu->Enable(MNU_DESCSORT, true);
		toolsMenu->Enable(MNU_REMOVESORT, true);

		return;
	}

	thread->Run();

	while (thread && thread->IsRunning())
	{
		wxTheApp->Yield(true);
		wxMilliSleep(10);
	}

	// Brute force check to ensure the user didn't get bored and close the window
	if (closing)
		return;

	if (!thread)
	{
		toolBar->EnableTool(MNU_REFRESH, true);
		viewMenu->Enable(MNU_REFRESH, true);
		toolBar->EnableTool(MNU_OPTIONS, true);
		toolsMenu->Enable(MNU_OPTIONS, true);
		toolsMenu->Enable(MNU_INCLUDEFILTER, true);
		toolsMenu->Enable(MNU_EXCLUDEFILTER, true);
		toolsMenu->Enable(MNU_REMOVEFILTERS, true);
		toolsMenu->Enable(MNU_ASCSORT, true);
		toolsMenu->Enable(MNU_DESCSORT, true);
		toolsMenu->Enable(MNU_REMOVESORT, true);
		return;
	}

	if (!thread->DataValid())
	{
		Abort();
		toolBar->EnableTool(MNU_REFRESH, true);
		viewMenu->Enable(MNU_REFRESH, true);
		toolBar->EnableTool(MNU_OPTIONS, true);
		toolsMenu->Enable(MNU_OPTIONS, true);
		toolsMenu->Enable(MNU_INCLUDEFILTER, true);
		toolsMenu->Enable(MNU_EXCLUDEFILTER, true);
		toolsMenu->Enable(MNU_REMOVEFILTERS, true);
		toolsMenu->Enable(MNU_ASCSORT, true);
		toolsMenu->Enable(MNU_DESCSORT, true);
		toolsMenu->Enable(MNU_REMOVESORT, true);
		return;
	}

	SetStatusTextRows((int)thread->DataSet()->NumRows());
	SetStatusText(_("OK"), EGSTATUSPOS_MSGS);

	sqlGrid->BeginBatch();

	// to force the grid to create scrollbars, we make sure the size  so small that scrollbars are needed
	// later, we will resize the grid's parent to force the correct size (now including scrollbars, even if
	// they are suppressed initially. Win32 won't need this.
	// !!! This hack breaks columns auto-sizing ( GetClientSize().GetWidth() is used in ctlSQLGrid::AutoSizeColumns() )
	// !!! Is it still required?
	//sqlGrid->SetSize(10, 10);

	sqlGrid->SetTable(new sqlTable(connection, thread, tableName, relid, hasOids, primaryKeyColNumbers, relkind), true);
	sqlGrid->AutoSizeColumns(false);

	sqlGrid->EndBatch();

	toolBar->EnableTool(MNU_REFRESH, true);
	viewMenu->Enable(MNU_REFRESH, true);
	toolBar->EnableTool(MNU_OPTIONS, true);
	toolsMenu->Enable(MNU_OPTIONS, true);
	toolsMenu->Enable(MNU_INCLUDEFILTER, true);
	toolsMenu->Enable(MNU_EXCLUDEFILTER, true);
	toolsMenu->Enable(MNU_REMOVEFILTERS, true);
	toolsMenu->Enable(MNU_ASCSORT, true);
	toolsMenu->Enable(MNU_DESCSORT, true);
	toolsMenu->Enable(MNU_REMOVESORT, true);

	manager.Update();

	if (!hasOids && primaryKeyColNumbers.IsEmpty() && relkind == 'r')
		frmHint::ShowHint(this, HINT_READONLY_NOPK, tableName);

	// Set the thread variable to zero so we don't try to
	// abort it if the user cancels now.
	thread = 0;
}


frmEditGrid::~frmEditGrid()
{
	closing = true;

	mainForm->RemoveFrame(this);

	settings->Write(wxT("frmEditGrid/Perspective-") + wxString(FRMEDITGRID_PERSPECTIVE_VER), manager.SavePerspective());
	manager.UnInit();

	if (connection)
		delete connection;
}


void frmEditGrid::Abort()
{
	if (sqlGrid->GetTable())
	{
		sqlGrid->HideCellEditControl();
		sqlGrid->SetTable(0);
	}

	if (thread)
	{
		SetStatusText(_("aborting."), EGSTATUSPOS_MSGS);
		if (thread->IsRunning())
		{
			thread->CancelExecution();
			thread->Wait();
		}
		delete thread;
		thread = 0;
	}
}


ctlSQLEditGrid::ctlSQLEditGrid(wxFrame *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
	: ctlSQLGrid(parent, id, pos, size)
{
}

bool ctlSQLEditGrid::CheckRowPresent(int row)
{
	return GetTable()->CheckInCache(row);
}

void ctlSQLEditGrid::ResizeEditor(int row, int col)
{

	if (GetTable()->needsResizing(col))
	{
		wxGridCellAttr *attr = GetCellAttr(row, col);
		wxGridCellRenderer *renderer = attr->GetRenderer(this, row, col);
		if ( renderer )
		{
			wxClientDC dc(GetGridWindow());
			wxSize size = renderer->GetBestSize(*this, *attr, dc, row, col);
			renderer->DecRef();

			int w = wxMax(size.GetWidth(), 15) + 20;
			int h = wxMax(size.GetHeight(), 15) + 20;


			wxGridCellEditor *editor = attr->GetEditor(this, row, col);
			if (editor)
			{
				wxRect cellRect = CellToRect(m_currentCellCoords);
				wxRect rect = cellRect;
				rect.SetWidth(w);
				rect.SetHeight(h);

				// we might have scrolled
				CalcUnscrolledPosition(0, 0, &w, &h);
				rect.SetLeft(rect.GetLeft() - w);
				rect.SetTop(rect.GetTop() - h);

				// Clip rect to client size
				GetClientSize(&w, &h);
				rect.SetRight(wxMin(rect.GetRight(), w));
				rect.SetBottom(wxMin(rect.GetBottom(), h));

				// but not smaller than original cell
				rect.SetWidth(wxMax(cellRect.GetWidth(), rect.GetWidth()));
				rect.SetHeight(wxMax(cellRect.GetHeight(), rect.GetHeight()));

				editor->SetSize(rect);
				editor->DecRef();
			}
		}

		attr->DecRef();
	}
}

wxArrayInt ctlSQLEditGrid::GetSelectedRows() const
{
	wxArrayInt rows, rows2;

	wxGridCellCoordsArray tl = GetSelectionBlockTopLeft(), br = GetSelectionBlockBottomRight();

	int maxCol = ((ctlSQLEditGrid *)this)->GetNumberCols() - 1;
	size_t i;
	for (i = 0 ; i < tl.GetCount() ; i++)
	{
		wxGridCellCoords c1 = tl.Item(i), c2 = br.Item(i);
		if (c1.GetCol() != 0 || c2.GetCol() != maxCol)
			return rows2;

		int j;
		for (j = c1.GetRow() ; j <= c2.GetRow() ; j++)
			rows.Add(j);
	}

	rows2 = wxGrid::GetSelectedRows();

	rows.Sort(ArrayCmp);
	rows2.Sort(ArrayCmp);

	size_t i2 = 0, cellRowMax = rows.GetCount();

	for (i = 0 ; i < rows2.GetCount() ; i++)
	{
		int row = rows2.Item(i);
		while (i2 < cellRowMax && rows.Item(i2) < row)
			i2++;
		if (i2 == cellRowMax || row != rows.Item(i2))
			rows.Add(row);
	}

	return rows;
}




bool ctlSQLEditGrid::IsColText(int col)
{
	return GetTable()->IsColText(col);
}



bool editGridFactoryBase::CheckEnable(pgObject *obj)
{
	if (obj)
	{
		pgaFactory *factory = obj->GetFactory();
		return factory == &tableFactory || factory == &foreignTableFactory || factory == &viewFactory || factory == &extTableFactory || factory == &catalogObjectFactory;
	}
	return false;
}


wxWindow *editGridFactoryBase::ViewData(frmMain *form, pgObject *obj, bool filter)
{
	pgDatabase *db = ((pgSchemaObject *)obj)->GetDatabase();
	wxString applicationname = appearanceFactory->GetLongAppName() + _(" - Edit Grid");

	pgServer *server = db->GetServer();
	pgConn *conn = db->CreateConn(applicationname);
	if (conn)
	{
		wxString txt = _("Edit Data - ")
		               + server->GetDescription()
		               + wxT(" (") + server->GetName()
		               + wxT(":") + NumToStr((long)server->GetPort())
		               + wxT(") - ") + db->GetName()
		               + wxT(" - ") + obj->GetFullIdentifier();

		frmEditGrid *eg = new frmEditGrid(form, txt, conn, (pgSchemaObject *)obj, pkAscending);
		eg->SetLimit(rowlimit);
		eg->ShowForm(filter);
		return eg;
	}
	return 0;
}


editGridFactory::editGridFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : editGridFactoryBase(list)
{
	mnu->Append(id, _("View &All Rows\tCtrl-D"), _("View the data in the selected object."));
	toolbar->AddTool(id, _("View All Rows\tCtrl-D"), *viewdata_png_bmp, _("View the data in the selected object."), wxITEM_NORMAL);
	context = false;
}


wxWindow *editGridFactory::StartDialog(frmMain *form, pgObject *obj)
{
	return ViewData(form, obj, false);
}


#include "images/viewfiltereddata.pngc"
editGridFilteredFactory::editGridFilteredFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : editGridFactoryBase(list)
{
	mnu->Append(id, _("View F&iltered Rows...\tCtrl-G"), _("Apply a filter and view the data in the selected object."));
	toolbar->AddTool(id, _("View Filtered Rows\tCtrl-G"), *viewfiltereddata_png_bmp, _("Apply a filter and view the data in the selected object."), wxITEM_NORMAL);
	context = false;
}


wxWindow *editGridFilteredFactory::StartDialog(frmMain *form, pgObject *obj)
{
	return ViewData(form, obj, true);
}

editGridLimitedFactory::editGridLimitedFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar, int limit, bool ascending) : editGridFactoryBase(list)
{
	if (ascending)
		mnu->Append(id, wxString::Format(wxPLURAL("View Top %i Row", "View Top %i Rows", limit), limit), _("View a limited number of rows in the selected object."));
	else
		mnu->Append(id, wxString::Format(wxPLURAL("View Last %i Row", "View Last %i Rows", limit), limit), _("View a limited number of rows in the selected object."));

	pkAscending = ascending;
	rowlimit = limit;
	context = false;
}

wxWindow *editGridLimitedFactory::StartDialog(frmMain *form, pgObject *obj)
{
	return ViewData(form, obj, false);
}
