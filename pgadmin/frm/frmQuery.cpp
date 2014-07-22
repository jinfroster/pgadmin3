//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// frmQuery.cpp - SQL Query Box
//
//////////////////////////////////////////////////////////////////////////

#include "pgAdmin3.h"

// wxWindows headers
#include <wx/wx.h>
#include <wx/busyinfo.h>
#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/dnd.h>
#include <wx/filename.h>
#include <wx/regex.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/aui/aui.h>
#include <wx/bmpcbox.h>
#include <wx/filefn.h>

// App headers
#include "frm/frmAbout.h"
#include "frm/frmMain.h"
#include "frm/frmQuery.h"
#include "frm/menu.h"
#include "ctl/explainCanvas.h"
#include "db/pgConn.h"

#include "ctl/ctlMenuToolbar.h"
#include "ctl/ctlSQLResult.h"
#include "dlg/dlgSelectConnection.h"
#include "dlg/dlgAddFavourite.h"
#include "dlg/dlgManageFavourites.h"
#include "dlg/dlgManageMacros.h"
#include "frm/frmReport.h"
#include "gqb/gqbViewController.h"
#include "gqb/gqbModel.h"
#include "gqb/gqbViewPanels.h"
#include "gqb/gqbEvents.h"
#include "schema/pgDatabase.h"
#include "schema/pgFunction.h"
#include "schema/pgTable.h"
#include "schema/pgForeignTable.h"
#include "schema/pgView.h"
#include "schema/gpExtTable.h"
#include "schema/pgServer.h"
#include "utils/favourites.h"
#include "utils/sysLogger.h"
#include "utils/sysSettings.h"
#include "utils/utffile.h"
#include "pgscript/pgsApplication.h"

// Icons
#include "images/sql-32.pngc"

// Bitmaps
#include "images/file_new.pngc"
#include "images/file_open.pngc"
#include "images/file_save.pngc"
#include "images/clip_cut.pngc"
#include "images/clip_copy.pngc"
#include "images/clip_paste.pngc"
#include "images/edit_clear.pngc"
#include "images/edit_find.pngc"
#include "images/edit_undo.pngc"
#include "images/edit_redo.pngc"
#include "images/query_execute.pngc"
#include "images/query_pgscript.pngc"
#include "images/query_execfile.pngc"
#include "images/query_explain.pngc"
#include "images/query_cancel.pngc"
#include "images/help.pngc"
#include "images/gqbJoin.pngc"

#define CTRLID_CONNECTION       4200
#define CTRLID_DATABASELABEL    4201

#define XML_FROM_WXSTRING(s) ((const xmlChar *)(const char *)s.mb_str(wxConvUTF8))
#define WXSTRING_FROM_XML(s) wxString((char *)s, wxConvUTF8)
#define XML_STR(s) ((const xmlChar *)s)

// Initialize execution 'mutex'. As this will always run in the
// main thread, there aren't any real concurrency issues, so
// a simple flag will suffice.
// Required because the pgScript parser isn't currently thread-safe :-(
bool    frmQuery::ms_pgScriptRunning = false;

BEGIN_EVENT_TABLE(frmQuery, pgFrame)
	EVT_ERASE_BACKGROUND(           frmQuery::OnEraseBackground)
	EVT_SIZE(                       frmQuery::OnSize)
	EVT_COMBOBOX(CTRLID_CONNECTION, frmQuery::OnChangeConnection)
	EVT_COMBOBOX(CTL_SQLQUERYCBOX,  frmQuery::OnChangeQuery)
	EVT_CLOSE(                      frmQuery::OnClose)
	EVT_SET_FOCUS(                  frmQuery::OnSetFocus)
	EVT_MENU(MNU_NEW,               frmQuery::OnNew)
	EVT_MENU(MNU_OPEN,              frmQuery::OnOpen)
	EVT_MENU(MNU_SAVE,              frmQuery::OnSave)
	EVT_MENU(MNU_SAVEAS,            frmQuery::OnSaveAs)
	EVT_MENU(MNU_EXPORT,            frmQuery::OnExport)
	EVT_MENU(MNU_SAVEAS_IMAGE_GQB,     frmQuery::SaveExplainAsImage)
	EVT_MENU(MNU_SAVEAS_IMAGE_EXPLAIN, frmQuery::SaveExplainAsImage)
	EVT_MENU(MNU_EXIT,              frmQuery::OnExit)
	EVT_MENU(MNU_CUT,               frmQuery::OnCut)
	EVT_MENU(MNU_COPY,              frmQuery::OnCopy)
	EVT_MENU(MNU_PASTE,             frmQuery::OnPaste)
	EVT_MENU(MNU_CLEAR,             frmQuery::OnClear)
	EVT_MENU(MNU_FIND,              frmQuery::OnSearchReplace)
	EVT_MENU(MNU_UNDO,              frmQuery::OnUndo)
	EVT_MENU(MNU_REDO,              frmQuery::OnRedo)
	EVT_MENU(MNU_EXECUTE,           frmQuery::OnExecute)
	EVT_MENU(MNU_EXECPGS,           frmQuery::OnExecScript)
	EVT_MENU(MNU_EXECFILE,          frmQuery::OnExecFile)
	EVT_MENU(MNU_EXPLAIN,           frmQuery::OnExplain)
	EVT_MENU(MNU_EXPLAINANALYZE,    frmQuery::OnExplain)
	EVT_MENU(MNU_CANCEL,            frmQuery::OnCancel)
	EVT_MENU(MNU_AUTOROLLBACK,      frmQuery::OnAutoRollback)
	EVT_MENU(MNU_CONTENTS,          frmQuery::OnContents)
	EVT_MENU(MNU_HELP,              frmQuery::OnHelp)
	EVT_MENU(MNU_CLEARHISTORY,      frmQuery::OnClearHistory)
	EVT_MENU(MNU_SAVEHISTORY,       frmQuery::OnSaveHistory)
	EVT_MENU(MNU_SELECTALL,         frmQuery::OnSelectAll)
	EVT_MENU(MNU_QUICKREPORT,       frmQuery::OnQuickReport)
	EVT_MENU(MNU_AUTOINDENT,        frmQuery::OnAutoIndent)
	EVT_MENU(MNU_WORDWRAP,          frmQuery::OnWordWrap)
	EVT_MENU(MNU_SHOWINDENTGUIDES,  frmQuery::OnShowIndentGuides)
	EVT_MENU(MNU_SHOWWHITESPACE,    frmQuery::OnShowWhitespace)
	EVT_MENU(MNU_SHOWLINEENDS,      frmQuery::OnShowLineEnds)
	EVT_MENU(MNU_SHOWLINENUMBER,    frmQuery::OnShowLineNumber)
	EVT_MENU(MNU_FAVOURITES_ADD,    frmQuery::OnAddFavourite)
	EVT_MENU(MNU_FAVOURITES_INJECT, frmQuery::OnInjectFavourite)
	EVT_MENU(MNU_FAVOURITES_MANAGE, frmQuery::OnManageFavourites)
	EVT_MENU(MNU_MACROS_MANAGE,     frmQuery::OnMacroManage)
	EVT_MENU(MNU_DATABASEBAR,       frmQuery::OnToggleDatabaseBar)
	EVT_MENU(MNU_TOOLBAR,           frmQuery::OnToggleToolBar)
	EVT_MENU(MNU_SCRATCHPAD,        frmQuery::OnToggleScratchPad)
	EVT_MENU(MNU_OUTPUTPANE,        frmQuery::OnToggleOutputPane)
	EVT_MENU(MNU_DEFAULTVIEW,       frmQuery::OnDefaultView)
	EVT_MENU(MNU_BLOCK_INDENT,      frmQuery::OnBlockIndent)
	EVT_MENU(MNU_BLOCK_OUTDENT,     frmQuery::OnBlockOutDent)
	EVT_MENU(MNU_UPPER_CASE,        frmQuery::OnChangeToUpperCase)
	EVT_MENU(MNU_LOWER_CASE,        frmQuery::OnChangeToLowerCase)
	EVT_MENU(MNU_COMMENT_TEXT,      frmQuery::OnCommentText)
	EVT_MENU(MNU_UNCOMMENT_TEXT,    frmQuery::OnUncommentText)
	EVT_MENU(MNU_EXTERNALFORMAT,    frmQuery::OnExternalFormat)
	EVT_MENU(MNU_LF,                frmQuery::OnSetEOLMode)
	EVT_MENU(MNU_CRLF,              frmQuery::OnSetEOLMode)
	EVT_MENU(MNU_CR,                frmQuery::OnSetEOLMode)
	EVT_MENU_RANGE(MNU_FAVOURITES_MANAGE + 1, MNU_FAVOURITES_MANAGE + 999, frmQuery::OnSelectFavourite)
	EVT_MENU_RANGE(MNU_MACROS_MANAGE + 1, MNU_MACROS_MANAGE + 99, frmQuery::OnMacroInvoke)
	EVT_ACTIVATE(                   frmQuery::OnActivate)
	EVT_STC_MODIFIED(CTL_SQLQUERY,  frmQuery::OnChangeStc)
	EVT_STC_UPDATEUI(CTL_SQLQUERY,  frmQuery::OnPositionStc)
	EVT_AUI_PANE_CLOSE(             frmQuery::OnAuiUpdate)
	EVT_TIMER(CTL_TIMERSIZES,       frmQuery::OnAdjustSizesTimer)
	EVT_TIMER(CTL_TIMERFRM,         frmQuery::OnTimer)
// These fire when the queries complete
	EVT_PGQUERYRESULT(QUERY_COMPLETE, frmQuery::OnQueryComplete)
	EVT_MENU(PGSCRIPT_COMPLETE,     frmQuery::OnScriptComplete)
	EVT_AUINOTEBOOK_PAGE_CHANGED(CTL_NTBKCENTER, frmQuery::OnChangeNotebook)
	EVT_SPLITTER_SASH_POS_CHANGED(GQB_HORZ_SASH, frmQuery::OnResizeHorizontally)
	EVT_BUTTON(CTL_DELETECURRENTBTN, frmQuery::OnDeleteCurrent)
	EVT_BUTTON(CTL_DELETEALLBTN,     frmQuery::OnDeleteAll)
END_EVENT_TABLE()

class DnDFile : public wxFileDropTarget
{
public:
	DnDFile(frmQuery *fquery)
	{
		m_fquery = fquery;
	}

	virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames)
	{
		size_t nFiles = filenames.GetCount();
		if ((int) nFiles > 1)
			wxLogError(_("Drag one file at a time"));
		else if ((int) nFiles == 1)
		{
			wxString str;
			bool modeUnicode = settings->GetUnicodeFile();
			wxUtfFile file(filenames[0], wxFile::read, modeUnicode ? wxFONTENCODING_UTF8 : wxFONTENCODING_DEFAULT);

			if (file.IsOpened())
				file.Read(str);

			if (!str.IsEmpty() && !m_fquery->CheckChanged(true))
			{
				m_fquery->SetLastPath(filenames[0]);
				m_fquery->SetQueryText(str);
				m_fquery->ColouriseQuery(0, str.Length());
				wxSafeYield();                            // needed to process sqlQuery modify event
				m_fquery->SetChanged(false);
				m_fquery->SetOrigin(ORIGIN_FILE);
				m_fquery->setExtendedTitle();
				m_fquery->SetLineEndingStyle();
				m_fquery->UpdateRecentFiles(true);
				m_fquery->UpdateAllRecentFiles();
			}
		}
		return true;
	}

private:
	frmQuery *m_fquery;
};


frmQuery::frmQuery(frmMain *form, const wxString &_title, pgConn *_conn, const wxString &query, const wxString &file)
	: pgFrame(NULL, _title),
	  timer(this, CTL_TIMERFRM),
	  pgScript(new pgsApplication(_conn)),
	  pgsStringOutput(&pgsOutputString),
	  pgsOutput(pgsStringOutput, wxEOL_UNIX),
	  pgsTimer(new pgScriptTimer(this)),
	  m_loadingfile(false)
{
	pgScript->SetCaller(this, PGSCRIPT_COMPLETE);

	mainForm = form;
	conn = _conn;

	loading = true;
	closing = false;
	origin = ORIGIN_MANUAL;

	dlgName = wxT("frmQuery");
	recentKey = wxT("RecentFiles");
	RestorePosition(100, 100, 600, 500, 450, 300);

	explainCanvas = NULL;

	// notify wxAUI which frame to use
	manager.SetManagedWindow(this);
	manager.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_TRANSPARENT_DRAG);

	SetMinSize(wxSize(450, 300));

	SetIcon(*sql_32_png_ico);
	SetFont(settings->GetSystemFont());
	menuBar = new wxMenuBar();

	fileMenu = new wxMenu();
	recentFileMenu = new wxMenu();
	fileMenu->Append(MNU_NEW, _("&New window\tCtrl-N"), _("Open a new query window"));
	fileMenu->Append(MNU_OPEN, _("&Open...\tCtrl-O"),   _("Open a query file"));
	fileMenu->Append(MNU_SAVE, _("&Save\tCtrl-S"),      _("Save current file"));
	saveasImageMenu = new wxMenu();
	saveasImageMenu->Append(MNU_SAVEAS, _("Query (text)"), _("Save file under new name"));
	saveasImageMenu->Append(MNU_SAVEAS_IMAGE_GQB, _("Graphical Query (image)"), _("Save Graphical Query as an image"));
	saveasImageMenu->Append(MNU_SAVEAS_IMAGE_EXPLAIN, _("Explain (image)"), _("Save output of Explain as an image"));
	fileMenu->Append(wxID_ANY, _("Save as"), saveasImageMenu);
	fileMenu->AppendSeparator();
	fileMenu->Append(MNU_EXPORT, _("&Export..."),  _("Export data to file"));
	fileMenu->Append(MNU_QUICKREPORT, _("&Quick report..."),  _("Run a quick report..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(MNU_RECENT, _("&Recent files"), recentFileMenu);
	fileMenu->Append(MNU_EXIT, _("E&xit\tCtrl-W"), _("Exit query window"));

	menuBar->Append(fileMenu, _("&File"));

	lineEndMenu = new wxMenu();
	lineEndMenu->AppendRadioItem(MNU_LF, _("Unix (LF)"), _("Use Unix style line endings"));
	lineEndMenu->AppendRadioItem(MNU_CRLF, _("DOS (CRLF)"), _("Use DOS style line endings"));
	lineEndMenu->AppendRadioItem(MNU_CR, _("Mac (CR)"), _("Use Mac style line endings"));

	editMenu = new wxMenu();
	editMenu->Append(MNU_UNDO, _("&Undo\tCtrl-Z"), _("Undo last action"), wxITEM_NORMAL);
	editMenu->Append(MNU_REDO, _("&Redo\tCtrl-Y"), _("Redo last action"), wxITEM_NORMAL);
	editMenu->AppendSeparator();
	editMenu->Append(MNU_CUT, _("Cu&t\tCtrl-X"), _("Cut selected text to clipboard"), wxITEM_NORMAL);
	editMenu->Append(MNU_COPY, _("&Copy\tCtrl-C"), _("Copy selected text to clipboard"), wxITEM_NORMAL);
	editMenu->Append(MNU_PASTE, _("&Paste\tCtrl-V"), _("Paste selected text from clipboard"), wxITEM_NORMAL);
	editMenu->Append(MNU_CLEAR, _("C&lear window"), _("Clear edit window"), wxITEM_NORMAL);
	editMenu->AppendSeparator();
	editMenu->Append(MNU_FIND, _("&Find and Replace\tCtrl-F"), _("Find and replace text"), wxITEM_NORMAL);
	editMenu->AppendSeparator();
	editMenu->Append(MNU_AUTOINDENT, _("&Auto indent"), _("Automatically indent text to the same level as the preceding line"), wxITEM_CHECK);

	//  editMenu->AppendSeparator();
	formatMenu = new wxMenu();
	formatMenu->Append(MNU_UPPER_CASE, _("&Upper case\tCtrl-U"), _("Change the selected text to upper case"));
	formatMenu->Append(MNU_LOWER_CASE, _("&Lower case\tCtrl-Shift-U"), _("Change the selected text to lower case"));
	formatMenu->AppendSeparator();
	formatMenu->Append(MNU_BLOCK_INDENT, _("Block &Indent\tTab"), _("Indent the selected block"));
	formatMenu->Append(MNU_BLOCK_OUTDENT, _("Block &Outdent\tShift-Tab"), _("Outdent the selected block"));
	formatMenu->Append(MNU_COMMENT_TEXT, _("Co&mment Text\tCtrl-K"), _("Comment out the selected text"));
	formatMenu->Append(MNU_UNCOMMENT_TEXT, _("Uncomme&nt Text\tCtrl-Shift-K"), _("Uncomment the selected text"));
	formatMenu->AppendSeparator();
	formatMenu->Append(MNU_EXTERNALFORMAT, _("External Format\tCtrl-Shift-F"), _("Call external format tool"));
	editMenu->AppendSubMenu(formatMenu, _("F&ormat"));
	editMenu->Append(MNU_LINEENDS, _("&Line ends"), lineEndMenu);

	menuBar->Append(editMenu, _("&Edit"));

	queryMenu = new wxMenu();
	queryMenu->Append(MNU_EXECUTE, _("&Execute\tF5"), _("Execute query"));
	queryMenu->Append(MNU_EXECPGS, _("Execute &pgScript\tF6"), _("Execute pgScript"));
	queryMenu->Append(MNU_EXECFILE, _("Execute to file"), _("Execute query, write result to file"));
	queryMenu->Append(MNU_EXPLAIN, _("E&xplain\tF7"), _("Explain query"));
	queryMenu->Append(MNU_EXPLAINANALYZE, _("Explain analyze\tShift-F7"), _("Explain and analyze query"));


	wxMenu *eo = new wxMenu();
	eo->Append(MNU_VERBOSE, _("Verbose"), _("Explain verbose query"), wxITEM_CHECK);
	eo->Append(MNU_COSTS, _("Costs"), _("Explain analyze query with (or without) costs"), wxITEM_CHECK);
	eo->Append(MNU_BUFFERS, _("Buffers"), _("Explain analyze query with (or without) buffers"), wxITEM_CHECK);
	eo->Append(MNU_TIMING, _("Timing"), _("Explain analyze query with (or without) timing"), wxITEM_CHECK);
	queryMenu->Append(MNU_EXPLAINOPTIONS, _("Explain &options"), eo, _("Options modifying Explain output"));
	queryMenu->AppendSeparator();
	queryMenu->Append(MNU_SAVEHISTORY, _("Save history"), _("Save history of executed commands."));
	queryMenu->Append(MNU_CLEARHISTORY, _("Clear history"), _("Clear history window."));
	queryMenu->AppendSeparator();
	queryMenu->Append(MNU_AUTOROLLBACK, _("&Auto-Rollback"), _("Rollback the current transaction if an error is detected"), wxITEM_CHECK);
	queryMenu->AppendSeparator();
	queryMenu->Append(MNU_CANCEL, _("&Cancel\tAlt-Break"), _("Cancel query"));
	menuBar->Append(queryMenu, _("&Query"));

	favouritesMenu = new wxMenu();
	favouritesMenu->Append(MNU_FAVOURITES_ADD, _("Add favourite..."), _("Add current query to favourites"));
	favouritesMenu->Append(MNU_FAVOURITES_INJECT, _("Inject\tF2"), _("Replace a word under cursor with a favourite with same name"));
	favouritesMenu->Append(MNU_FAVOURITES_MANAGE, _("Manage favourites..."), _("Edit and delete favourites"));
	favouritesMenu->AppendSeparator();
	favourites = 0L;
	UpdateFavouritesList();
	menuBar->Append(favouritesMenu, _("Fav&ourites"));

	macrosMenu = new wxMenu();
	macrosMenu->Append(MNU_MACROS_MANAGE, _("Manage macros..."), _("Edit and delete macros"));
	macrosMenu->AppendSeparator();
	macros = 0L;
	UpdateMacrosList();
	menuBar->Append(macrosMenu, _("&Macros"));

	// View menu
	viewMenu = new wxMenu();
	viewMenu->Append(MNU_DATABASEBAR, _("&Connection bar\tCtrl-Alt-B"), _("Show or hide the database selection bar."), wxITEM_CHECK);
	viewMenu->Append(MNU_OUTPUTPANE, _("&Output pane\tCtrl-Alt-O"), _("Show or hide the output pane."), wxITEM_CHECK);
	viewMenu->Append(MNU_SCRATCHPAD, _("S&cratch pad\tCtrl-Alt-S"), _("Show or hide the scratch pad."), wxITEM_CHECK);
	viewMenu->Append(MNU_TOOLBAR, _("&Tool bar\tCtrl-Alt-T"), _("Show or hide the tool bar."), wxITEM_CHECK);
	viewMenu->AppendSeparator();
	viewMenu->Append(MNU_SHOWINDENTGUIDES, _("&Indent guides"), _("Enable or disable display of indent guides"), wxITEM_CHECK);
	viewMenu->Append(MNU_SHOWLINEENDS, _("&Line ends"), _("Enable or disable display of line ends"), wxITEM_CHECK);
	viewMenu->Append(MNU_SHOWWHITESPACE, _("&Whitespace"), _("Enable or disable display of whitespaces"), wxITEM_CHECK);
	viewMenu->Append(MNU_WORDWRAP, _("&Word wrap"), _("Enable or disable word wrapping"), wxITEM_CHECK);
	viewMenu->Append(MNU_SHOWLINENUMBER, _("&Line number"), _("Enable or disable display of line number"), wxITEM_CHECK);
	viewMenu->AppendSeparator();
	viewMenu->Append(MNU_DEFAULTVIEW, _("&Default view\tCtrl-Alt-V"),     _("Restore the default view."));

	menuBar->Append(viewMenu, _("&View"));

	wxMenu *helpMenu = new wxMenu();
	helpMenu->Append(MNU_CONTENTS, _("&Help"),                 _("Open the helpfile."));
	helpMenu->Append(MNU_HELP, _("&SQL Help\tF1"),                _("Display help on SQL commands."));

#ifdef __WXMAC__
	menuFactories = new menuFactoryList();
	aboutFactory *af = new aboutFactory(menuFactories, helpMenu, 0);
	wxApp::s_macAboutMenuItemId = af->GetId();
	menuFactories->RegisterMenu(this, wxCommandEventHandler(pgFrame::OnAction));
#endif

	menuBar->Append(helpMenu, _("&Help"));

	SetMenuBar(menuBar);

	queryMenu->Check(MNU_VERBOSE, settings->GetExplainVerbose());
	queryMenu->Check(MNU_COSTS, settings->GetExplainCosts());
	queryMenu->Check(MNU_BUFFERS, settings->GetExplainBuffers());
	queryMenu->Check(MNU_TIMING, settings->GetExplainTiming());

	UpdateRecentFiles();

	wxAcceleratorEntry entries[14];

	entries[0].Set(wxACCEL_CTRL,                (int)'E',      MNU_EXECUTE);
	entries[1].Set(wxACCEL_CTRL,                (int)'O',      MNU_OPEN);
	entries[2].Set(wxACCEL_CTRL,                (int)'S',      MNU_SAVE);
	entries[3].Set(wxACCEL_CMD,                 (int)'S',      MNU_SAVE);
	entries[4].Set(wxACCEL_CTRL,                (int)'F',      MNU_FIND);
	entries[5].Set(wxACCEL_CTRL,                (int)'R',      MNU_REPLACE);
	entries[6].Set(wxACCEL_NORMAL,              WXK_F5,        MNU_EXECUTE);
	entries[7].Set(wxACCEL_NORMAL,              WXK_F7,        MNU_EXPLAIN);
	entries[8].Set(wxACCEL_ALT,                 WXK_PAUSE,     MNU_CANCEL);
	entries[9].Set(wxACCEL_CTRL,                (int)'A',       MNU_SELECTALL);
	entries[10].Set(wxACCEL_CMD,                (int)'A',       MNU_SELECTALL);
	entries[11].Set(wxACCEL_NORMAL,              WXK_F1,        MNU_HELP);
	entries[12].Set(wxACCEL_CTRL,               (int)'N',      MNU_NEW);
	entries[13].Set(wxACCEL_CTRL,               WXK_F6,        MNU_EXECPGS);

	wxAcceleratorTable accel(12, entries);
	SetAcceleratorTable(accel);

	queryMenu->Enable(MNU_CANCEL, false);

	int iWidths[7] = {0, -1, 40, 200, 80, 80, 80};
	statusBar = CreateStatusBar(7);
	SetStatusBarPane(-1);
	SetStatusWidths(7, iWidths);
	SetStatusText(_("ready"), STATUSPOS_MSGS);

	toolBar = new ctlMenuToolbar(this, -1, wxDefaultPosition, wxDefaultSize, wxTB_FLAT | wxTB_NODIVIDER);

	toolBar->SetToolBitmapSize(wxSize(16, 16));

	toolBar->AddTool(MNU_NEW, wxEmptyString, *file_new_png_bmp, _("New window"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_OPEN, wxEmptyString, *file_open_png_bmp, _("Open file"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_SAVE, wxEmptyString, *file_save_png_bmp, _("Save file"), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_CUT, wxEmptyString, *clip_cut_png_bmp, _("Cut selected text to clipboard"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_COPY, wxEmptyString, *clip_copy_png_bmp, _("Copy selected text to clipboard"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_PASTE, wxEmptyString, *clip_paste_png_bmp, _("Paste selected text from clipboard"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_CLEAR, wxEmptyString, *edit_clear_png_bmp, _("Clear edit window"), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_UNDO, wxEmptyString, *edit_undo_png_bmp, _("Undo last action"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_REDO, wxEmptyString, *edit_redo_png_bmp, _("Redo last action"), wxITEM_NORMAL);
	toolBar->AddSeparator();
	toolBar->AddTool(MNU_FIND, wxEmptyString, *edit_find_png_bmp, _("Find and replace text"), wxITEM_NORMAL);
	toolBar->AddSeparator();

	toolBar->AddTool(MNU_EXECUTE, wxEmptyString, *query_execute_png_bmp, _("Execute query"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_EXECPGS, wxEmptyString, *query_pgscript_png_bmp, _("Execute pgScript"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_EXECFILE, wxEmptyString, *query_execfile_png_bmp, _("Execute query, write result to file"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_EXPLAIN, wxEmptyString, *query_explain_png_bmp, _("Explain query"), wxITEM_NORMAL);
	toolBar->AddTool(MNU_CANCEL, wxEmptyString, *query_cancel_png_bmp, _("Cancel query"), wxITEM_NORMAL);
	toolBar->AddSeparator();

	toolBar->AddTool(MNU_HELP, wxEmptyString, *help_png_bmp, _("Display help on SQL commands."), wxITEM_NORMAL);
	toolBar->Realize();

	// Add the database selection bar
	cbConnection = new wxBitmapComboBox(this, CTRLID_CONNECTION, wxEmptyString, wxDefaultPosition, wxSize(-1, -1), wxArrayString(), wxCB_READONLY | wxCB_DROPDOWN);
	cbConnection->Append(conn->GetName(), CreateBitmap(GetServerColour(conn)), (void *)conn);
	cbConnection->Append(_("<new connection>"), wxNullBitmap, (void *) NULL);

	//Create SQL editor notebook
	sqlNotebook = new ctlAuiNotebook(this, CTL_NTBKCENTER, wxDefaultPosition, wxDefaultSize, wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_WINDOWLIST_BUTTON);

	// Create panel for query
	wxPanel *pnlQuery = new wxPanel(sqlNotebook);

	// Create the outer box sizer
	wxBoxSizer *boxQuery = new wxBoxSizer(wxVERTICAL);

	// Create the inner box sizer
	// This one will contain the label, the combobox, and the two buttons
	wxBoxSizer *boxHistory = new wxBoxSizer(wxHORIZONTAL);

	// Label
	wxStaticText *label = new wxStaticText(pnlQuery, 0, _("Previous queries"));
	boxHistory->Add(label, 0, wxALL | wxALIGN_CENTER_VERTICAL, 1);

	// Query combobox
	sqlQueries = new wxComboBox(pnlQuery, CTL_SQLQUERYCBOX, wxT(""), wxDefaultPosition, wxDefaultSize, wxArrayString(), wxCB_DROPDOWN | wxCB_READONLY);
	sqlQueries->SetToolTip(_("Previous queries"));
	LoadQueries();
	boxHistory->Add(sqlQueries, 1, wxEXPAND | wxALL | wxALIGN_CENTER_VERTICAL, 1);

	// Delete Current button
	btnDeleteCurrent = new wxButton(pnlQuery, CTL_DELETECURRENTBTN, _("Delete"));
	btnDeleteCurrent->Enable(false);
	boxHistory->Add(btnDeleteCurrent, 0, wxALL | wxALIGN_CENTER_VERTICAL, 1);

	// Delete All button
	btnDeleteAll = new wxButton(pnlQuery, CTL_DELETEALLBTN, _("Delete All"));
	btnDeleteAll->Enable(sqlQueries->GetCount() > 0);
	boxHistory->Add(btnDeleteAll, 0, wxALL | wxALIGN_CENTER_VERTICAL, 1);

	boxQuery->Add(boxHistory, 0, wxEXPAND | wxALL, 1);

	// Create the other inner box sizer
	// This one will contain the SQL box
	wxBoxSizer *boxSQL = new wxBoxSizer(wxHORIZONTAL);

	// Query box
	sqlQuery = new ctlSQLBox(pnlQuery, CTL_SQLQUERY, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxSIMPLE_BORDER | wxTE_RICH2);
	sqlQuery->SetDatabase(conn);
	sqlQuery->SetMarginWidth(1, 16);
	sqlQuery->SetDropTarget(new DnDFile(this));
	SetEOLModeDisplay(sqlQuery->GetEOLMode());
	boxSQL->Add(sqlQuery, 1, wxEXPAND | wxRIGHT | wxLEFT | wxBOTTOM, 1);

	boxQuery->Add(boxSQL, 1, wxEXPAND | wxRIGHT | wxLEFT | wxBOTTOM, 1);

	// Auto-sizing
	pnlQuery->SetSizer(boxQuery);
	boxQuery->Fit(pnlQuery);

	// Results pane
	outputPane = new ctlAuiNotebook(this, CTL_NTBKGQB, wxDefaultPosition, wxSize(500, 300), wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_WINDOWLIST_BUTTON);
	sqlResult = new ctlSQLResult(outputPane, conn, CTL_SQLRESULT, wxDefaultPosition, wxDefaultSize);
	explainCanvas = new ExplainCanvas(outputPane);
	msgResult = new wxTextCtrl(outputPane, CTL_MSGRESULT, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	msgResult->SetFont(settings->GetSQLFont());
	msgHistory = new wxTextCtrl(outputPane, CTL_MSGHISTORY, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	msgHistory->SetFont(settings->GetSQLFont());

	// Graphical Canvas
	// initialize values
	model = new gqbModel();
	controller = new gqbController(model, sqlNotebook, outputPane, wxSize(GQB_MIN_WIDTH, GQB_MIN_HEIGHT));
	firstTime = true;                             // Inform to GQB that the tree of table haven't filled.
	gqbUpdateRunning = false;                      // Are we already updating the SQL query - event recursion protection.
	adjustSizesTimer = NULL;                      // Timer used to avoid a bug when close outputPane

	// Setup SQL editor notebook NBP_SQLEDTR
	sqlNotebook->AddPage(pnlQuery, _("SQL Editor"));
	sqlNotebook->AddPage(controller->getViewContainer(), _("Graphical Query Builder"));
	sqlNotebook->SetSelection(0);

	outputPane->AddPage(sqlResult, _("Data Output"));
	outputPane->AddPage(explainCanvas, _("Explain"));
	outputPane->AddPage(msgResult, _("Messages"));
	outputPane->AddPage(msgHistory, _("History"));

	sqlQuery->Connect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	sqlQuery->Connect(wxID_ANY, wxEVT_KILL_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	sqlResult->Connect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	msgResult->Connect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	msgHistory->Connect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));

	// Now, the scratchpad
	scratchPad = new wxTextCtrl(this, CTL_SCRATCHPAD, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxHSCROLL);

	// Kickstart wxAUI
	manager.AddPane(toolBar, wxAuiPaneInfo().Name(wxT("toolBar")).Caption(_("Tool bar")).ToolbarPane().Top().LeftDockable(false).RightDockable(false));
	manager.AddPane(cbConnection, wxAuiPaneInfo().Name(wxT("databaseBar")).Caption(_("Connection bar")).ToolbarPane().Top().LeftDockable(false).RightDockable(false));
	manager.AddPane(outputPane, wxAuiPaneInfo().Name(wxT("outputPane")).Caption(_("Output pane")).Bottom().MinSize(wxSize(200, 100)).BestSize(wxSize(550, 300)));
	manager.AddPane(scratchPad, wxAuiPaneInfo().Name(wxT("scratchPad")).Caption(_("Scratch pad")).Right().MinSize(wxSize(100, 100)).BestSize(wxSize(250, 200)));
	manager.AddPane(sqlNotebook, wxAuiPaneInfo().Name(wxT("sqlQuery")).Caption(_("SQL query")).Center().CaptionVisible(false).CloseButton(false).MinSize(wxSize(200, 100)).BestSize(wxSize(350, 200)));

	// Now load the layout
	wxString perspective;
	settings->Read(wxT("frmQuery/Perspective-") + wxString(FRMQUERY_PERSPECTIVE_VER), &perspective, FRMQUERY_DEFAULT_PERSPECTIVE);
	manager.LoadPerspective(perspective, true);

	// and reset the captions for the current language
	manager.GetPane(wxT("toolBar")).Caption(_("Tool bar"));
	manager.GetPane(wxT("databaseBar")).Caption(_("Connection bar"));
	manager.GetPane(wxT("sqlQuery")).Caption(_("SQL query"));
	manager.GetPane(wxT("outputPane")).Caption(_("Output pane"));
	manager.GetPane(wxT("scratchPad")).Caption(_("Scratch pad"));


	// Sync the View menu options
	viewMenu->Check(MNU_DATABASEBAR, manager.GetPane(wxT("databaseBar")).IsShown());
	viewMenu->Check(MNU_TOOLBAR, manager.GetPane(wxT("toolBar")).IsShown());
	viewMenu->Check(MNU_OUTPUTPANE, manager.GetPane(wxT("outputPane")).IsShown());
	viewMenu->Check(MNU_SCRATCHPAD, manager.GetPane(wxT("scratchPad")).IsShown());

	// tell the manager to "commit" all the changes just made
	manager.Update();

	bool bVal;

	// Auto-rollback
	settings->Read(wxT("frmQuery/AutoRollback"), &bVal, false);
	queryMenu->Check(MNU_AUTOROLLBACK, bVal);

	// Auto indent
	settings->Read(wxT("frmQuery/AutoIndent"), &bVal, true);
	editMenu->Check(MNU_AUTOINDENT, bVal);
	if (bVal)
		sqlQuery->SetAutoIndent(true);
	else
		sqlQuery->SetAutoIndent(false);

	// Word wrap
	settings->Read(wxT("frmQuery/WordWrap"), &bVal, false);
	viewMenu->Check(MNU_WORDWRAP, bVal);
	if (bVal)
		sqlQuery->SetWrapMode(wxSTC_WRAP_WORD);
	else
		sqlQuery->SetWrapMode(wxSTC_WRAP_NONE);

	// Indent Guides
	settings->Read(wxT("frmQuery/ShowIndentGuides"), &bVal, false);
	viewMenu->Check(MNU_SHOWINDENTGUIDES, bVal);
	if (bVal)
		sqlQuery->SetIndentationGuides(true);
	else
		sqlQuery->SetIndentationGuides(false);

	// Whitespace
	settings->Read(wxT("frmQuery/ShowWhitespace"), &bVal, false);
	viewMenu->Check(MNU_SHOWWHITESPACE, bVal);
	if (bVal)
		sqlQuery->SetViewWhiteSpace(wxSTC_WS_VISIBLEALWAYS);
	else
		sqlQuery->SetViewWhiteSpace(wxSTC_WS_INVISIBLE);

	// Line ends
	settings->Read(wxT("frmQuery/ShowLineEnds"), &bVal, false);
	viewMenu->Check(MNU_SHOWLINEENDS, bVal);
	if (bVal)
		sqlQuery->SetViewEOL(1);
	else
		sqlQuery->SetViewEOL(0);

	// Line number
	settings->Read(wxT("frmQuery/ShowLineNumber"), &bVal, false);
	viewMenu->Check(MNU_SHOWLINENUMBER, bVal);

	if (!file.IsEmpty() && wxFileName::FileExists(file))
	{
		wxFileName fn = file;
		lastFilename = fn.GetFullName();
		lastDir = fn.GetPath();
		lastPath = fn.GetFullPath();
		OpenLastFile();
		sqlQuery->Colourise(0, query.Length());
	}
	else if (!query.IsNull())
	{
		sqlQuery->SetText(query);
		sqlQuery->Colourise(0, query.Length());
		wxSafeYield();                            // needed to process sqlQuery modify event
		changed = false;
		origin = ORIGIN_INITIAL;
		/* _title if not empty should contain displayName of base object for the query.
		   It's pretty good for a proposed filename if the user chooses to Save As. */
		lastFilename = _title;
		setExtendedTitle();
	}

	updateMenu();
	queryMenu->Enable(MNU_SAVEHISTORY, false);
	queryMenu->Enable(MNU_CLEARHISTORY, false);
	setTools(false);
	lastFileFormat = settings->GetUnicodeFile();

	// Note that under GTK+, SetMaxLength() function may only be used with single line text controls.
	// (see http://docs.wxwidgets.org/2.8/wx_wxtextctrl.html#wxtextctrlsetmaxlength)
#ifndef __WXGTK__
	msgResult->SetMaxLength(0L);
	msgHistory->SetMaxLength(0L);
#endif
}


frmQuery::~frmQuery()
{
	closing = true;

	// Save frmQuery Perspective
	settings->Write(wxT("frmQuery/Perspective-") + wxString(FRMQUERY_PERSPECTIVE_VER), manager.SavePerspective());

	// Uninitialize wxAUIManager
	manager.UnInit();

	if(sqlNotebook)
	{
		delete sqlNotebook;
		sqlNotebook = NULL;
	}
	if(controller)
	{
		delete controller;
		controller = NULL;
	}
	if(model)
	{
		delete model;
		model = NULL;
	}
	if(adjustSizesTimer)
	{
		delete adjustSizesTimer;
		adjustSizesTimer = NULL;
	}

	while (cbConnection->GetCount() > 1)
	{
		delete (pgConn *)cbConnection->GetClientData(0);
		cbConnection->Delete(0);
	}

	if (favourites)
	{
		delete favourites;
		favourites = NULL;
	}

	if (pgsTimer)
	{
		delete pgsTimer;
		pgsTimer = NULL;
	}

	if (pgScript)
	{
		delete pgScript;
		pgScript = NULL;
	}

	if (mainForm)
		mainForm->RemoveFrame(this);
}


void frmQuery::OnExit(wxCommandEvent &event)
{
	closing = true;
	Close();
}


void frmQuery::OnEraseBackground(wxEraseEvent &event)
{
	event.Skip();
}


void frmQuery::OnSize(wxSizeEvent &event)
{
	event.Skip();
}


void frmQuery::OnToggleScratchPad(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_SCRATCHPAD))
		manager.GetPane(wxT("scratchPad")).Show(true);
	else
		manager.GetPane(wxT("scratchPad")).Show(false);
	manager.Update();
}


void frmQuery::OnToggleDatabaseBar(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_DATABASEBAR))
		manager.GetPane(wxT("databaseBar")).Show(true);
	else
		manager.GetPane(wxT("databaseBar")).Show(false);
	manager.Update();
}


void frmQuery::OnToggleToolBar(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_TOOLBAR))
		manager.GetPane(wxT("toolBar")).Show(true);
	else
		manager.GetPane(wxT("toolBar")).Show(false);
	manager.Update();
}


void frmQuery::OnToggleOutputPane(wxCommandEvent &event)
{
	if (viewMenu->IsChecked(MNU_OUTPUTPANE))
	{
		manager.GetPane(wxT("outputPane")).Show(true);
	}
	else
	{
		manager.GetPane(wxT("outputPane")).Show(false);
	}
	manager.Update();
	adjustGQBSizes();
}


void frmQuery::OnAuiUpdate(wxAuiManagerEvent &event)
{
	if(event.pane->name == wxT("databaseBar"))
	{
		viewMenu->Check(MNU_DATABASEBAR, false);
	}
	else if(event.pane->name == wxT("toolBar"))
	{
		viewMenu->Check(MNU_TOOLBAR, false);
	}
	else if(event.pane->name == wxT("outputPane"))
	{
		viewMenu->Check(MNU_OUTPUTPANE, false);
		if(!adjustSizesTimer)
			adjustSizesTimer = new wxTimer(this, CTL_TIMERSIZES);
		adjustSizesTimer->Start(500);
	}
	else if(event.pane->name == wxT("scratchPad"))
	{
		viewMenu->Check(MNU_SCRATCHPAD, false);
	}
	event.Skip();
}


void frmQuery::OnDefaultView(wxCommandEvent &event)
{
	manager.LoadPerspective(FRMQUERY_DEFAULT_PERSPECTIVE, true);

	// Reset the captions for the current language
	manager.GetPane(wxT("toolBar")).Caption(_("Tool bar"));
	manager.GetPane(wxT("databaseBar")).Caption(_("Connection bar"));
	manager.GetPane(wxT("sqlQuery")).Caption(_("SQL query"));
	manager.GetPane(wxT("outputPane")).Caption(_("Output pane"));
	manager.GetPane(wxT("scratchPad")).Caption(_("Scratch pad"));

	manager.Update();

	// Sync the View menu options
	viewMenu->Check(MNU_DATABASEBAR, manager.GetPane(wxT("databaseBar")).IsShown());
	viewMenu->Check(MNU_TOOLBAR, manager.GetPane(wxT("toolBar")).IsShown());
	viewMenu->Check(MNU_OUTPUTPANE, manager.GetPane(wxT("outputPane")).IsShown());
	viewMenu->Check(MNU_SCRATCHPAD, manager.GetPane(wxT("scratchPad")).IsShown());
}


void frmQuery::OnAutoRollback(wxCommandEvent &event)
{
	queryMenu->Check(MNU_AUTOROLLBACK, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/AutoRollback"), queryMenu->IsChecked(MNU_AUTOROLLBACK));
}


void frmQuery::OnAutoIndent(wxCommandEvent &event)
{
	editMenu->Check(MNU_AUTOINDENT, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/AutoIndent"), editMenu->IsChecked(MNU_AUTOINDENT));

	if (editMenu->IsChecked(MNU_AUTOINDENT))
		sqlQuery->SetAutoIndent(true);
	else
		sqlQuery->SetAutoIndent(false);
}


void frmQuery::OnWordWrap(wxCommandEvent &event)
{
	viewMenu->Check(MNU_WORDWRAP, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/WordWrap"), viewMenu->IsChecked(MNU_WORDWRAP));

	if (viewMenu->IsChecked(MNU_WORDWRAP))
		sqlQuery->SetWrapMode(wxSTC_WRAP_WORD);
	else
		sqlQuery->SetWrapMode(wxSTC_WRAP_NONE);
}


void frmQuery::OnShowIndentGuides(wxCommandEvent &event)
{
	viewMenu->Check(MNU_SHOWINDENTGUIDES, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/ShowIndentGuides"), viewMenu->IsChecked(MNU_SHOWINDENTGUIDES));

	if (viewMenu->IsChecked(MNU_SHOWINDENTGUIDES))
		sqlQuery->SetIndentationGuides(true);
	else
		sqlQuery->SetIndentationGuides(false);
}


void frmQuery::OnShowWhitespace(wxCommandEvent &event)
{
	viewMenu->Check(MNU_SHOWWHITESPACE, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/ShowWhitespace"), viewMenu->IsChecked(MNU_SHOWWHITESPACE));

	if (viewMenu->IsChecked(MNU_SHOWWHITESPACE))
		sqlQuery->SetViewWhiteSpace(wxSTC_WS_VISIBLEALWAYS);
	else
		sqlQuery->SetViewWhiteSpace(wxSTC_WS_INVISIBLE);
}


void frmQuery::OnShowLineEnds(wxCommandEvent &event)
{
	viewMenu->Check(MNU_SHOWLINEENDS, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/ShowLineEnds"), viewMenu->IsChecked(MNU_SHOWLINEENDS));

	if (viewMenu->IsChecked(MNU_SHOWLINEENDS))
		sqlQuery->SetViewEOL(1);
	else
		sqlQuery->SetViewEOL(0);
}


void frmQuery::OnShowLineNumber(wxCommandEvent &event)
{
	viewMenu->Check(MNU_SHOWLINENUMBER, event.IsChecked());

	settings->WriteBool(wxT("frmQuery/ShowLineNumber"), viewMenu->IsChecked(MNU_SHOWLINENUMBER));

	sqlQuery->UpdateLineNumber();
}

void frmQuery::OnActivate(wxActivateEvent &event)
{
	if (event.GetActive())
		updateMenu();
	event.Skip();
}


void frmQuery::OnExport(wxCommandEvent &ev)
{
	sqlResult->Export();
}


void frmQuery::Go()
{
	cbConnection->SetSelection(0L);
	wxCommandEvent ev;
	OnChangeConnection(ev);

	Show(true);
	sqlQuery->SetFocus();
	loading = false;
}


typedef struct __sqltokenhelp
{
	const wxChar *token;
	const wxChar *page;
	int type;
} SqlTokenHelp;

SqlTokenHelp sqlTokenHelp[] =
{
	// SQL commands
	{ wxT("ABORT"), 0, 0},
	{ wxT("ALTER"), 0, 2},
	{ wxT("ANALYZE"), 0, 0},
	{ wxT("BEGIN"), 0, 0},
	{ wxT("CHECKPOINT"), 0, 0},
	{ wxT("CLOSE"), 0, 0},
	{ wxT("CLUSTER"), 0, 0},
	{ wxT("COMMENT"), 0, 0},
	{ wxT("COMMIT"), 0, 0},
	{ wxT("COPY"), 0, 0},
	{ wxT("CREATE"), 0, 1},
	{ wxT("DEALLOCATE"), 0, 0},
	{ wxT("DECLARE"), 0, 0},
	{ wxT("DELETE"), 0, 0},
	{ wxT("DROP"), 0, 1},
	{ wxT("END"), 0, 0},
	{ wxT("EXECUTE"), 0, 0},
	{ wxT("EXPLAIN"), 0, 0},
	{ wxT("FETCH"), 0, 0},
	{ wxT("GRANT"), 0, 0},
	{ wxT("INSERT"), 0, 0},
	{ wxT("LISTEN"), 0, 0},
	{ wxT("LOAD"), 0, 0},
	{ wxT("LOCK"), 0, 0},
	{ wxT("MOVE"), 0, 0},
	{ wxT("NOTIFY"), 0, 0},
	{ wxT("END"), 0, 0},
	// { wxT("PREPARE"), 0, 0},  handled individually
	{ wxT("REINDEX"), 0, 0},
	{ wxT("RELEASE"), wxT("pg/sql-release-savepoint"), 0},
	{ wxT("RESET"), 0, 0},
	{ wxT("REVOKE"), 0, 0},
	// { wxT("ROLLBACK"), 0, 0}, handled individually
	{ wxT("SAVEPOINT"), 0, 0},
	{ wxT("SELECT"), 0, 0},
	{ wxT("SET"), 0, 0},
	{ wxT("SHOW"), 0, 0},
	{ wxT("START"), wxT("pg/sql-start-transaction"), 0},
	{ wxT("TRUNCATE"), 0, 0},
	{ wxT("UNLISTEN"), 0, 0},
	{ wxT("UPDATE"), 0, 0},
	{ wxT("VACUUM"), 0, 0},

	// SQL commands (second token)
	{ wxT("AGGREGATE"), 0, 12},
	{ wxT("CAST"), 0, 11},
	{ wxT("COLLATION"), 0, 12},
	{ wxT("CONSTRAINT"), 0, 11},
	{ wxT("CONVERSION"), 0, 12},
	{ wxT("DATABASE"), 0, 12},
	{ wxT("DOMAIN"), 0, 12},
	{ wxT("EXTENSION"), 0, 12},
	{ wxT("FUNCTION"), 0, 12},
	{ wxT("GROUP"), 0, 12},
	{ wxT("INDEX"), 0, 12},
	{ wxT("LANGUAGE"), 0, 12},
	{ wxT("OPERATOR"), 0, 12},
	{ wxT("ROLE"), 0, 12},
	{ wxT("RULE"), 0, 12},
	{ wxT("SCHEMA"), 0, 12},
	{ wxT("SEQUENCE"), 0, 12},
	{ wxT("SERVER"), 0, 12},
	{ wxT("TABLE"), 0, 12},
	{ wxT("TABLESPACE"), 0, 12},
	{ wxT("TRIGGER"), 0, 12},
	{ wxT("TYPE"), 0, 12},
	{ wxT("USER"), 0, 12},
	{ wxT("VIEW"), 0, 12},
	{ wxT("EXTTABLE"), 0, 12},

	// Data types
	{ wxT("SMALLINT"), wxT("datatype-numeric"), 0},
	{ wxT("INTEGER"), wxT("datatype-numeric"), 0},
	{ wxT("BIGINT"), wxT("datatype-numeric"), 0},
	{ wxT("DECIMAL"), wxT("datatype-numeric"), 0},
	{ wxT("NUMERIC"), wxT("datatype-numeric"), 0},
	{ wxT("REAL"), wxT("datatype-numeric"), 0},
	{ wxT("DOUBLE PRECISION"), wxT("datatype-numeric"), 0},
	{ wxT("SMALLSERIAL"), wxT("datatype-numeric"), 0},
	{ wxT("SERIAL"), wxT("datatype-numeric"), 0},
	{ wxT("BIGSERIAL"), wxT("datatype-numeric"), 0},
	{ wxT("MONEY"), wxT("datatype-money"), 0},
	{ wxT("CHARACTER"), wxT("datatype-character"), 0},
	{ wxT("VARCHAR"), wxT("datatype-character"), 0},
	{ wxT("CHAR"), wxT("datatype-character"), 0},
	{ wxT("TEXT"), wxT("datatype-character"), 0},
	{ wxT("BYTEA"), wxT("datatype-binary"), 0},
	{ wxT("TIMESTAMP"), wxT("datatype-datetime"), 0},
	{ wxT("DATE"), wxT("datatype-datetime"), 0},
	{ wxT("TIME"), wxT("datatype-datetime"), 0},
	{ wxT("INTERVAL"), wxT("datatype-datetime"), 0},
	{ wxT("BOOLEAN"), wxT("datatype-boolean"), 0},
	{ wxT("ENUM"), wxT("datatype-enum"), 0},
	{ wxT("POINT"), wxT("datatype-geometric"), 0},
	{ wxT("PATH"), wxT("datatype-geometric"), 0},
	{ wxT("POLYGON"), wxT("datatype-geometric"), 0},
	{ wxT("CIRCLE"), wxT("datatype-geometric"), 0},
	{ wxT("LINE"), wxT("datatype-geometric"), 0},
	{ wxT("LSEG"), wxT("datatype-geometric"), 0},
	{ wxT("BOX"), wxT("datatype-geometric"), 0},
	{ wxT("CIDR"), wxT("datatype-net-types"), 0},
	{ wxT("INET"), wxT("datatype-net-types"), 0},
	{ wxT("MACADDR"), wxT("datatype-net-types"), 0},
	{ wxT("BIT"), wxT("datatype-bit"), 0},
	{ wxT("TSVECTOR"), wxT("datatype-textsearch"), 0},
	{ wxT("TSQUERY"), wxT("datatype-textsearch"), 0},
	{ wxT("UUID"), wxT("datatype-uuid"), 0},
	{ wxT("XML"), wxT("datatype-xml"), 0},
	{ wxT("JSON"), wxT("datatype-json"), 0},
	{ wxT("ARRAY"), wxT("arrays"), 0},
	{ wxT("INT4RANGE"), wxT("rangetypes"), 0},
	{ wxT("INT8RANGE"), wxT("rangetypes"), 0},
	{ wxT("NUMRANGE"), wxT("rangetypes"), 0},
	{ wxT("TSRANGE"), wxT("rangetypes"), 0},
	{ wxT("TSTZRANGE"), wxT("rangetypes"), 0},
	{ wxT("DATERANGE"), wxT("rangetypes"), 0},
	{ wxT("OID"), wxT("datatype-oid"), 0},
	{ wxT("REGPROC"), wxT("datatype-oid"), 0},
	{ wxT("REGPROCEDURE"), wxT("datatype-oid"), 0},
	{ wxT("REGOPER"), wxT("datatype-oid"), 0},
	{ wxT("REGOPERATOR"), wxT("datatype-oid"), 0},
	{ wxT("REGCLASS"), wxT("datatype-oid"), 0},
	{ wxT("REGTYPE"), wxT("datatype-oid"), 0},
	{ wxT("REGCONFIG"), wxT("datatype-oid"), 0},
	{ wxT("REGDICTIONARY"), wxT("datatype-oid"), 0},

	{ wxT("ANY"), wxT("datatype-pseudo"), 0},
	{ wxT("ANYELEMENT"), wxT("datatype-pseudo"), 0},
	{ wxT("ANYARRAY"), wxT("datatype-pseudo"), 0},
	{ wxT("ANYNONARRAY"), wxT("datatype-pseudo"), 0},
	{ wxT("ANYENUM"), wxT("datatype-pseudo"), 0},
	{ wxT("ANYRANGE"), wxT("datatype-pseudo"), 0},
	{ wxT("CSTRING"), wxT("datatype-pseudo"), 0},
	{ wxT("INTERNAL"), wxT("datatype-pseudo"), 0},
	{ wxT("LANGUAGE_HANDLER"), wxT("datatype-pseudo"), 0},
	{ wxT("FDW_HANDLER"), wxT("datatype-pseudo"), 0},
	{ wxT("RECORD"), wxT("datatype-pseudo"), 0},
	{ wxT("TRIGGER"), wxT("datatype-pseudo"), 0},
	{ wxT("VOID"), wxT("datatype-pseudo"), 0},
	{ wxT("OPAQUE"), wxT("datatype-pseudo"), 0},

	// Functions
	{ wxT("AND"), wxT("functions-logical"), 0},
	{ wxT("OR"), wxT("functions-logical"), 0},
	{ wxT("NOT"), wxT("functions-logical"), 0},
	{ wxT("IS"), wxT("functions-comparison"), 0},
	{ wxT("ABS"), wxT("functions-math"), 0},
	{ wxT("CBRT"), wxT("functions-math"), 0},
	{ wxT("CEIL"), wxT("functions-math"), 0},
	{ wxT("CEILING"), wxT("functions-math"), 0},
	{ wxT("DEGREES"), wxT("functions-math"), 0},
	{ wxT("DIV"), wxT("functions-math"), 0},
	{ wxT("EXP"), wxT("functions-math"), 0},
	{ wxT("FLOOR"), wxT("functions-math"), 0},
	{ wxT("LN"), wxT("functions-math"), 0},
	{ wxT("LOG"), wxT("functions-math"), 0},
	{ wxT("MOD"), wxT("functions-math"), 0},
	{ wxT("PI"), wxT("functions-math"), 0},
	{ wxT("POWER"), wxT("functions-math"), 0},
	{ wxT("POWER"), wxT("functions-math"), 0},
	{ wxT("RADIANS"), wxT("functions-math"), 0},
	{ wxT("RANDOM"), wxT("functions-math"), 0},
	{ wxT("ROUND"), wxT("functions-math"), 0},
	{ wxT("SETSEED"), wxT("functions-math"), 0},
	{ wxT("SIGN"), wxT("functions-math"), 0},
	{ wxT("SQRT"), wxT("functions-math"), 0},
	{ wxT("TRUNC"), wxT("functions-math"), 0},
	{ wxT("WIDTH_BUCKET"), wxT("functions-math"), 0},

	{ wxT("BIT_LENGTH"), wxT("functions-string"), 0},
	{ wxT("CHAR_LENGTH"), wxT("functions-string"), 0},
	{ wxT("CHARACTER_LENGTH"), wxT("functions-string"), 0},
	{ wxT("LOWER"), wxT("functions-string"), 0},
	{ wxT("OCTET_LENGTH"), wxT("functions-string"), 0},
	{ wxT("OVERLAY"), wxT("functions-string"), 0},
	{ wxT("POSITION"), wxT("functions-string"), 0},
	{ wxT("SUBSTRING"), wxT("functions-string"), 0},
	{ wxT("TRIM"), wxT("functions-string"), 0},
	{ wxT("UPPER"), wxT("functions-string"), 0},
	{ wxT("ASCII"), wxT("functions-string"), 0},
	{ wxT("BTRIM"), wxT("functions-string"), 0},
	{ wxT("CHR"), wxT("functions-string"), 0},
	{ wxT("CONCAT"), wxT("functions-string"), 0},
	{ wxT("CONCAT_WS"), wxT("functions-string"), 0},
	{ wxT("CONVERT"), wxT("functions-string"), 0},
	{ wxT("CONVERT_FROM"), wxT("functions-string"), 0},
	{ wxT("CONVERT_TO"), wxT("functions-string"), 0},
	{ wxT("DECODE"), wxT("functions-string"), 0},
	{ wxT("ENCODE"), wxT("functions-string"), 0},
	{ wxT("FORMAT"), wxT("functions-string"), 0},
	{ wxT("INITCAP"), wxT("functions-string"), 0},
	{ wxT("LEFT"), wxT("functions-string"), 0},
	{ wxT("LENGTH"), wxT("functions-string"), 0},
	{ wxT("LPAD"), wxT("functions-string"), 0},
	{ wxT("LTRIM"), wxT("functions-string"), 0},
	{ wxT("MD5"), wxT("functions-string"), 0},
	{ wxT("PG_CLIENT_ENCODING"), wxT("functions-string"), 0},
	{ wxT("QUOTE"), wxT("functions-string"), 0},
	{ wxT("QUOTE_IDENT"), wxT("functions-string"), 0},
	{ wxT("QUOTE_LITERAL"), wxT("functions-string"), 0},
	{ wxT("QUOTE_NULLABLE"), wxT("functions-string"), 0},
	{ wxT("REPEAT"), wxT("functions-string"), 0},
	{ wxT("REPLACE"), wxT("functions-string"), 0},
	{ wxT("REVERSE"), wxT("functions-string"), 0},
	{ wxT("RIGHT"), wxT("functions-string"), 0},
	{ wxT("RPAD"), wxT("functions-string"), 0},
	{ wxT("RTRIM"), wxT("functions-string"), 0},
	{ wxT("SPLIT_PART"), wxT("functions-string"), 0},
	{ wxT("STRPOS"), wxT("functions-string"), 0},
	{ wxT("SUBSTR"), wxT("functions-string"), 0},
	{ wxT("TO_ASCII"), wxT("functions-string"), 0},
	{ wxT("TO_HEX"), wxT("functions-string"), 0},
	{ wxT("TRANSLATE"), wxT("functions-string"), 0},

	{ wxT("OCTET_LENGTH"), wxT("functions-binarystring"), 0},
	{ wxT("OVERLAY"), wxT("functions-binarystring"), 0},
	{ wxT("POSITION"), wxT("functions-binarystring"), 0},
	{ wxT("SUBSTRING"), wxT("functions-binarystring"), 0},
	{ wxT("TRIM"), wxT("functions-binarystring"), 0},
	{ wxT("BTRIM"), wxT("functions-binarystring"), 0},
	{ wxT("GET_BIT"), wxT("functions-binarystring"), 0},
	{ wxT("GET_BYTE"), wxT("functions-binarystring"), 0},
	{ wxT("LENGTH"), wxT("functions-binarystring"), 0},
	{ wxT("MD5"), wxT("functions-binarystring"), 0},
	{ wxT("SET_BIT"), wxT("functions-binarystring"), 0},
	{ wxT("SET_BYTE"), wxT("functions-binarystring"), 0},

	{ wxT("LIKE"), wxT("functions-matching"), 0},
	{ wxT("ILIKE"), wxT("functions-matching"), 0},
	{ wxT("SIMILAR"), wxT("functions-matching"), 0},
	{ wxT("REGEXP_MATCHES"), wxT("functions-matching"), 0},
	{ wxT("REGEXP_REPLACE"), wxT("functions-matching"), 0},
	{ wxT("REGEXP_SPLIT_TO_ARRAY"), wxT("functions-matching"), 0},
	{ wxT("REGEXP_SPLIT_TO_TABLE"), wxT("functions-matching"), 0},

	{ wxT("TO_CHAR"), wxT("functions-formatting"), 0},
	{ wxT("TO_DATE"), wxT("functions-formatting"), 0},
	{ wxT("TO_NUMBER"), wxT("functions-formatting"), 0},
	{ wxT("TO_TIMESTAMP"), wxT("functions-formatting"), 0},

	{ wxT("AGE"), wxT("functions-datetime"), 0},
	{ wxT("CLOCK_TIMESTAMP"), wxT("functions-datetime"), 0},
	{ wxT("CURRENT_DATE"), wxT("functions-datetime"), 0},
	{ wxT("CURRENT_TIME"), wxT("functions-datetime"), 0},
	{ wxT("CURRENT_TIMESTAMP"), wxT("functions-datetime"), 0},
	{ wxT("DATE_PART"), wxT("functions-datetime"), 0},
	{ wxT("DATE_TRUNC"), wxT("functions-datetime"), 0},
	{ wxT("EXTRACT"), wxT("functions-datetime"), 0},
	{ wxT("ISFINITE"), wxT("functions-datetime"), 0},
	{ wxT("JUSTIFY_DAYS"), wxT("functions-datetime"), 0},
	{ wxT("JUSTIFY_HOURS"), wxT("functions-datetime"), 0},
	{ wxT("JUSTIFY_INTERVAL"), wxT("functions-datetime"), 0},
	{ wxT("LOCALTIME"), wxT("functions-datetime"), 0},
	{ wxT("LOCALTIMESTAMP"), wxT("functions-datetime"), 0},
	{ wxT("NOW"), wxT("functions-datetime"), 0},
	{ wxT("STATEMENT_TIMESTAMP"), wxT("functions-datetime"), 0},
	{ wxT("TIMEOFDAY"), wxT("functions-datetime"), 0},
	{ wxT("TRANSACTION_TIMESTAMP"), wxT("functions-datetime"), 0},

	{ wxT("ENUM_FIRST"), wxT("functions-enum"), 0},
	{ wxT("ENUM_LAST"), wxT("functions-enum"), 0},
	{ wxT("ENUM_RANGE"), wxT("functions-enum"), 0},

	{ wxT("AREA"), wxT("functions-geometry"), 0},
	{ wxT("CENTER"), wxT("functions-geometry"), 0},
	{ wxT("DIAMETER"), wxT("functions-geometry"), 0},
	{ wxT("HEIGHT"), wxT("functions-geometry"), 0},
	{ wxT("ISCLOSED"), wxT("functions-geometry"), 0},
	{ wxT("ISOPEN"), wxT("functions-geometry"), 0},
	{ wxT("LENGTH"), wxT("functions-geometry"), 0},
	{ wxT("NPOINTS"), wxT("functions-geometry"), 0},
	{ wxT("PCLOSE"), wxT("functions-geometry"), 0},
	{ wxT("POPEN"), wxT("functions-geometry"), 0},
	{ wxT("RADIUS"), wxT("functions-geometry"), 0},
	{ wxT("WIDTH"), wxT("functions-geometry"), 0},
	{ wxT("BOX"), wxT("functions-geometry"), 0},
	{ wxT("CIRCLE"), wxT("functions-geometry"), 0},
	{ wxT("LSEG"), wxT("functions-geometry"), 0},
	{ wxT("PATH"), wxT("functions-geometry"), 0},
	{ wxT("POINT"), wxT("functions-geometry"), 0},
	{ wxT("POLYGON"), wxT("functions-geometry"), 0},

	{ wxT("ABBREV"), wxT("functions-net"), 0},
	{ wxT("BROADCAST"), wxT("functions-net"), 0},
	{ wxT("FAMILY"), wxT("functions-net"), 0},
	{ wxT("HOST"), wxT("functions-net"), 0},
	{ wxT("HOSTMASK"), wxT("functions-net"), 0},
	{ wxT("MASKLEN"), wxT("functions-net"), 0},
	{ wxT("NETMASK"), wxT("functions-net"), 0},
	{ wxT("NETWORK"), wxT("functions-net"), 0},
	{ wxT("SET_MASKLEN"), wxT("functions-net"), 0},

	{ wxT("GET_CURRENT_TS_CONFIG"), wxT("functions-textsearch"), 0},
	{ wxT("LENGTH"), wxT("functions-textsearch"), 0},
	{ wxT("NUMNODE"), wxT("functions-textsearch"), 0},
	{ wxT("PLAINTO_TSQUERY"), wxT("functions-textsearch"), 0},
	{ wxT("QUERYTREE"), wxT("functions-textsearch"), 0},
	{ wxT("SETWEIGHT"), wxT("functions-textsearch"), 0},
	{ wxT("STRIP"), wxT("functions-textsearch"), 0},
	{ wxT("TO_TSQUERY"), wxT("functions-textsearch"), 0},
	{ wxT("TO_TSVECTOR"), wxT("functions-textsearch"), 0},
	{ wxT("TS_HEADLINE"), wxT("functions-textsearch"), 0},
	{ wxT("TS_RANK"), wxT("functions-textsearch"), 0},
	{ wxT("TS_RANK_CD"), wxT("functions-textsearch"), 0},
	{ wxT("TS_REWRITE"), wxT("functions-textsearch"), 0},
	{ wxT("TSVECTOR_UPDATE_TRIGGER"), wxT("functions-textsearch"), 0},
	{ wxT("TSVECTOR_UPDATE_TRIGGER_COLUMN"), wxT("functions-textsearch"), 0},
	{ wxT("TS_DEBUG"), wxT("functions-textsearch"), 0},
	{ wxT("TS_LEXIZE"), wxT("functions-textsearch"), 0},
	{ wxT("TS_PARSE"), wxT("functions-textsearch"), 0},
	{ wxT("TS_TOKEN_TYPE"), wxT("functions-textsearch"), 0},
	{ wxT("TS_STAT"), wxT("functions-textsearch"), 0},

	{ wxT("XMLPARSE"), wxT("functions-xml"), 0},
	{ wxT("XMLSERIALIZE"), wxT("functions-xml"), 0},
	{ wxT("XMLCOMMENT"), wxT("functions-xml"), 0},
	{ wxT("XMLCONCAT"), wxT("functions-xml"), 0},
	{ wxT("XMLELEMENT"), wxT("functions-xml"), 0},
	{ wxT("XMLFOREST"), wxT("functions-xml"), 0},
	{ wxT("XMLPI"), wxT("functions-xml"), 0},
	{ wxT("XMLROOT"), wxT("functions-xml"), 0},
	{ wxT("XMLAGG"), wxT("functions-xml"), 0},
	{ wxT("XMLEXISTS"), wxT("functions-xml"), 0},
	{ wxT("XML_IS_WELL_FORMED"), wxT("functions-xml"), 0},
	{ wxT("XML_IS_WELL_FORMED_DOCUMENT"), wxT("functions-xml"), 0},
	{ wxT("XML_IS_WELL_FORMED_CONTENT"), wxT("functions-xml"), 0},
	{ wxT("XPATH"), wxT("functions-xml"), 0},
	{ wxT("XPATH_EXISTS"), wxT("functions-xml"), 0},
	{ wxT("TABLE_TO_XML"), wxT("functions-xml"), 0},
	{ wxT("QUERY_TO_XML"), wxT("functions-xml"), 0},
	{ wxT("CURSOR_TO_XML"), wxT("functions-xml"), 0},

	{ wxT("ARRAY_TO_JSON"), wxT("functions-json"), 0},
	{ wxT("ROW_TO_JSON"), wxT("functions-json"), 0},
	{ wxT("TO_JSON"), wxT("functions-json"), 0},
	{ wxT("JSON_ARRAY_LENGTH"), wxT("functions-json"), 0},
	{ wxT("JSON_EACH"), wxT("functions-json"), 0},
	{ wxT("JSON_EACH_TEXT"), wxT("functions-json"), 0},
	{ wxT("JSON_EXTRACT_PATH"), wxT("functions-json"), 0},
	{ wxT("JSON_EXTRACT_PATH_TEXT"), wxT("functions-json"), 0},
	{ wxT("JSON_OBJECT_KEYS"), wxT("functions-json"), 0},
	{ wxT("JSON_POPULATE_RECORD"), wxT("functions-json"), 0},
	{ wxT("JSON_POPULATE_RECORDSET"), wxT("functions-json"), 0},
	{ wxT("JSON_ARRAY_ELEMENTS"), wxT("functions-json"), 0},

	{ wxT("CURRVAL"), wxT("functions-sequence"), 0},
	{ wxT("LASTVAL"), wxT("functions-sequence"), 0},
	{ wxT("NEXTVAL"), wxT("functions-sequence"), 0},
	{ wxT("SETVAL"), wxT("functions-sequence"), 0},

	{ wxT("CASE"), wxT("functions-conditional"), 0},
	{ wxT("COALESCE"), wxT("functions-conditional"), 0},
	{ wxT("NULLIF"), wxT("functions-conditional"), 0},
	{ wxT("GREATEST"), wxT("functions-conditional"), 0},
	{ wxT("LEAST"), wxT("functions-conditional"), 0},

	{ wxT("ARRAY_APPEND"), wxT("functions-array"), 0},
	{ wxT("ARRAY_CAT"), wxT("functions-array"), 0},
	{ wxT("ARRAY_NDIMS"), wxT("functions-array"), 0},
	{ wxT("ARRAY_DIMS"), wxT("functions-array"), 0},
	{ wxT("ARRAY_FILL"), wxT("functions-array"), 0},
	{ wxT("ARRAY_LENGTH"), wxT("functions-array"), 0},
	{ wxT("ARRAY_LOWER"), wxT("functions-array"), 0},
	{ wxT("ARRAY_PREPEND"), wxT("functions-array"), 0},
	{ wxT("ARRAY_REMOVE"), wxT("functions-array"), 0},
	{ wxT("ARRAY_REPLACE"), wxT("functions-array"), 0},
	{ wxT("ARRAY_TO_STRING"), wxT("functions-array"), 0},
	{ wxT("ARRAY_UPPER"), wxT("functions-array"), 0},
	{ wxT("STRING_TO_ARRAY"), wxT("functions-array"), 0},
	{ wxT("UNNEST"), wxT("functions-array"), 0},

	{ wxT("LOWER"), wxT("functions-range"), 0},
	{ wxT("UPPER"), wxT("functions-range"), 0},
	{ wxT("ISEMPTY"), wxT("functions-range"), 0},
	{ wxT("LOWER_INC"), wxT("functions-range"), 0},
	{ wxT("UPPER_INC"), wxT("functions-range"), 0},
	{ wxT("LOWER_INF"), wxT("functions-range"), 0},
	{ wxT("UPPER_INF"), wxT("functions-range"), 0},

	{ wxT("ARRAY_AGG"), wxT("functions-aggregate"), 0},
	{ wxT("AVG"), wxT("functions-aggregate"), 0},
	{ wxT("BIT_AND"), wxT("functions-aggregate"), 0},
	{ wxT("BIT_OR"), wxT("functions-aggregate"), 0},
	{ wxT("BOOL_AND"), wxT("functions-aggregate"), 0},
	{ wxT("BOOL_OR"), wxT("functions-aggregate"), 0},
	{ wxT("COUNT"), wxT("functions-aggregate"), 0},
	{ wxT("EVERY"), wxT("functions-aggregate"), 0},
	{ wxT("JSON_AGG"), wxT("functions-aggregate"), 0},
	{ wxT("MAX"), wxT("functions-aggregate"), 0},
	{ wxT("MIN"), wxT("functions-aggregate"), 0},
	{ wxT("STRING_AGG"), wxT("functions-aggregate"), 0},
	{ wxT("SUM"), wxT("functions-aggregate"), 0},
	{ wxT("XMLAGG"), wxT("functions-aggregate"), 0},
	{ wxT("CORR"), wxT("functions-aggregate"), 0},
	{ wxT("COVAR_POP"), wxT("functions-aggregate"), 0},
	{ wxT("COVAR_SAMP"), wxT("functions-aggregate"), 0},
	{ wxT("REGR_AVGX"), wxT("functions-aggregate"), 0},
	{ wxT("REGR_AVGY"), wxT("functions-aggregate"), 0},
	{ wxT("REGR_COUNT"), wxT("functions-aggregate"), 0},
	{ wxT("REGR_INTERCEPT"), wxT("functions-aggregate"), 0},
	{ wxT("REGR_R2"), wxT("functions-aggregate"), 0},
	{ wxT("REGR_SLOPE"), wxT("functions-aggregate"), 0},
	{ wxT("STDDEV_POP"), wxT("functions-aggregate"), 0},
	{ wxT("STDDEV_SAMP"), wxT("functions-aggregate"), 0},
	{ wxT("VARIANCE"), wxT("functions-aggregate"), 0},
	{ wxT("VAR_POP"), wxT("functions-aggregate"), 0},
	{ wxT("VAR_SAMP"), wxT("functions-aggregate"), 0},

	{ wxT("ROW_NUMBER"), wxT("functions-window"), 0},
	{ wxT("RANK"), wxT("functions-window"), 0},
	{ wxT("DENSE_RANK"), wxT("functions-window"), 0},
	{ wxT("PERCENT_RANK"), wxT("functions-window"), 0},
	{ wxT("CUME_DIST"), wxT("functions-window"), 0},
	{ wxT("NTILE"), wxT("functions-window"), 0},
	{ wxT("LAG"), wxT("functions-window"), 0},
	{ wxT("LEAD"), wxT("functions-window"), 0},
	{ wxT("FIRST_VALUE"), wxT("functions-window"), 0},
	{ wxT("LAST_VALUE"), wxT("functions-window"), 0},
	{ wxT("NTH_VALUE"), wxT("functions-window"), 0},

	{ wxT("EXISTS"), wxT("functions-subquery"), 0},
	{ wxT("IN"), wxT("functions-subquery"), 0},
	{ wxT("ANY"), wxT("functions-subquery"), 0},
	{ wxT("SOME"), wxT("functions-subquery"), 0},
	{ wxT("ALL"), wxT("functions-subquery"), 0},

	{ wxT("GENERATE_SERIES"), wxT("functions-srf"), 0},
	{ wxT("GENERATE_SUBSCRIPTS"), wxT("functions-srf"), 0},

	{ wxT("CURRENT_CATALOG"), wxT("functions-info"), 0},
	{ wxT("CURRENT_USER"), wxT("functions-info"), 0},
	{ wxT("INET_CLIENT_ADDR"), wxT("functions-info"), 0},
	{ wxT("INET_CLIENT_PORT"), wxT("functions-info"), 0},
	{ wxT("INET_SERVER_ADDR"), wxT("functions-info"), 0},
	{ wxT("INET_SERVER_PORT"), wxT("functions-info"), 0},
	{ wxT("PG_BACKEND_PID"), wxT("functions-info"), 0},
	{ wxT("PG_CONF_LOAD_TIME"), wxT("functions-info"), 0},
	{ wxT("PG_IS_OTHER_TEMP_SCHEMA"), wxT("functions-info"), 0},
	{ wxT("PG_LISTENING_CHANNELS"), wxT("functions-info"), 0},
	{ wxT("PG_MY_TEMP_SCHEMA"), wxT("functions-info"), 0},
	{ wxT("PG_POSTMASTER_START_TIME"), wxT("functions-info"), 0},
	{ wxT("PG_TRIGGER_DEPTH"), wxT("functions-info"), 0},
	{ wxT("SESSION_USER"), wxT("functions-info"), 0},
	{ wxT("USER"), wxT("functions-info"), 0},
	{ wxT("VERSION"), wxT("functions-info"), 0},
	{ wxT("HAS_ANY_COLUMN_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_COLUMN_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_DATABASE_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_FOREIGN_DATA_WRAPPER_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_FUNCTION_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_LANGUAGE_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_SCHEMA_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_SEQUENCE_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_SERVER_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_TABLE_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("HAS_TABLESPACE_PRIVILEGE"), wxT("functions-info"), 0},
	{ wxT("PG_HAS_ROLE"), wxT("functions-info"), 0},
	{ wxT("PG_COLLATION_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_CONVERSION_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_FUNCTION_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_OPCLASS_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_OPERATOR_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_OPFAMILY_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_TABLE_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_TS_CONFIG_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_TS_DICT_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_TS_PARSER_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_TS_TEMPLATE_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("PG_TYPE_IS_VISIBLE"), wxT("functions-info"), 0},
	{ wxT("FORMAT_TYPE"), wxT("functions-info"), 0},
	{ wxT("PG_DESCRIBE_OBJECT"), wxT("functions-info"), 0},
	{ wxT("PG_IDENTIFY_OBJECT"), wxT("functions-info"), 0},
	{ wxT("PG_GET_CONSTRAINTDEF"), wxT("functions-info"), 0},
	{ wxT("PG_GET_EXPR"), wxT("functions-info"), 0},
	{ wxT("PG_GET_FUNCTIONDEF"), wxT("functions-info"), 0},
	{ wxT("PG_GET_FUNCTION_ARGUMENTS"), wxT("functions-info"), 0},
	{ wxT("PG_GET_FUNCTION_IDENTITY_ARGUMENTS"), wxT("functions-info"), 0},
	{ wxT("PG_GET_FUNCTION_RESULT"), wxT("functions-info"), 0},
	{ wxT("PG_GET_INDEXDEF"), wxT("functions-info"), 0},
	{ wxT("PG_GET_KEYWORDS"), wxT("functions-info"), 0},
	{ wxT("PG_GET_RULEDEF"), wxT("functions-info"), 0},
	{ wxT("PG_GET_SERIAL_SEQUENCE"), wxT("functions-info"), 0},
	{ wxT("PG_GET_TRIGGERDEF"), wxT("functions-info"), 0},
	{ wxT("PG_GET_USERBYID"), wxT("functions-info"), 0},
	{ wxT("PG_GET_VIEWDEF"), wxT("functions-info"), 0},
	{ wxT("PG_OPTIONS_TO_TABLE"), wxT("functions-info"), 0},
	{ wxT("PG_TABLESPACE_DATABASES"), wxT("functions-info"), 0},
	{ wxT("PG_TABLESPACE_LOCATION"), wxT("functions-info"), 0},
	{ wxT("PG_TYPEOF"), wxT("functions-info"), 0},
	{ wxT("COLLATION"), wxT("functions-info"), 0},
	{ wxT("COL_DESCRIPTION"), wxT("functions-info"), 0},
	{ wxT("OBJ_DESCRIPTION"), wxT("functions-info"), 0},
	{ wxT("SHOBJ_DESCRIPTION"), wxT("functions-info"), 0},
	{ wxT("TXID_CURRENT"), wxT("functions-info"), 0},
	{ wxT("TXID_CURRENT_SNAPSHOT"), wxT("functions-info"), 0},
	{ wxT("TXID_SNAPSHOT_XIP"), wxT("functions-info"), 0},
	{ wxT("TXID_SNAPSHOT_XMAX"), wxT("functions-info"), 0},
	{ wxT("TXID_SNAPSHOT_XMIN"), wxT("functions-info"), 0},
	{ wxT("TXID_VISIBLE_IN_SNAPSHOT"), wxT("functions-info"), 0},
	{ wxT("XMIN"), wxT("functions-info"), 0},
	{ wxT("XMAX"), wxT("functions-info"), 0},
	{ wxT("XIP_LIST"), wxT("functions-info"), 0},

	{ wxT("CURRENT_SETTING"), wxT("functions-admin"), 0},
	{ wxT("SET_CONFIG"), wxT("functions-admin"), 0},
	{ wxT("PG_CANCEL_BACKEND"), wxT("functions-admin"), 0},
	{ wxT("PG_RELOAD_CONF"), wxT("functions-admin"), 0},
	{ wxT("PG_ROTATE_LOGFILE"), wxT("functions-admin"), 0},
	{ wxT("PG_TERMINATE_BACKEND"), wxT("functions-admin"), 0},
	{ wxT("PG_CREATE_RESTORE_POINT"), wxT("functions-admin"), 0},
	{ wxT("PG_CURRENT_XLOG_INSERT_LOCATION"), wxT("functions-admin"), 0},
	{ wxT("PG_CURRENT_XLOG_LOCATION"), wxT("functions-admin"), 0},
	{ wxT("PG_START_BACKUP"), wxT("functions-admin"), 0},
	{ wxT("PG_STOP_BACKUP"), wxT("functions-admin"), 0},
	{ wxT("PG_IS_IN_BACKUP"), wxT("functions-admin"), 0},
	{ wxT("PG_BACKUP_START_TIME"), wxT("functions-admin"), 0},
	{ wxT("PG_SWITCH_XLOG"), wxT("functions-admin"), 0},
	{ wxT("PG_XLOGFILE_NAME"), wxT("functions-admin"), 0},
	{ wxT("PG_XLOGFILE_NAME_OFFSET"), wxT("functions-admin"), 0},
	{ wxT("PG_XLOG_LOCATION_DIFF"), wxT("functions-admin"), 0},
	{ wxT("PG_IS_IN_RECOVERY"), wxT("functions-admin"), 0},
	{ wxT("PG_LAST_XLOG_RECEIVE_LOCATION"), wxT("functions-admin"), 0},
	{ wxT("PG_LAST_XLOG_REPLAY_LOCATION"), wxT("functions-admin"), 0},
	{ wxT("PG_LAST_XACT_REPLAY_TIMESTAMP"), wxT("functions-admin"), 0},
	{ wxT("PG_IS_XLOG_REPLAY_PAUSED"), wxT("functions-admin"), 0},
	{ wxT("PG_XLOG_REPLAY_PAUSE"), wxT("functions-admin"), 0},
	{ wxT("PG_XLOG_REPLAY_RESUME"), wxT("functions-admin"), 0},
	{ wxT("PG_EXPORT_SNAPSHOT"), wxT("functions-admin"), 0},
	{ wxT("PG_COLUMN_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_DATABASE_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_INDEXES_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_RELATION_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_SIZE_PRETTY"), wxT("functions-admin"), 0},
	{ wxT("PG_TABLE_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_TABLESPACE_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_TOTAL_RELATION_SIZE"), wxT("functions-admin"), 0},
	{ wxT("PG_RELATION_FILENODE"), wxT("functions-admin"), 0},
	{ wxT("PG_RELATION_FILEPATH"), wxT("functions-admin"), 0},
	{ wxT("PG_LS_DIR"), wxT("functions-admin"), 0},
	{ wxT("PG_READ_FILE"), wxT("functions-admin"), 0},
	{ wxT("PG_READ_BINARY_FILE"), wxT("functions-admin"), 0},
	{ wxT("PG_STAT_FILE"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_LOCK"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_LOCK_SHARED"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_UNLOCK"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_UNLOCK_ALL"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_UNLOCK_SHARED"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_XACT_LOCK"), wxT("functions-admin"), 0},
	{ wxT("PG_ADVISORY_XACT_LOCK_SHARED"), wxT("functions-admin"), 0},
	{ wxT("PG_TRY_ADVISORY_LOCK"), wxT("functions-admin"), 0},
	{ wxT("PG_TRY_ADVISORY_LOCK_SHARED"), wxT("functions-admin"), 0},
	{ wxT("PG_TRY_ADVISORY_XACT_LOCK"), wxT("functions-admin"), 0},
	{ wxT("PG_TRY_ADVISORY_XACT_LOCK_SHARED"), wxT("functions-admin"), 0},

	{ wxT("PG_EVENT_TRIGGER_DROPPED_OBJECTS"), wxT("functions-event-triggers"), 0},

	// System catalogs
	{ wxT("PG_AGGREGATE"), wxT("catalog-pg-aggregate"), 0},
	{ wxT("PG_AM"), wxT("catalog-pg-am"), 0},
	{ wxT("PG_AMOP"), wxT("catalog-pg-amop"), 0},
	{ wxT("PG_AMPROC"), wxT("catalog-pg-amproc"), 0},
	{ wxT("PG_ATTRDEF"), wxT("catalog-pg-attrdef"), 0},
	{ wxT("PG_ATTRIBUTE"), wxT("catalog-pg-attribute"), 0},
	{ wxT("PG_AUTHID"), wxT("catalog-pg-authid"), 0},
	{ wxT("PG_AUTH_MEMBERS"), wxT("catalog-pg-auth-members"), 0},
	{ wxT("PG_CAST"), wxT("catalog-pg-cast"), 0},
	{ wxT("PG_CLASS"), wxT("catalog-pg-class"), 0},
	{ wxT("PG_CONSTRAINT"), wxT("catalog-pg-constraint"), 0},
	{ wxT("PG_COLLATION"), wxT("catalog-pg-collation"), 0},
	{ wxT("PG_CONVERSION"), wxT("catalog-pg-conversion"), 0},
	{ wxT("PG_DATABASE"), wxT("catalog-pg-database"), 0},
	{ wxT("PG_DB_ROLE_SETTING"), wxT("catalog-pg-db-role-setting"), 0},
	{ wxT("PG_DEFAULT_ACL"), wxT("catalog-pg-default-acl"), 0},
	{ wxT("PG_DEPEND"), wxT("catalog-pg-depend"), 0},
	{ wxT("PG_DESCRIPTION"), wxT("catalog-pg-description"), 0},
	{ wxT("PG_ENUM"), wxT("catalog-pg-enum"), 0},
	{ wxT("PG_EVENT_TRIGGER"), wxT("catalog-pg-event-trigger"), 0},
	{ wxT("PG_EXTENSION"), wxT("catalog-pg-extension"), 0},
	{ wxT("PG_FOREIGN_DATA_WRAPPER"), wxT("catalog-pg-foreign-data-wrapper"), 0},
	{ wxT("PG_FOREIGN_SERVER"), wxT("catalog-pg-foreign-server"), 0},
	{ wxT("PG_FOREIGN_TABLE"), wxT("catalog-pg-foreign-table"), 0},
	{ wxT("PG_INDEX"), wxT("catalog-pg-index"), 0},
	{ wxT("PG_INHERITS"), wxT("catalog-pg-inherits"), 0},
	{ wxT("PG_LANGUAGE"), wxT("catalog-pg-language"), 0},
	{ wxT("PG_LARGEOBJECT"), wxT("catalog-pg-largeobject"), 0},
	{ wxT("PG_LARGEOBJECT_METADATA"), wxT("catalog-pg-largeobject-metadata"), 0},
	{ wxT("PG_NAMESPACE"), wxT("catalog-pg-namespace"), 0},
	{ wxT("PG_OPCLASS"), wxT("catalog-pg-opclass"), 0},
	{ wxT("PG_OPERATOR"), wxT("catalog-pg-operator"), 0},
	{ wxT("PG_OPFAMILY"), wxT("catalog-pg-opfamily"), 0},
	{ wxT("PG_PLTEMPLATE"), wxT("catalog-pg-pltemplate"), 0},
	{ wxT("PG_PROC"), wxT("catalog-pg-proc"), 0},
	{ wxT("PG_RANGE"), wxT("catalog-pg-range"), 0},
	{ wxT("PG_REWRITE"), wxT("catalog-pg-rewrite"), 0},
	{ wxT("PG_SECLABEL"), wxT("catalog-pg-seclabel"), 0},
	{ wxT("PG_SHDEPEND"), wxT("catalog-pg-shdepend"), 0},
	{ wxT("PG_SHDESCRIPTION"), wxT("catalog-pg-shdescription"), 0},
	{ wxT("PG_SHSECLABEL"), wxT("catalog-pg-shseclabel"), 0},
	{ wxT("PG_STATISTIC"), wxT("catalog-pg-statistic"), 0},
	{ wxT("PG_TABLESPACE"), wxT("catalog-pg-tablespace"), 0},
	{ wxT("PG_TRIGGER"), wxT("catalog-pg-trigger"), 0},
	{ wxT("PG_TS_CONFIG"), wxT("catalog-pg-ts-config"), 0},
	{ wxT("PG_TS_CONFIG_MAP"), wxT("catalog-pg-ts-config-map"), 0},
	{ wxT("PG_TS_DICT"), wxT("catalog-pg-ts-dict"), 0},
	{ wxT("PG_TS_PARSER"), wxT("catalog-pg-ts-parser"), 0},
	{ wxT("PG_TS_TEMPLATE"), wxT("catalog-pg-ts-template"), 0},
	{ wxT("PG_TYPE"), wxT("catalog-pg-type"), 0},
	{ wxT("PG_USER_MAPPING"), wxT("catalog-pg-user-mapping"), 0},

	// System views
	{ wxT("PG_AVAILABLE_EXTENSIONS"), wxT("view-pg-available-extensions"), 0},
	{ wxT("PG_AVAILABLE_EXTENSION_VERSIONS"), wxT("view-pg-available-extension-versions"), 0},
	{ wxT("PG_CURSORS"), wxT("view-pg-cursors"), 0},
	{ wxT("PG_GROUP"), wxT("view-pg-group"), 0},
	{ wxT("PG_INDEXES"), wxT("view-pg-indexes"), 0},
	{ wxT("PG_LOCKS"), wxT("view-pg-locks"), 0},
	{ wxT("PG_MATVIEWS"), wxT("view-pg-matviews"), 0},
	{ wxT("PG_PREPARED_STATEMENTS"), wxT("view-pg-prepared-statements"), 0},
	{ wxT("PG_PREPARED_XACTS"), wxT("view-pg-prepared-xacts"), 0},
	{ wxT("PG_ROLES"), wxT("view-pg-roles"), 0},
	{ wxT("PG_RULES"), wxT("view-pg-rules"), 0},
	{ wxT("PG_SECLABELS"), wxT("view-pg-seclabels"), 0},
	{ wxT("PG_SETTINGS"), wxT("view-pg-settings"), 0},
	{ wxT("PG_SHADOW"), wxT("view-pg-shadow"), 0},
	{ wxT("PG_STATS"), wxT("view-pg-stats"), 0},
	{ wxT("PG_TABLES"), wxT("view-pg-tables"), 0},
	{ wxT("PG_TIMEZONE_ABBREVS"), wxT("view-pg-timezone-abbrevs"), 0},
	{ wxT("PG_TIMEZONE_NAMES"), wxT("view-pg-timezone-names"), 0},
	{ wxT("PG_USER"), wxT("view-pg-user"), 0},
	{ wxT("PG_USER_MAPPINGS"), wxT("view-pg-user-mappings"), 0},
	{ wxT("PG_VIEWS"), wxT("view-pg-views"), 0},

	{ 0, 0 }
};

void frmQuery::OnContents(wxCommandEvent &event)
{
	DisplayHelp(wxT("query"), HELP_PGADMIN);
}


void frmQuery::OnChangeConnection(wxCommandEvent &ev)
{
	// On Solaris, this event seems to get fired when the form closes(!!)
	if(!IsVisible() && !loading)
		return;

	unsigned int sel = cbConnection->GetSelection();
	if (sel == cbConnection->GetCount() - 1)
	{
		// new Connection
		dlgSelectConnection dlg(this, mainForm);
		int rc = dlg.Go(conn, cbConnection);
		if (rc == wxID_OK)
		{
			bool createdNewConn;
			wxString applicationname = appearanceFactory->GetLongAppName() + _(" - Query Tool");
			pgConn *newconn = dlg.CreateConn(applicationname, createdNewConn);
			if (newconn && createdNewConn)
			{
				cbConnection->Insert(newconn->GetName(), CreateBitmap(GetServerColour(newconn)), sel);
				cbConnection->SetClientData(sel, (void *)newconn);
				cbConnection->SetSelection(sel);
				OnChangeConnection(ev);
			}
			else
				rc = wxID_CANCEL;
		}
		if (rc != wxID_OK)
		{
			unsigned int i;
			for (i = 0 ; i < sel ; i++)
			{
				if (cbConnection->GetClientData(i) == conn)
				{
					cbConnection->SetSelection(i);
					break;
				}
			}
		}
	}
	else
	{
		conn = (pgConn *)cbConnection->GetClientData(sel);
		sqlResult->SetConnection(conn);
		pgScript->SetConnection(conn);
		title = wxT("Query - ") + cbConnection->GetValue();
		setExtendedTitle();

		//Refresh GQB Tree if used
		if(conn && !firstTime)
		{
			controller->getTablesBrowser()->refreshTables(conn);
			controller->getView()->Refresh();
		}
	}
}


void frmQuery::OnHelp(wxCommandEvent &event)
{
	wxString page;
	wxString query = sqlQuery->GetSelectedText();
	if (query.IsEmpty())
	{
		// get the word under cursor:
		int curPos = sqlQuery->GetCurrentPos();
		query = sqlQuery->GetTextRange(sqlQuery->WordStartPosition(curPos, true),
				                       sqlQuery->WordEndPosition(curPos, true));
		query.Trim(false);
	}

	if (query.IsEmpty())
	{
		//get whole query
		query = sqlQuery->GetText();
	query.Trim(false);
	}

	wxLogInfo(wxString::Format(wxT("frmQuery::OnHelp(): [%s]"), query.c_str()));

	if (!query.IsEmpty())
	{
		wxStringTokenizer tokens(query);
		query = tokens.GetNextToken();

		if (query.IsSameAs(wxT("PREPARE"), false))
		{
			if (tokens.GetNextToken().IsSameAs(wxT("TRANSACTION"), false))
				page = wxT("sql-prepare-transaction");
			else
				page = wxT("sql-prepare");
		}
		else if (query.IsSameAs(wxT("ROLLBACK"), false))
		{
			if (tokens.GetNextToken().IsSameAs(wxT("PREPARED"), false))
				page = wxT("sql-rollback-prepared");
			else
				page = wxT("sql-rollback");
		}
		else
		{
			SqlTokenHelp *sth = sqlTokenHelp;
			while (sth->token)
			{
				if (sth->type < 10 && query.IsSameAs(sth->token, false))
				{
					if (sth->page)
						page = sth->page;
					else
						page = wxT("sql-") + query.Lower();

					if (sth->type)
					{
						int type = sth->type + 10;

						query = tokens.GetNextToken();
						sth = sqlTokenHelp;
						while (sth->token)
						{
							if (sth->type >= type && query.IsSameAs(sth->token, false))
							{
								if (sth->page)
									page += sth->page;
								else
									page += query.Lower();
								break;
							}
							sth++;
						}
						if (!sth->token)
							page = wxT("sql-commands");
					}
					break;
				}
				sth++;
			}
		}
	}
	if (page.IsEmpty())
		page = wxT("sql-commands");

	if (conn->GetIsEdb())
		DisplayHelp(page, HELP_ENTERPRISEDB);
	else if (conn->GetIsGreenplum())
		DisplayHelp(page, HELP_GREENPLUM);
	else
		DisplayHelp(page, HELP_POSTGRESQL);
}


void frmQuery::OnSaveHistory(wxCommandEvent &event)
{
#ifdef __WXMSW__
	wxFileDialog *dlg = new wxFileDialog(this, _("Save history"), lastDir, wxEmptyString,
	                                     _("Log files (*.log)|*.log|All files (*.*)|*.*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
#else
	wxFileDialog *dlg = new wxFileDialog(this, _("Save history"), lastDir, wxEmptyString,
	                                     _("Log files (*.log)|*.log|All files (*)|*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
#endif
	if (dlg->ShowModal() == wxID_OK)
	{
		if (!FileWrite(dlg->GetPath(), msgHistory->GetValue(), false))
		{
			wxLogError(__("Could not write the file %s: Errcode=%d."), dlg->GetPath().c_str(), wxSysErrorCode());
		}
	}
	delete dlg;

}

void frmQuery::OnChangeNotebook(wxAuiNotebookEvent &event)
{
	// A bug in wxGTK prevents us to show a modal dialog within a
	// EVT_AUINOTEBOOK_PAGE_CHANGED event
	// So, we need these three lines of code to work-around it
	wxWindow *win = wxWindow::GetCapture();
	if (win)
		win->ReleaseMouse();

	if(sqlNotebook && sqlNotebook->GetPageCount() >= 2)
	{

		if (event.GetSelection() == 0)
		{
			queryMenu->SetHelpString(MNU_EXECUTE, _("Execute query"));
			queryMenu->SetHelpString(MNU_EXECFILE, _("Execute query, write result to file"));
			toolBar->SetToolShortHelp(MNU_EXECUTE, _("Execute query"));
			toolBar->SetToolShortHelp(MNU_EXECFILE, _("Execute query, write result to file"));
			viewMenu->Enable(MNU_OUTPUTPANE, true);
			viewMenu->Enable(MNU_SCRATCHPAD, true);

			// Reset the panes
			if (viewMenu->IsChecked(MNU_OUTPUTPANE))
				manager.GetPane(wxT("outputPane")).Show(true);
			if (viewMenu->IsChecked(MNU_SCRATCHPAD))
				manager.GetPane(wxT("scratchPad")).Show(true);
			manager.Update();

			updateFromGqb(false);
		}
		else
		{
			manager.GetPane(wxT("outputPane")).Show(false);
			manager.GetPane(wxT("scratchPad")).Show(false);
			manager.Update();
			viewMenu->Enable(MNU_OUTPUTPANE, false);
			viewMenu->Enable(MNU_SCRATCHPAD, false);

			if(firstTime)        //Things that should be done on first click on GQB
			{
				// Menu
				queryMenu->Append(MNU_EXECUTE, _("Generate SQL from Graphical Query Builder Model"));
				queryMenu->SetHelpString(MNU_EXECFILE, _("Generate SQL from Graphical Query Builder Model"));
				toolBar->SetToolShortHelp(MNU_EXECUTE, _("Generate SQL from Graphical Query Builder Model"));
				toolBar->SetToolShortHelp(MNU_EXECFILE, _("Generate SQL from Graphical Query Builder Model"));

				// Size, and pause to allow the window to draw
				adjustGQBSizes();
				wxTheApp->Yield(true);

				// Database related Stuffs.
				// Create a server object and connect it.
				controller->getTablesBrowser()->refreshTables(conn);
				firstTime = false;
			}
		}
	}
}


void frmQuery::OnSetFocus(wxFocusEvent &event)
{
	sqlQuery->SetFocus();
	event.Skip();
}


void frmQuery::OnClearHistory(wxCommandEvent &event)
{
	queryMenu->Enable(MNU_SAVEHISTORY, false);
	queryMenu->Enable(MNU_CLEARHISTORY, false);
	msgHistory->Clear();
	msgHistory->SetFont(settings->GetSQLFont());
}


void frmQuery::OnFocus(wxFocusEvent &ev)
{
	if (wxDynamicCast(this, wxFrame))
		updateMenu();
	else
	{
		frmQuery *wnd = (frmQuery *)GetParent();

		if (wnd)
			wnd->OnFocus(ev);
	}
	ev.Skip();
}


void frmQuery::OnCut(wxCommandEvent &ev)
{
	if (currentControl() == sqlQuery)
	{
		sqlQuery->Cut();
		updateMenu();
	}
}


wxWindow *frmQuery::currentControl()
{
	wxWindow *wnd = FindFocus();
	if (wnd == outputPane)
	{
		switch (outputPane->GetSelection())
		{
			case 0:
				wnd = sqlResult;
				break;
			case 1:
				wnd = explainCanvas;
				break;
			case 2:
				wnd = msgResult;
				break;
			case 3:
				wnd = msgHistory;
				break;
		}
	}
	return wnd;

}


void frmQuery::OnCopy(wxCommandEvent &ev)
{
	wxWindow *wnd = currentControl();

	if (wnd == sqlQuery)
		sqlQuery->Copy();
	else if (wnd == msgResult)
		msgResult->Copy();
	else if (wnd == msgHistory)
		msgHistory->Copy();
	else if (wnd == scratchPad)
		scratchPad->Copy();
	else
	{
		wxWindow *obj = wnd;

		while (obj != NULL)
		{
			if (obj == sqlResult)
			{
				sqlResult->Copy();
				break;
			}
			obj = obj->GetParent();
		}
	}
	updateMenu();
}


void frmQuery::OnPaste(wxCommandEvent &ev)
{
	if (currentControl() == sqlQuery)
		sqlQuery->Paste();
	else if (currentControl() == scratchPad)
		scratchPad->Paste();
}


void frmQuery::OnClear(wxCommandEvent &ev)
{
	wxWindow *wnd = currentControl();

	if (wnd == sqlQuery)
		sqlQuery->ClearAll();
	else if (wnd == msgResult)
	{
		msgResult->Clear();
		msgResult->SetFont(settings->GetSQLFont());
	}
	else if (wnd == msgHistory)
	{
		msgHistory->Clear();
		msgHistory->SetFont(settings->GetSQLFont());
	}
	else if (wnd == scratchPad)
		scratchPad->Clear();
}


void frmQuery::OnSelectAll(wxCommandEvent &ev)
{
	wxWindow *wnd = currentControl();

	if (wnd == sqlQuery)
		sqlQuery->SelectAll();
	else if (wnd == msgResult)
		msgResult->SelectAll();
	else if (wnd == msgHistory)
		msgHistory->SelectAll();
	else if (wnd == sqlResult)
		sqlResult->SelectAll();
	else if (wnd == scratchPad)
		scratchPad->SelectAll();
	else if (wnd->GetParent() == sqlResult)
		sqlResult->SelectAll();
}


void frmQuery::OnSearchReplace(wxCommandEvent &ev)
{
	sqlQuery->OnSearchReplace(ev);
}


void frmQuery::OnUndo(wxCommandEvent &ev)
{
	sqlQuery->Undo();
}


void frmQuery::OnRedo(wxCommandEvent &ev)
{
	sqlQuery->Redo();
}


void frmQuery::setExtendedTitle()
{
	wxString chgStr;
	if (changed)
		chgStr = wxT(" *");

	if (lastPath.IsNull())
		SetTitle(title + chgStr);
	else
	{
		SetTitle(title + wxT(" - [") + lastPath + wxT("]") + chgStr);
	}
	// Allow to save initial queries though they are not changed
	bool enableSave = changed || (origin == ORIGIN_INITIAL);
	toolBar->EnableTool(MNU_SAVE, enableSave);
	fileMenu->Enable(MNU_SAVE, enableSave);
}

bool frmQuery::relatesToWindow(wxWindow *which, wxWindow *related)
{
	while (which != NULL)
	{
		if (which == related)
			return true;
		else
			which = which->GetParent();
	}
	return false;
}

void frmQuery::updateMenu(bool allowUpdateModelSize)
{
	bool canCut = false;
	bool canCopy = false;
	bool canPaste = false;
	bool canUndo = false;
	bool canRedo = false;
	bool canClear = false;
	bool canFind = false;
	bool canAddFavourite = false;
	bool canManageFavourite = false;
	bool canSaveExplain = false;
	bool canSaveGQB = false;

	wxAuiFloatingFrame *fp = wxDynamicCastThis(wxAuiFloatingFrame);
	if (fp)
		return;

	if (closing)
		return;

	wxWindow *wnd = currentControl();
	if (wnd != NULL)
	{
		if (   relatesToWindow(wnd, sqlQuery)
		        || relatesToWindow(wnd, sqlResult)
		        || relatesToWindow(wnd, msgResult)
		        || relatesToWindow(wnd, msgHistory)
		        || relatesToWindow(wnd, scratchPad)   )
		{
			if (relatesToWindow(wnd, sqlQuery))
			{
				canUndo = sqlQuery->CanUndo();
				canRedo = sqlQuery->CanRedo();
				canPaste = sqlQuery->CanPaste();
				canFind = true;
				canAddFavourite = (sqlQuery->GetLength() > 0) && (settings->GetFavouritesFile().Length() > 0);
				canManageFavourite = (settings->GetFavouritesFile().Length() > 0);
			}
			else if (relatesToWindow(wnd, scratchPad))
				canPaste = true;
			canCopy = true;
			canCut = true;
			canClear = true;
		}
	}

	canSaveExplain = explainCanvas->GetDiagram()->GetCount() > 0;

	if (allowUpdateModelSize)
	{
		canSaveGQB = controller->getView() != NULL && controller->getView()->canSaveAsImage();
	}

	toolBar->EnableTool(MNU_UNDO, canUndo);
	editMenu->Enable(MNU_UNDO, canUndo);

	toolBar->EnableTool(MNU_REDO, canRedo);
	editMenu->Enable(MNU_REDO, canRedo);

	toolBar->EnableTool(MNU_COPY, canCopy);
	editMenu->Enable(MNU_COPY, canCopy);

	toolBar->EnableTool(MNU_PASTE, canPaste);
	editMenu->Enable(MNU_PASTE, canPaste);

	toolBar->EnableTool(MNU_CUT, canCut);
	editMenu->Enable(MNU_CUT, canCut);

	toolBar->EnableTool(MNU_CLEAR, canClear);
	editMenu->Enable(MNU_CLEAR, canClear);

	toolBar->EnableTool(MNU_FIND, canFind);
	editMenu->Enable(MNU_FIND, canFind);

	favouritesMenu->Enable(MNU_FAVOURITES_ADD, canAddFavourite);
	favouritesMenu->Enable(MNU_FAVOURITES_INJECT, canAddFavourite); // these two use the same criteria
	favouritesMenu->Enable(MNU_FAVOURITES_MANAGE, canManageFavourite);
}


void frmQuery::UpdateFavouritesList()
{
	if (favourites)
		delete favourites;

	favourites = queryFavouriteFileProvider::LoadFavourites(true);

	while (favouritesMenu->GetMenuItemCount() > 4) // there are 3 static items + separator above
	{
		favouritesMenu->Destroy(favouritesMenu->GetMenuItems()[4]);
	}

	favourites->AppendAllToMenu(favouritesMenu, MNU_FAVOURITES_MANAGE + 1);
}


void frmQuery::UpdateMacrosList()
{
	if (macros)
		delete macros;

	macros = queryMacroFileProvider::LoadMacros(true);

	while (macrosMenu->GetMenuItemCount() > 2)
	{
		macrosMenu->Destroy(macrosMenu->GetMenuItems()[2]);
	}

	macros->AppendAllToMenu(macrosMenu, MNU_MACROS_MANAGE + 1);
}


void frmQuery::OnAddFavourite(wxCommandEvent &event)
{
	if (sqlQuery->GetText().Trim().IsEmpty())
		return;
	int r = dlgAddFavourite(this, favourites).AddFavourite(sqlQuery->GetText());
	if (r == 1)
	{
		// Added a favourite, so save
		queryFavouriteFileProvider::SaveFavourites(favourites);
	}
	if (r == 1 || r == -1)
	{
		// Changed something requiring rollback
		mainForm->UpdateAllFavouritesList();
	}
}


void frmQuery::OnInjectFavourite(wxCommandEvent &event)
{
	queryFavouriteItem *fav;
	bool selected = true;
	int startPos, endPos;
	wxString name = sqlQuery->GetSelectedText();

	if (name.IsEmpty())
	{
		// get the word under cursor:
		int curPos = sqlQuery->GetCurrentPos();
		startPos = sqlQuery->WordStartPosition(curPos, true);
		endPos = sqlQuery->WordEndPosition(curPos, true);
		name = sqlQuery->GetTextRange(startPos, endPos);
		selected = false;
	}
	name.Trim(false).Trim(true);
	if (name.IsEmpty())
		return;

	// search for favourite with this name
	fav = favourites->FindFavourite(name);
	if (!fav)
		return;

	// replace selection (or current word) with it's contents
	//wxLogInfo(wxT("frmQuery::OnReplaceFavourite(): name=[%s] contents=[%s]"), name, fav->GetContents());
	sqlQuery->BeginUndoAction();
	if (!selected)
		sqlQuery->SetSelection(startPos, endPos);
	sqlQuery->ReplaceSelection(fav->GetContents());
	sqlQuery->EndUndoAction();
}


void frmQuery::OnManageFavourites(wxCommandEvent &event)
{
	int r = dlgManageFavourites(this, favourites).ManageFavourites();
	if (r == 1)
	{
		// Changed something, so save
		queryFavouriteFileProvider::SaveFavourites(favourites);
	}
	if (r == 1 || r == -1)
	{
		// Changed something requiring rollback
		mainForm->UpdateAllFavouritesList();
	}
}


void frmQuery::OnSelectFavourite(wxCommandEvent &event)
{
	queryFavouriteItem *fav;

	fav = favourites->FindFavourite(event.GetId());
	if (!fav)
		return;

	if (!sqlQuery->GetText().Trim().IsEmpty())
	{
		int r = wxMessageDialog(this, _("Replace current query?"), _("Confirm replace"), wxYES_NO | wxCANCEL | wxICON_QUESTION).ShowModal();
		if (r == wxID_CANCEL)
			return;
		else if (r == wxID_YES)
			sqlQuery->ClearAll();
		else
		{
			if (sqlQuery->GetText().Last() != '\n')
				sqlQuery->AddText(wxT("\n"));     // Add a newline after the last query
		}
	}
	sqlQuery->AddText(fav->GetContents());
}


bool frmQuery::CheckChanged(bool canVeto)
{
	if (changed && settings->GetAskSaveConfirmation())
	{
		wxString fn;
		if (!lastPath.IsNull())
			fn = wxString::Format(_("The text in file %s has changed.\nDo you want to save changes?"), lastPath.c_str());
		else
			fn = _("The text has changed.\nDo you want to save changes?");
		wxMessageDialog msg(this, fn, _("Query"),
		                    wxYES_NO | wxICON_EXCLAMATION |
		                    (canVeto ? wxCANCEL : 0));

		wxCommandEvent noEvent;
		switch (msg.ShowModal())
		{
			case wxID_YES:
				if (lastPath.IsNull())
					OnSaveAs(noEvent);
				else
					OnSave(noEvent);

				return changed;

			case wxID_CANCEL:
				return true;
		}
	}
	return false;
}


void frmQuery::OnClose(wxCloseEvent &event)
{
	if (queryMenu->IsEnabled(MNU_CANCEL))
	{
		if (event.CanVeto())
		{
			wxMessageDialog msg(this, _("A query is running. Do you wish to cancel it?"), _("Query"),
			                    wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);

			if (msg.ShowModal() != wxID_YES)
			{
				event.Veto();
				return;
			}
		}

		wxCommandEvent ev;
		OnCancel(ev);
	}

	while (sqlResult->RunStatus() == CTLSQL_RUNNING)
	{
		wxLogInfo(wxT("SQL Query box: Waiting for query to abort"));
		wxSleep(1);
	}

	if (m_loadingfile && event.CanVeto())
	{
		wxMessageBox(_("The query tool cannot be closed whilst a file is loading."), _("Warning"), wxICON_INFORMATION | wxOK);
		event.Veto();

		return;
	}

	if (CheckChanged(event.CanVeto()) && event.CanVeto())
	{
		event.Veto();
		return;
	}

	closing = true;

	// Reset the panes
	if (viewMenu->IsChecked(MNU_OUTPUTPANE))
		manager.GetPane(wxT("outputPane")).Show(true);
	if (viewMenu->IsChecked(MNU_SCRATCHPAD))
		manager.GetPane(wxT("scratchPad")).Show(true);
	manager.Update();

	Hide();

	sqlQuery->Disconnect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	sqlResult->Disconnect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	msgResult->Disconnect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));
	msgHistory->Disconnect(wxID_ANY, wxEVT_SET_FOCUS, wxFocusEventHandler(frmQuery::OnFocus));

	controller->nullView();                   //to avoid bug on *nix when deleting controller

	settings->SetExplainVerbose(queryMenu->IsChecked(MNU_VERBOSE));
	settings->SetExplainCosts(queryMenu->IsChecked(MNU_COSTS));
	settings->SetExplainBuffers(queryMenu->IsChecked(MNU_BUFFERS));
	settings->SetExplainTiming(queryMenu->IsChecked(MNU_TIMING));

	sqlResult->Abort();                           // to make sure conn is unused

	Destroy();

}


void frmQuery::OnChangeStc(wxStyledTextEvent &event)
{
	// The STC seems to fire this event even if it loses focus. Fortunately,
	// that seems to be m_modificationType == 512.
	if (event.m_modificationType != 512 &&
	        // Sometimes there come events 20 and 520 AFTER the initial query was set by constructor.
	        // Their occurence is related to query's size and possibly international characters in it (??)
	        // Filter them out to keep "initial" origin of query text.
	        (origin != ORIGIN_INITIAL || (event.m_modificationType != 20 && event.m_modificationType != 520)))
	{
		// This is the default change origin.
		// In other cases the changer function will reset it after this event.
		origin = ORIGIN_MANUAL;
		if (!changed)
		{
			changed = true;
			setExtendedTitle();
		}
	}
	// do not allow update of model size of GQB on input (key press) of each
	// character of the query in Query Tool
	updateMenu(false);
}


void frmQuery::OnPositionStc(wxStyledTextEvent &event)
{
	int selFrom, selTo, selCount;
	sqlQuery->GetSelection(&selFrom, &selTo);
	selCount = selTo - selFrom;

	wxString pos;
	pos.Printf(_("Ln %d, Col %d, Ch %d"), sqlQuery->LineFromPosition(sqlQuery->GetCurrentPos()) + 1, sqlQuery->GetColumn(sqlQuery->GetCurrentPos()) + 1, sqlQuery->GetCurrentPos() + 1);
	SetStatusText(pos, STATUSPOS_POS);
	if (selCount < 1)
		pos = wxEmptyString;
	else
		pos.Printf(wxPLURAL("%d char", "%d chars", selCount), selCount);
	SetStatusText(pos, STATUSPOS_SEL);
}


void frmQuery::OpenLastFile()
{
	wxString str;
	bool modeUnicode = settings->GetUnicodeFile();
	wxUtfFile file(lastPath, wxFile::read, modeUnicode ? wxFONTENCODING_UTF8 : wxFONTENCODING_DEFAULT);

	m_loadingfile = true;
	if (file.IsOpened())
		file.Read(str);

	if (!str.IsEmpty())
	{
		sqlQuery->SetText(str);
		sqlQuery->Colourise(0, str.Length());
		wxSafeYield();                            // needed to process sqlQuery modify event
		changed = false;
		origin = ORIGIN_FILE;
		setExtendedTitle();
		SetLineEndingStyle();
		UpdateRecentFiles(true);
		if(mainForm != NULL)
		{
			mainForm->UpdateAllRecentFiles();
		}
	}
	sqlQuery->SetFocus();
	m_loadingfile = false;
}


void frmQuery::UpdateAllRecentFiles()
{
	mainForm->UpdateAllRecentFiles();
}

void frmQuery::OnNew(wxCommandEvent &event)
{
	frmQuery *fq = new frmQuery(mainForm, wxEmptyString, conn->Duplicate(), wxEmptyString);
	if (mainForm)
		mainForm->AddFrame(fq);
	fq->Go();
}


void frmQuery::OnOpen(wxCommandEvent &event)
{
	if (CheckChanged(true))
		return;

#ifdef __WXMSW__
	wxFileDialog dlg(this, _("Open query file"), lastDir, wxT(""),
	                 _("Query files (*.sql)|*.sql|pgScript files (*.pgs)|*.pgs|All files (*.*)|*.*"), wxFD_OPEN);
#else
	wxFileDialog dlg(this, _("Open query file"), lastDir, wxT(""),
	                 _("Query files (*.sql)|*.sql|pgScript files (*.pgs)|*.pgs|All files (*)|*"), wxFD_OPEN);
#endif

	if (dlg.ShowModal() == wxID_OK)
	{
		lastFilename = dlg.GetFilename();
		lastDir = dlg.GetDirectory();
		lastPath = dlg.GetPath();
		OpenLastFile();
	}
}


void frmQuery::OnSave(wxCommandEvent &event)
{
	bool modeUnicode = settings->GetUnicodeFile();

	if (lastPath.IsNull())
	{
		OnSaveAs(event);
		return;
	}

	wxUtfFile file(lastPath, wxFile::write, modeUnicode ? wxFONTENCODING_UTF8 : wxFONTENCODING_DEFAULT);
	if (file.IsOpened())
	{
		if ((file.Write(sqlQuery->GetText()) == 0) && (!modeUnicode))
			wxMessageBox(_("Query text incomplete.\nQuery contained characters that could not be converted to the local charset.\nPlease correct the data or try using UTF8 instead."));
		file.Close();
		changed = false;
		setExtendedTitle();
		UpdateRecentFiles();
	}
	else
	{
		wxLogError(__("Could not write the file %s: Errcode=%d."), lastPath.c_str(), wxSysErrorCode());
	}
}


// Set the line ending style based on the current document.
void frmQuery::SetLineEndingStyle()
{
	// Detect the file mode
	wxRegEx *reLF = new wxRegEx(wxT("[^\r]\n"), wxRE_NEWLINE);
	wxRegEx *reCRLF = new wxRegEx(wxT("\r\n"), wxRE_NEWLINE);
	wxRegEx *reCR = new wxRegEx(wxT("\r[^\n]"), wxRE_NEWLINE);

	bool haveLF = reLF->Matches(sqlQuery->GetText());
	bool haveCRLF = reCRLF->Matches(sqlQuery->GetText());
	bool haveCR = reCR->Matches(sqlQuery->GetText());
	int mode = GetLineEndingStyle();

	if ((haveLF && haveCR) ||
	        (haveLF && haveCRLF) ||
	        (haveCR && haveCRLF))
	{
		wxMessageBox(_("This file contains mixed line endings. They will be converted to the current setting."), _("Warning"), wxICON_INFORMATION | wxOK);
		sqlQuery->ConvertEOLs(mode);
		changed = true;
		setExtendedTitle();
		updateMenu();
	}
	else
	{
		if (haveLF)
			mode = wxSTC_EOL_LF;
		else if (haveCRLF)
			mode = wxSTC_EOL_CRLF;
		else if (haveCR)
			mode = wxSTC_EOL_CR;
	}

	// Now set the status text, menu options, and the mode
	sqlQuery->SetEOLMode(mode);
	switch(mode)
	{

		case wxSTC_EOL_LF:
			lineEndMenu->Check(MNU_LF, true);
			SetStatusText(_("Unix"), STATUSPOS_FORMAT);
			break;

		case wxSTC_EOL_CRLF:
			lineEndMenu->Check(MNU_CRLF, true);
			SetStatusText(_("DOS"), STATUSPOS_FORMAT);
			break;

		case wxSTC_EOL_CR:
			lineEndMenu->Check(MNU_CR, true);
			SetStatusText(_("Mac"), STATUSPOS_FORMAT);
			break;

		default:
			wxLogError(wxT("Someone created a new line ending style! Run, run for your lives!!"));
	}

	delete reCRLF;
	delete reCR;
	delete reLF;
}


// Get the line ending style
int frmQuery::GetLineEndingStyle()
{
	if (lineEndMenu->IsChecked(MNU_LF))
		return wxSTC_EOL_LF;
	else if (lineEndMenu->IsChecked(MNU_CRLF))
		return wxSTC_EOL_CRLF;
	else if (lineEndMenu->IsChecked(MNU_CR))
		return wxSTC_EOL_CR;
	else
		return sqlQuery->GetEOLMode();
}


// User-set the current EOL mode for the form
void frmQuery::OnSetEOLMode(wxCommandEvent &event)
{
	int mode = GetLineEndingStyle();
	sqlQuery->ConvertEOLs(mode);
	sqlQuery->SetEOLMode(mode);
	settings->SetLineEndingType(mode);

	SetEOLModeDisplay(mode);

	if (!changed)
	{
		changed = true;
		setExtendedTitle();
	}

	pgScript->SetConnection(conn);
}


// Display the EOL mode settings on the form
void frmQuery::SetEOLModeDisplay(int mode)
{
	switch(mode)
	{

		case wxSTC_EOL_LF:
			lineEndMenu->Check(MNU_LF, true);
			SetStatusText(_("Unix"), STATUSPOS_FORMAT);
			break;

		case wxSTC_EOL_CRLF:
			lineEndMenu->Check(MNU_CRLF, true);
			SetStatusText(_("DOS"), STATUSPOS_FORMAT);
			break;

		case wxSTC_EOL_CR:
			lineEndMenu->Check(MNU_CR, true);
			SetStatusText(_("Mac"), STATUSPOS_FORMAT);
			break;

		default:
			wxLogError(wxT("Someone created a new line ending style! Run, run for your lives!!"));
	}
}


void frmQuery::OnSaveAs(wxCommandEvent &event)
{
#ifdef __WXMSW__
	wxFileDialog *dlg = new wxFileDialog(this, _("Save query file as"), lastDir, lastFilename,
	                                     _("Query files (*.sql)|*.sql|All files (*.*)|*.*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
#else
	wxFileDialog *dlg = new wxFileDialog(this, _("Save query file as"), lastDir, lastFilename,
	                                     _("Query files (*.sql)|*.sql|All files (*)|*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
#endif
	if (dlg->ShowModal() == wxID_OK)
	{
		lastFilename = dlg->GetFilename();
		lastDir = dlg->GetDirectory();
		lastPath = dlg->GetPath();
		switch (dlg->GetFilterIndex())
		{
			case 0:
#ifdef __WXMAC__
				if (!lastPath.Contains(wxT(".")))
					lastPath += wxT(".sql");
#endif
				break;
			case 1:
#ifdef __WXMAC__
				if (!lastPath.Contains(wxT(".")))
					lastPath += wxT(".sql");
#endif
				break;
			default:
				break;
		}

		lastFileFormat = settings->GetUnicodeFile();

		wxUtfFile file(lastPath, wxFile::write, lastFileFormat ? wxFONTENCODING_UTF8 : wxFONTENCODING_DEFAULT);
		if (file.IsOpened())
		{
			if ((file.Write(sqlQuery->GetText()) == 0) && (!lastFileFormat))
				wxMessageBox(_("Query text incomplete.\nQuery contained characters that could not be converted to the local charset.\nPlease correct the data or try using UTF8 instead."));
			file.Close();
			changed = false;

			// Forget about Initial origin thus making "Save" button behave as usual
			// (be enabled/disabled according to dirty flag only).
			if (origin == ORIGIN_INITIAL)
				origin = ORIGIN_FILE;

			setExtendedTitle();
			UpdateRecentFiles();
			fileMenu->Enable(MNU_RECENT, (recentFileMenu->GetMenuItemCount() > 0));
		}
		else
		{
			wxLogError(__("Could not write the file %s: Errcode=%d."), lastPath.c_str(), wxSysErrorCode());
		}
	}
	delete dlg;
}


void frmQuery::OnQuickReport(wxCommandEvent &event)
{
	wxDateTime now = wxDateTime::Now();

	frmReport *rep = new frmReport(this);

	rep->XmlAddHeaderValue(wxT("generated"), now.Format(wxT("%c")));
	rep->XmlAddHeaderValue(wxT("database"), conn->GetName());

	rep->SetReportTitle(_("Quick report"));

	int section = rep->XmlCreateSection(_("Query results"));

	rep->XmlAddSectionTableFromGrid(section, sqlResult);

	wxString stats;
	stats.Printf(wxT("%ld rows with %d columns retrieved."), sqlResult->NumRows(), sqlResult->GetNumberCols());

	rep->XmlSetSectionTableInfo(section, stats);

	wxString query = sqlQuery->GetSelectedText();
	if (query.IsNull())
		query = sqlQuery->GetText();

	rep->XmlSetSectionSql(section, query);

	rep->ShowModal();
}


void frmQuery::OnCancel(wxCommandEvent &event)
{
	toolBar->EnableTool(MNU_CANCEL, false);
	queryMenu->Enable(MNU_CANCEL, false);
	SetStatusText(_("Cancelling."), STATUSPOS_MSGS);

	if (sqlResult->RunStatus() == CTLSQL_RUNNING)
		sqlResult->Abort();
	else if (pgScript->IsRunning())
		pgScript->Terminate();

	QueryExecInfo *qi = (QueryExecInfo *)event.GetClientData();
	if (qi)
		delete qi;

	aborted = true;
}


void frmQuery::OnExplain(wxCommandEvent &event)
{
	if(sqlNotebook->GetSelection() == 1)
	{
		if (!updateFromGqb(true))
			return;
	}

	wxString query = sqlQuery->GetSelectedText();
	if (query.IsNull())
		query = sqlQuery->GetText();

	if (query.IsNull())
		return;
	wxString sql;
	int resultToRetrieve = 1;
	bool verbose = queryMenu->IsChecked(MNU_VERBOSE);
	bool analyze = event.GetId() == MNU_EXPLAINANALYZE;

	if (analyze)
	{
		sql += wxT("\nBEGIN;\n");
		resultToRetrieve++;
	}
	sql += wxT("EXPLAIN ");
	if (conn->BackendMinimumVersion(9, 0))
	{
		bool costs = queryMenu->IsChecked(MNU_COSTS);
		bool buffers = queryMenu->IsChecked(MNU_BUFFERS) && analyze;
		bool timing = queryMenu->IsChecked(MNU_TIMING) && analyze;

		sql += wxT("(");
		if (analyze)
			sql += wxT("ANALYZE on, ");
		else
			sql += wxT("ANALYZE off, ");
		if (verbose)
			sql += wxT("VERBOSE on, ");
		else
			sql += wxT("VERBOSE off, ");
		if (costs)
			sql += wxT("COSTS on, ");
		else
			sql += wxT("COSTS off, ");
		if (buffers)
			sql += wxT("BUFFERS on");
		else
			sql += wxT("BUFFERS off");
		if (conn->BackendMinimumVersion(9, 2))
		{
			if (timing)
				sql += wxT(", TIMING on ");
			else
				sql += wxT(", TIMING off ");
		}
		sql += wxT(")");
	}
	else
	{
		if (analyze)
			sql += wxT("ANALYZE ");
		if (verbose)
			sql += wxT("VERBOSE ");
	}

	int offset = sql.Length();

	sql += query;

	if (analyze)
	{
		// Bizarre bug fix - if we append a rollback directly after -- it'll crash!!
		// Add a \n first.
		sql += wxT("\n;\nROLLBACK;");
	}

	execQuery(sql, resultToRetrieve, true, offset, false, true, verbose);
}

// Update the main SQL query from the GQB if desired
bool frmQuery::updateFromGqb(bool executing)
{
	if (closing)
		return false;

	// Make sure this doesn't get call recursively through an event
	if (gqbUpdateRunning)
		return false;
	updateMenu();

	gqbUpdateRunning = true;

	// Execute Generation of SQL sentence from GQB
	bool canGenerate = false;
	wxString newQuery = controller->generateSQL();

	// If the new query is empty, don't do anything
	if (newQuery.IsEmpty())
	{
		if (controller->getTableCount() > 0)
		{
			wxMessageBox(_("No SQL query was generated."), _("Graphical Query Builder"), wxICON_INFORMATION | wxOK);
		}
		gqbUpdateRunning = false;
		return false;
	}

	// Only prompt the user if the dirty flag is set, and last modification wasn't from GQB,
	// and the textbox is not empty, and the new query is different.
	if(changed && origin != ORIGIN_GQB &&
	        !sqlQuery->GetText().Trim().IsEmpty() && sqlQuery->GetText() != newQuery + wxT("\n"))
	{
		wxString fn;
		if (executing)
			fn = _("The generated SQL query has changed.\nDo you want to update it and execute the query?");
		else
			fn = _("The generated SQL query has changed.\nDo you want to update it?");

		wxMessageDialog msg(this, fn, _("Query"), wxYES_NO | wxICON_EXCLAMATION);
		if(msg.ShowModal() == wxID_YES && changed)
		{
			canGenerate = true;
		}
		else
		{
			gqbUpdateRunning = false;
		}
	}
	else
	{
		canGenerate = true;
	}

	if(canGenerate)
	{
		sqlQuery->SetText(newQuery + wxT("\n"));
		sqlQuery->Colourise(0, sqlQuery->GetText().Length());
		wxSafeYield();                            // needed to process sqlQuery modify event
		sqlNotebook->SetSelection(0);
		changed = true;
		origin = ORIGIN_GQB;
		setExtendedTitle();

		gqbUpdateRunning = false;
		return true;
	}

	return false;
}

void frmQuery::OnExecute(wxCommandEvent &event)
{
	if(sqlNotebook->GetSelection() == 1)
	{
		if (!updateFromGqb(true))
			return;
	}

	wxString query = sqlQuery->GetSelectedText();
	if (query.IsNull())
		query = sqlQuery->GetText();

	if (query.IsNull())
		return;

	execQuery(query);
	sqlQuery->SetFocus();
}


void frmQuery::OnExecScript(wxCommandEvent &event)
{
	// Get the script
	wxString query = sqlQuery->GetSelectedText();
	if (query.IsNull())
		query = sqlQuery->GetText();
	if (query.IsNull())
		return;

	// Make sure pgScript is not already running
	// Required because the pgScript parser isn't currently thread-safe :-(
	if (frmQuery::ms_pgScriptRunning == true)
	{
		wxMessageBox(_("pgScript already running."), _("Concurrent execution of pgScripts is not supported at this time."), wxICON_WARNING | wxOK);
		return;
	}
	frmQuery::ms_pgScriptRunning = true;

	// Clear markers and indicators
	sqlQuery->MarkerDeleteAll(0);
	sqlQuery->StartStyling(0, wxSTC_INDICS_MASK);
	sqlQuery->SetStyling(sqlQuery->GetText().Length(), 0);

	// Menu stuff to initialize
	setTools(true);
	queryMenu->Enable(MNU_SAVEHISTORY, true);
	queryMenu->Enable(MNU_CLEARHISTORY, true);

	// Window stuff
	explainCanvas->Clear();
	msgResult->Clear();
	msgResult->SetFont(settings->GetSQLFont());
	outputPane->SetSelection(2);

	// Status text
	SetStatusText(wxT(""), STATUSPOS_SECS);
	SetStatusText(_("pgScript is running."), STATUSPOS_MSGS);
	SetStatusText(wxT(""), STATUSPOS_ROWS);

	// History
	msgHistory->AppendText(_("-- Executing pgScript\n"));
	Update();
	wxTheApp->Yield(true);

	// Timer
	startTimeQuery = wxGetLocalTimeMillis();
	timer.Start(10);

	// Delete previous variables
	pgScript->ClearSymbols();

	// Parse script. Note that we add \n so the parse can correctly identify
	// a comment on the last line of the query.
	pgScript->ParseString(query + wxT("\n"), pgsOutput);
	pgsTimer->Start(20);
	aborted = false;
}



void frmQuery::OnExecFile(wxCommandEvent &event)
{
	if(sqlNotebook->GetSelection() == 1)
	{
		if (!updateFromGqb(true))
			return;
	}

	wxString query = sqlQuery->GetSelectedText();
	if (query.IsNull())
		query = sqlQuery->GetText();

	if (query.IsNull())
		return;

	execQuery(query, 0, false, 0, true);
	sqlQuery->SetFocus();
}


void frmQuery::OnMacroManage(wxCommandEvent &event)
{
	int r = dlgManageMacros(this, mainForm, macros).ManageMacros();
	if (r == 1)
	{
		// Changed something, so save
		queryMacroFileProvider::SaveMacros(macros);
	}
	if (r == -1 || r == 1)
	{
		// Changed something requiring rollback
		mainForm->UpdateAllMacrosList();
	}

}


void frmQuery::OnMacroInvoke(wxCommandEvent &event)
{
	queryMacroItem *mac;

	mac = macros->FindMacro(event.GetId());
	if (!mac)
		return;

	wxString query = mac->GetQuery();
	if (query.IsEmpty())
		return;            // do not execute empty query

	if (query.Find(wxT("$SELECTION$")) != wxNOT_FOUND)
	{
		wxString selection = sqlQuery->GetSelectedText();
		if (selection.IsEmpty())
		{
			wxMessageBox(_("This macro includes a text substitution. Please select some text in the SQL pane and re-run the macro."), _("Execute macro"), wxICON_EXCLAMATION | wxOK);
			return;
		}
		query.Replace(wxT("$SELECTION$"), selection);
	}
	execQuery(query);
	sqlQuery->SetFocus();
}


void frmQuery::setTools(const bool running)
{
	toolBar->EnableTool(MNU_EXECUTE, !running);
	toolBar->EnableTool(MNU_EXECPGS, !running);
	toolBar->EnableTool(MNU_EXECFILE, !running);
	toolBar->EnableTool(MNU_EXPLAIN, !running);
	toolBar->EnableTool(MNU_CANCEL, running);
	queryMenu->Enable(MNU_EXECUTE, !running);
	queryMenu->Enable(MNU_EXECPGS, !running);
	queryMenu->Enable(MNU_EXECFILE, !running);
	queryMenu->Enable(MNU_EXPLAIN, !running);
	queryMenu->Enable(MNU_EXPLAINANALYZE, !running);
	queryMenu->Enable(MNU_CANCEL, running);
	fileMenu->Enable(MNU_EXPORT, sqlResult->CanExport());
	fileMenu->Enable(MNU_QUICKREPORT, sqlResult->CanExport());
	fileMenu->Enable(MNU_RECENT, (recentFileMenu->GetMenuItemCount() > 0));
	sqlQuery->EnableAutoComp(running);
}


void frmQuery::showMessage(const wxString &msg, const wxString &msgShort)
{
	msgResult->AppendText(msg + wxT("\n"));
	msgHistory->AppendText(msg + wxT("\n"));
	wxString str;
	if (msgShort.IsNull())
		str = msg;
	else
		str = msgShort;
	str.Replace(wxT("\n"), wxT(" "));
	SetStatusText(str, STATUSPOS_MSGS);
}


void frmQuery::execQuery(const wxString &query, int resultToRetrieve, bool singleResult, const int queryOffset, bool toFile, bool explain, bool verbose)
{
	setTools(true);
	queryMenu->Enable(MNU_SAVEHISTORY, true);
	queryMenu->Enable(MNU_CLEARHISTORY, true);

	explainCanvas->Clear();

	// Clear markers and indicators
	sqlQuery->MarkerDeleteAll(0);
	sqlQuery->StartStyling(0, wxSTC_INDICS_MASK);
	sqlQuery->SetStyling(sqlQuery->GetText().Length(), 0);

	if (!changed)
		setExtendedTitle();

	aborted = false;

	QueryExecInfo *qi = new QueryExecInfo();
	qi->queryOffset = queryOffset;
	qi->toFileExportForm = NULL;
	qi->singleResult = singleResult;
	qi->explain = explain;
	qi->verbose = verbose;

	if (toFile)
	{
		qi->toFileExportForm = new frmExport(this);
		if (qi->toFileExportForm->ShowModal() != wxID_OK)
		{
			delete qi;
			setTools(false);
			aborted = true;
			return;
		}
	}

	// We must do this lot before the query starts, otherwise
	// it might not happen once the main thread gets busy with
	// other stuff.
	SetStatusText(wxT(""), STATUSPOS_SECS);
	SetStatusText(_("Query is running."), STATUSPOS_MSGS);
	SetStatusText(wxT(""), STATUSPOS_ROWS);
	msgResult->Clear();
	msgResult->SetFont(settings->GetSQLFont());

	msgHistory->AppendText(_("-- Executing query:\n"));
	msgHistory->AppendText(query);
	msgHistory->AppendText(wxT("\n"));
	Update();
	wxTheApp->Yield(true);

	startTimeQuery = wxGetLocalTimeMillis();
	timer.Start(10);

	if (sqlResult->Execute(query, resultToRetrieve, this, QUERY_COMPLETE, qi) >= 0)
	{
		// Return and wait for the result
		return;
	}

	completeQuery(false, false, false);
}


// When the query completes, it raises an event which we process here.
void frmQuery::OnQueryComplete(pgQueryResultEvent &ev)
{
	QueryExecInfo *qi = (QueryExecInfo *)ev.GetClientData();

	bool done = false;

	while (sqlResult->RunStatus() == CTLSQL_RUNNING)
	{
		wxTheApp->Yield(true);
	}

	while (pgScript->IsRunning())
	{
		wxLogInfo(wxT("SQL Query box: Waiting for script to abort"));
		wxSleep(1);
	}

	timer.Stop();

	wxString str;
	str = sqlResult->GetMessagesAndClear();
	msgResult->AppendText(str);
	msgHistory->AppendText(str);

	elapsedQuery = wxGetLocalTimeMillis() - startTimeQuery;
	SetStatusText(elapsedQuery.ToString() + wxT(" ms"), STATUSPOS_SECS);

	if (sqlResult->RunStatus() != PGRES_TUPLES_OK)
	{
		outputPane->SetSelection(2);
		if (sqlResult->RunStatus() == PGRES_COMMAND_OK)
		{
			done = true;

			int insertedCount = sqlResult->InsertedCount();
			OID insertedOid = sqlResult->InsertedOid();
			if (insertedCount < 0)
			{
				showMessage(wxString::Format(_("Query returned successfully with no result in %s ms."),
				                             elapsedQuery.ToString().c_str()), _("OK."));
			}
			else if (insertedCount == 1)
			{
				if (insertedOid)
				{
					showMessage(wxString::Format(_("Query returned successfully: one row with OID %ld inserted, %s ms execution time."),
					                             (long)insertedOid, elapsedQuery.ToString().c_str()),
					            wxString::Format(_("One row with OID %ld inserted."), (long)insertedOid));
				}
				else
				{
					showMessage(wxString::Format(_("Query returned successfully: one row affected, %s ms execution time."),
					                             elapsedQuery.ToString().c_str()),
					            wxString::Format(_("One row affected.")));
				}
			}
			else
			{
				showMessage(wxString::Format(_("Query returned successfully: %d rows affected, %s ms execution time."),
				                             insertedCount, elapsedQuery.ToString().c_str()),
				            wxString::Format(_("%d rows affected."), insertedCount));
			}
		}
		else if (sqlResult->RunStatus() == PGRES_EMPTY_QUERY)
		{
			showMessage(_("Empty query, no results."));
		}
		else if (ev.GetInt() == pgQueryResultEvent::PGQ_EXECUTION_CANCELLED)
		{
			showMessage(_("Execution Cancelled!"));
		}
		else
		{
			wxString errMsg, errMsg2;
			long errPos;

			pgError err = sqlResult->GetResultError();
			errMsg = err.formatted_msg;
			wxLogQuietError(wxT("%s"), conn->GetLastError().Trim().c_str());
			err.statement_pos.ToLong(&errPos);

			if (err.sql_state.IsEmpty())
			{
				if (wxMessageBox(_("Do you want to attempt to reconnect to the database?"),
				                 wxString::Format(_("Connection to database %s lost."), conn->GetDbname().c_str()),
				                 wxICON_EXCLAMATION | wxYES_NO) == wxYES)
				{
					conn->Reset();
					errMsg2 = _("Connection reset.");
				}
			}

			showMessage(wxString::Format(wxT("********** %s **********\n"), _("Error")));
			showMessage(errMsg);
			if (!errMsg2.IsEmpty())
				showMessage(errMsg2);

			if (errPos > 0)
			{
				int selStart = sqlQuery->GetSelectionStart(), selEnd = sqlQuery->GetSelectionEnd();
				if (selStart == selEnd)
					selStart = 0;

				errPos -= qi->queryOffset;        // do not count EXPLAIN or similar

				// Set an indicator on the error word (break on any kind of bracket, a space or full stop)
				int sPos = errPos + selStart - 1, wEnd = 1;
				sqlQuery->StartStyling(sPos, wxSTC_INDICS_MASK);
				int c = sqlQuery->GetCharAt(sPos + wEnd);
				size_t len = sqlQuery->GetText().Length();
				while(c != ' ' && c != '(' && c != '{' && c != '[' && c != '.' &&
				        (unsigned int)(sPos + wEnd) < len)
				{
					wEnd++;
					c = sqlQuery->GetCharAt(sPos + wEnd);
				}
				sqlQuery->SetStyling(wEnd, wxSTC_INDIC0_MASK);

				int line = 0, maxLine = sqlQuery->GetLineCount();
				while (line < maxLine && sqlQuery->GetLineEndPosition(line) < errPos + selStart + 1)
					line++;
				if (line < maxLine)
				{
					sqlQuery->GotoPos(sPos);
					sqlQuery->MarkerAdd(line, 0);

					if (!changed)
						setExtendedTitle();

					sqlQuery->EnsureVisible(line);
				}
			}
		}
	}
	else
	{
		done = true;
		outputPane->SetSelection(0);
		long rowsTotal = sqlResult->NumRows();

		if (qi->toFileExportForm)
		{
			SetStatusText(wxString::Format(wxPLURAL("%d row.", "%d rows.", rowsTotal), rowsTotal), STATUSPOS_ROWS);

			if (rowsTotal)
			{
				SetStatusText(_("Writing data."), STATUSPOS_MSGS);

				toolBar->EnableTool(MNU_CANCEL, false);
				queryMenu->Enable(MNU_CANCEL, false);
				SetCursor(*wxHOURGLASS_CURSOR);

				if (sqlResult->ToFile(qi->toFileExportForm))
					SetStatusText(_("Data written to file."), STATUSPOS_MSGS);
				else
					SetStatusText(_("Data export aborted."), STATUSPOS_MSGS);
				SetCursor(wxNullCursor);
			}
			else
				SetStatusText(_("No data to export."), STATUSPOS_MSGS);
		}
		else
		{
			if (qi->singleResult)
			{
				sqlResult->DisplayData(true);

				showMessage(wxString::Format(
				                wxPLURAL("%ld row retrieved.", "%ld rows retrieved.",
				                         sqlResult->NumRows()), sqlResult->NumRows()),
				            _("OK."));
			}
			else
			{
				SetStatusText(wxString::Format(wxPLURAL("Retrieving data: %d row.", "Retrieving data: %d rows.", (int)rowsTotal), (int)rowsTotal), STATUSPOS_MSGS);
				wxTheApp->Yield(true);

				sqlResult->DisplayData();

				SetStatusText(elapsedQuery.ToString() + wxT(" ms"), STATUSPOS_SECS);

				str = _("Total query runtime: ") + elapsedQuery.ToString() + wxT(" ms.\n") ;
				msgResult->AppendText(str);
				msgHistory->AppendText(str);

				showMessage(wxString::Format(wxPLURAL("%d row retrieved.", "%d rows retrieved.", (int)sqlResult->NumRows()), (int)sqlResult->NumRows()), _("OK."));
			}
			SetStatusText(wxString::Format(wxPLURAL("%ld row.", "%ld rows.", rowsTotal), rowsTotal), STATUSPOS_ROWS);
		}
	}

	if (sqlResult->RunStatus() == PGRES_TUPLES_OK || sqlResult->RunStatus() == PGRES_COMMAND_OK)
	{
		// Get the executed query
		wxString executedQuery = sqlQuery->GetSelectedText();
		if (executedQuery.IsNull())
			executedQuery = sqlQuery->GetText();

		// Same query, but without return feeds and carriage returns
		wxString executedQueryWithoutReturns = executedQuery;
		executedQueryWithoutReturns.Replace(wxT("\n"), wxT(" "));
		executedQueryWithoutReturns.Replace(wxT("\r"), wxT(" "));
		executedQueryWithoutReturns = executedQueryWithoutReturns.Trim();

		if (executedQuery.Len() < (unsigned int)settings->GetHistoryMaxQuerySize())
		{
			// We put in the combo box the query without returns...
			sqlQueries->Append(executedQueryWithoutReturns);

			// .. but we save the query with returns in the array
			// (so that we have the real query in the file)
			histoQueries.Add(executedQuery);

			// Finally, we save the queries
			SaveQueries();
		}

		// Search a matching old query
		unsigned int index = 0;
		bool found = false;
		while (!found && index < sqlQueries->GetCount())
		{
			found = sqlQueries->GetString(index) == executedQueryWithoutReturns;
			if (!found)
				index++;
		}

		// If we found one, delete it from the combobox and the array
		if (found && index < (unsigned int)sqlQueries->GetCount() - 1)
		{
			histoQueries.RemoveAt(index);
			sqlQueries->Delete(index);
		}
	}

	// Make sure only the maximum query number is enforced
	while (sqlQueries->GetCount() > (unsigned int)settings->GetHistoryMaxQueries())
	{
		histoQueries.RemoveAt(0);
		sqlQueries->Delete(0);
	}

	SaveQueries();

	completeQuery(done, qi->explain, qi->verbose);
	delete qi;
}


void frmQuery::OnScriptComplete(wxCommandEvent &ev)
{
	// Stop timers
	timer.Stop();
	pgsTimer->Stop();

	// Write output
	writeScriptOutput();

	// Reset tools
	setTools(false);

	// Unlock our pseudo-mutex thingy
	frmQuery::ms_pgScriptRunning = false;

	// Manage timer
	elapsedQuery = wxGetLocalTimeMillis() - startTimeQuery;
	SetStatusText(elapsedQuery.ToString() + wxT(" ms"), STATUSPOS_SECS);
	SetStatusText(_("pgScript completed."), STATUSPOS_MSGS);
	wxString str = _("Total pgScript runtime: ") + elapsedQuery.ToString() + wxT(" ms.\n\n");
	msgHistory->AppendText(str);

	// Check whether there was an error/exception
	if (pgScript->errorOccurred() && pgScript->errorLine() >= 1)
	{
		// Find out what the line number is
		int selStart = sqlQuery->GetSelectionStart(), selEnd = sqlQuery->GetSelectionEnd();
		if (selStart == selEnd)
			selStart = 0;
		int line = 0, maxLine = sqlQuery->GetLineCount();
		while (line < maxLine && sqlQuery->GetLineEndPosition(line) < selStart)
			line++;
		line += pgScript->errorLine() - 1;

		// Mark the line where the error occurred
		sqlQuery->MarkerAdd(line, 0);

		// Go to that line
		sqlQuery->GotoPos(sqlQuery->GetLineEndPosition(line));
	}
}

void frmQuery::writeScriptOutput()
{
	pgScript->LockOutput();

	wxString output(pgsOutputString);
	pgsOutputString.Clear();
	msgResult->AppendText(output);

	pgScript->UnlockOutput();
}

// Complete the processing of a query
void frmQuery::completeQuery(bool done, bool explain, bool verbose)
{
	// Display async notifications
	pgNotification *notify;
	int notifies = 0;
	notify = conn->GetNotification();
	while (notify)
	{
		wxString notifyStr;
		notifies++;

		if (notify->data.IsEmpty())
			notifyStr.Printf(_("\nAsynchronous notification of '%s' received from backend pid %d"), notify->name.c_str(), notify->pid);
		else
			notifyStr.Printf(_("\nAsynchronous notification of '%s' received from backend pid %d\n   Data: %s"), notify->name.c_str(), notify->pid, notify->data.c_str());

		msgResult->AppendText(notifyStr);
		msgHistory->AppendText(notifyStr);

		notify = conn->GetNotification();
	}

	if (notifies)
	{
		wxString statusMsg = statusBar->GetStatusText(STATUSPOS_MSGS);
		if (statusMsg.Last() == '.')
			statusMsg = statusMsg.Left(statusMsg.Length() - 1);

		SetStatusText(wxString::Format(
		                  wxPLURAL("%s (%d asynchronous notification received).", "%s (%d asynchronous notifications received).", notifies),
		                  statusMsg.c_str(), notifies), STATUSPOS_MSGS);
	}

	msgResult->AppendText(wxT("\n"));
	msgResult->ShowPosition(0);
	msgHistory->AppendText(wxT("\n"));
	msgHistory->ShowPosition(0);

	// If the transaction aborted for some reason, issue a rollback to cleanup.
	if (settings->GetAutoRollback() && conn->GetTxStatus() == PGCONN_TXSTATUS_INERROR)
		conn->ExecuteVoid(wxT("ROLLBACK;"));

	setTools(false);
	fileMenu->Enable(MNU_EXPORT, sqlResult->CanExport());

	if (!IsActive() || IsIconized())
		RequestUserAttention();

	if (!viewMenu->IsChecked(MNU_OUTPUTPANE))
	{
		viewMenu->Check(MNU_OUTPUTPANE, true);
		manager.GetPane(wxT("outputPane")).Show(true);
		manager.Update();
	}

	// If this was an EXPLAIN query, process the results
	if (done && explain)
	{
		if (!verbose || conn->BackendMinimumVersion(8, 4))
		{
			int i;
			wxString str;
			if (sqlResult->NumRows() == 1)
			{
				// Avoid shared storage issues with strings
				str.Append(sqlResult->OnGetItemText(0, 0).c_str());
			}
			else
			{
				for (i = 0 ; i < sqlResult->NumRows() ; i++)
				{
					if (i)
						str.Append(wxT("\n"));
					str.Append(sqlResult->OnGetItemText(i, 0));
				}
			}
			explainCanvas->SetExplainString(str);
			outputPane->SetSelection(1);
		}
		updateMenu();
	}

	sqlQuery->SetFocus();
}


void frmQuery::OnTimer(wxTimerEvent &event)
{
	elapsedQuery = wxGetLocalTimeMillis() - startTimeQuery;
	SetStatusText(elapsedQuery.ToString() + wxT(" ms"), STATUSPOS_SECS);

	wxString str = sqlResult->GetMessagesAndClear();
	if (!str.IsEmpty())
	{
		msgResult->AppendText(str + wxT("\n"));
		msgHistory->AppendText(str + wxT("\n"));
	}

	// Increase the granularity for longer running queries
	if (elapsedQuery > 200 && timer.GetInterval() == 10 && timer.IsRunning())
	{
		timer.Stop();
		timer.Start(100);
	}
}

// Adjust sizes of GQB components, Located here because need to
// avoid some issues when implementing inside controller/view Classes
void frmQuery::adjustGQBSizes()
{
	// Get Size (only height) from main Tab with GQB and SQL Editor and adjust the width
	// to desiree, then set [Sash of tablesBrowser | GQB_Canvas]
	manager.Update();
	sqlNotebook->Refresh();
	wxSize s = sqlNotebook->GetSize();
	s.SetWidth(200);
	s.SetHeight(s.GetHeight() - 180);      //re-adjust weight eliminating Horz Sash Position
	controller->getTablesBrowser()->SetSize(s);
	controller->setSashVertPosition(controller->getTablesBrowser()->GetSize().GetWidth());

	// Now Adjust Sash Horizontal
	s = sqlNotebook->GetSize();
	controller->setSashHorizPosition(s.GetHeight() - 150);

	// Adjust GQB grids internal columns sizes
	controller->calcGridColsSizes();
}


// Adjust sizes of GQB components after vertical sash adjustment,
// Located here because need to avoid some issues when implementing
// inside controller/view Classes
void frmQuery::OnResizeHorizontally(wxSplitterEvent &event)
{
	int y = event.GetSashPosition();
	wxSize s = controller->getTablesBrowser()->GetSize();
	s.SetHeight(y);               // re-adjust weight eliminating Horz Sash Position
	controller->getTablesBrowser()->SetSize(s);
}



// This function adjust the GQB Components after an event on the wxAui
// event, it's a workaround because need event finish to work properly
void frmQuery::OnAdjustSizesTimer(wxTimerEvent &event)
{
	adjustGQBSizes();
	adjustSizesTimer->Stop();
}

void frmQuery::OnBlockIndent(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->CmdKeyExecute(wxSTC_CMD_TAB);
	else if (FindFocus()->GetId() == CTL_SCRATCHPAD)
		scratchPad->WriteText(wxT("\t"));
}

void frmQuery::OnBlockOutDent(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->CmdKeyExecute(wxSTC_CMD_BACKTAB);
}

void frmQuery::OnChangeToUpperCase(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->UpperCase();
}

void frmQuery::OnChangeToLowerCase(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->LowerCase();
}

void frmQuery::OnCommentText(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->BlockComment(false);
}

void frmQuery::OnUncommentText(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->BlockComment(true);
}

void frmQuery::OnExternalFormat(wxCommandEvent &event)
{
	if (FindFocus()->GetId() == CTL_SQLQUERY)
		sqlQuery->ExternalFormat();
}

wxBitmap frmQuery::CreateBitmap(const wxColour &colour)
{
	const int w = 10, h = 10;

	wxMemoryDC dc;
	wxBitmap bmp(w, h);
	dc.SelectObject(bmp);
	if (colour == wxNullColour)
		dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
	else
		dc.SetBrush(wxBrush(colour));
	dc.DrawRectangle(0, 0, w, h);

	return bmp;
}

wxColour frmQuery::GetServerColour(pgConn *connection)
{
	wxColour tmp = wxNullColour;
	if (mainForm != NULL)
	{
		ctlTree *browser = mainForm->GetBrowser();
		wxTreeItemIdValue foldercookie, servercookie;
		wxTreeItemId folderitem, serveritem;
		pgObject *object;
		pgServer *server;

		folderitem = browser->GetFirstChild(browser->GetRootItem(), foldercookie);
		while (folderitem)
		{
			if (browser->ItemHasChildren(folderitem))
			{
				serveritem = browser->GetFirstChild(folderitem, servercookie);
				while (serveritem)
				{
					object = browser->GetObject(serveritem);
					if (object && object->IsCreatedBy(serverFactory))
					{
						server = (pgServer *)object;
						if (server->GetConnected() &&
						        server->GetConnection()->GetHost() == connection->GetHost() &&
						        server->GetConnection()->GetPort() == connection->GetPort())
						{
							tmp = wxColour(server->GetColour());
						}
					}
					serveritem = browser->GetNextChild(folderitem, servercookie);
				}
			}
			folderitem = browser->GetNextChild(browser->GetRootItem(), foldercookie);
		}
	}
	return tmp;
}

void frmQuery::LoadQueries()
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *key;

	if (!wxFile::Access(settings->GetHistoryFile(), wxFile::read))
		return;

	doc = xmlParseFile((const char *)settings->GetHistoryFile().mb_str(wxConvUTF8));
	if (doc == NULL)
	{
		wxMessageBox(_("Failed to load the history file!"));
		::wxRemoveFile(settings->GetHistoryFile());
		return;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL)
	{
		xmlFreeDoc(doc);
		return;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "histoqueries"))
	{
		wxMessageBox(_("Failed to load the history file!"));
		xmlFreeDoc(doc);
		::wxRemoveFile(settings->GetHistoryFile());
		return;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"histoquery")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

			if (key)
			{
				if (WXSTRING_FROM_XML(key) != wxT(""))
				{
					wxString query = WXSTRING_FROM_XML(key);
					wxString tmp = query;
					tmp.Replace(wxT("\n"), wxT(" "));
					tmp.Replace(wxT("\r"), wxT(" "));
					sqlQueries->Append(tmp);
					histoQueries.Add(query);
				}
				xmlFree(key);
			}
		}

		cur = cur->next;
	}

	xmlFreeDoc(doc);

	// Make sure only the maximum query number is enforced
	if (sqlQueries->GetCount() > (unsigned int)settings->GetHistoryMaxQueries())
	{
		while (sqlQueries->GetCount() > (unsigned int)settings->GetHistoryMaxQueries())
		{
			histoQueries.RemoveAt(0);
			sqlQueries->Delete(0);
		}
		SaveQueries();
	}

	return;
}


void frmQuery::SaveQueries()
{
	size_t i;
	xmlTextWriterPtr writer;

	writer = xmlNewTextWriterFilename((const char *)settings->GetHistoryFile().mb_str(wxConvUTF8), 0);
	if (!writer)
	{
		wxMessageBox(_("Failed to write to history file!"));
		return;
	}
	xmlTextWriterSetIndent(writer, 1);

	if ((xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL) < 0) ||
	        (xmlTextWriterStartElement(writer, XML_STR("histoqueries")) < 0))
	{
		wxMessageBox(_("Failed to write to history file!"));
		xmlFreeTextWriter(writer);
		return;
	}

	for (i = 0; i < histoQueries.GetCount(); i++)
	{
		xmlTextWriterStartElement(writer, XML_STR("histoquery"));
		xmlTextWriterWriteString(writer, XML_FROM_WXSTRING(histoQueries.Item(i)));
		xmlTextWriterEndElement(writer);
	}

	if (xmlTextWriterEndDocument(writer) < 0)
	{
		wxMessageBox(_("Failed to write to history file!"));
	}

	xmlFreeTextWriter(writer);
}


void frmQuery::OnChangeQuery(wxCommandEvent &event)
{
	wxString query = histoQueries.Item(sqlQueries->GetSelection());
	if (query.Length() > 0)
	{
		sqlQuery->SetText(query);
		sqlQuery->Colourise(0, query.Length());
		wxSafeYield();                            // needed to process sqlQuery modify event
		changed = true;
		origin = ORIGIN_HISTORY;
		setExtendedTitle();
		SetLineEndingStyle();
		btnDeleteCurrent->Enable(true);
	}
	btnDeleteAll->Enable(sqlQueries->GetCount() > 0);
}


void frmQuery::OnDeleteCurrent(wxCommandEvent &event)
{

	if ( wxMessageDialog(this,
	                     _("Delete current query from history?"),
	                     _("Confirm deletion"),
	                     wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION).ShowModal() == wxID_YES )
	{
		histoQueries.RemoveAt(sqlQueries->GetSelection());
		sqlQueries->Delete(sqlQueries->GetSelection());
		sqlQueries->SetValue(wxT(""));
		btnDeleteCurrent->Enable(false);
		btnDeleteAll->Enable(sqlQueries->GetCount() > 0);
		SaveQueries();
	}
}


void frmQuery::OnDeleteAll(wxCommandEvent &event)
{

	if ( wxMessageDialog(this,
	                     _("Delete all queries from history?"),
	                     _("Confirm deletion"),
	                     wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION).ShowModal() == wxID_YES )
	{
		histoQueries.Clear();
		sqlQueries->Clear();
		sqlQueries->SetValue(wxT(""));
		btnDeleteCurrent->Enable(false);
		btnDeleteAll->Enable(false);
		SaveQueries();
	}
}


///////////////////////////////////////////////////////

wxWindow *queryToolBaseFactory::StartDialogSql(frmMain *form, pgObject *obj, const wxString &sql)
{
	pgDatabase *db = obj->GetDatabase();
	wxString applicationname = appearanceFactory->GetLongAppName() + _(" - Query Tool");
	pgConn *conn = db->CreateConn(applicationname);
	if (conn)
	{
		frmQuery *fq = new frmQuery(form, obj->GetDisplayName(), conn, sql);
		fq->Go();
		return fq;
	}
	return 0;
}


bool queryToolBaseFactory::CheckEnable(pgObject *obj)
{
	return obj && obj->GetDatabase() && obj->GetDatabase()->GetConnected();
}


bool queryToolDataFactory::CheckEnable(pgObject *obj)
{
	return queryToolBaseFactory::CheckEnable(obj) && !obj->IsCollection() &&
	       (obj->IsCreatedBy(tableFactory) || obj->IsCreatedBy(viewFactory));
}


queryToolFactory::queryToolFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolBaseFactory(list)
{
	mnu->Append(id, _("&Query tool\tCtrl-E"), _("Execute arbitrary SQL queries."));
	toolbar->AddTool(id, wxEmptyString, *sql_32_png_bmp, _("Execute arbitrary SQL queries."), wxITEM_NORMAL);
}


wxWindow *queryToolFactory::StartDialog(frmMain *form, pgObject *obj)
{
	wxString qry;
	if (settings->GetStickySql())
		qry = obj->GetSql(form->GetBrowser());
	return StartDialogSql(form, obj, qry);
}


queryToolSqlFactory::queryToolSqlFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolBaseFactory(list)
{
	mnu->Append(id, _("CREATE Script"), _("Start Query tool with CREATE script."));
	if (toolbar)
		toolbar->AddTool(id, wxEmptyString, *sql_32_png_bmp, _("Start query tool with CREATE script."), wxITEM_NORMAL);
}


wxWindow *queryToolSqlFactory::StartDialog(frmMain *form, pgObject *obj)
{
	return StartDialogSql(form, obj, obj->GetSql(form->GetBrowser()));
}


bool queryToolSqlFactory::CheckEnable(pgObject *obj)
{
	return queryToolBaseFactory::CheckEnable(obj) && obj->CanCreate() && !obj->IsCollection();
}


queryToolSelectFactory::queryToolSelectFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolDataFactory(list)
{
	mnu->Append(id, _("SELECT Script"), _("Start query tool with SELECT script."));
}

bool queryToolSelectFactory::CheckEnable(pgObject *obj)
{
	return queryToolBaseFactory::CheckEnable(obj) && !obj->IsCollection() &&
	       (obj->IsCreatedBy(tableFactory) || obj->IsCreatedBy(foreignTableFactory) || obj->IsCreatedBy(viewFactory) || obj->IsCreatedBy(functionFactory));
}

wxWindow *queryToolSelectFactory::StartDialog(frmMain *form, pgObject *obj)
{
	if (obj->IsCreatedBy(tableFactory))
	{
		pgTable *table = (pgTable *)obj;
		return StartDialogSql(form, obj, table->GetSelectSql(form->GetBrowser()));
	}
	else if (obj->IsCreatedBy(viewFactory))
	{
		pgView *view = (pgView *)obj;
		return StartDialogSql(form, obj, view->GetSelectSql(form->GetBrowser()));
	}
	else if (obj->IsCreatedBy(extTableFactory))
	{
		gpExtTable *exttable = (gpExtTable *)obj;
		return StartDialogSql(form, obj, exttable->GetSelectSql(form->GetBrowser()));
	}
	else if (obj->IsCreatedBy(functionFactory))
	{
		pgFunction *function = (pgFunction *)obj;
		return StartDialogSql(form, obj, function->GetSelectSql(form->GetBrowser()));
	}
	else if (obj->IsCreatedBy(foreignTableFactory))
	{
		pgForeignTable *foreigntable = (pgForeignTable *)obj;
		return StartDialogSql(form, obj, foreigntable->GetSelectSql(form->GetBrowser()));
	}
	return 0;
}

queryToolExecFactory::queryToolExecFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolDataFactory(list)
{
	mnu->Append(id, _("EXEC Script"), _("Start query tool with EXEC script."));
}

bool queryToolExecFactory::CheckEnable(pgObject *obj)
{
	return queryToolBaseFactory::CheckEnable(obj) && !obj->IsCollection() && obj->IsCreatedBy(procedureFactory);
}

wxWindow *queryToolExecFactory::StartDialog(frmMain *form, pgObject *obj)
{
	if (obj->IsCreatedBy(procedureFactory))
	{
		pgProcedure *procedure = (pgProcedure *)obj;
		return StartDialogSql(form, obj, procedure->GetExecSql(form->GetBrowser()));
	}
	return 0;
}

queryToolDeleteFactory::queryToolDeleteFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolDataFactory(list)
{
	mnu->Append(id, _("DELETE Script"), _("Start query tool with DELETE script."));
}


bool queryToolDeleteFactory::CheckEnable(pgObject *obj)
{
	if (!queryToolDataFactory::CheckEnable(obj))
		return false;
	if (obj->IsCreatedBy(tableFactory))
		return true;
	return false;
}


wxWindow *queryToolDeleteFactory::StartDialog(frmMain *form, pgObject *obj)
{
	if (obj->IsCreatedBy(tableFactory))
	{
		pgTable *table = (pgTable *)obj;
		return StartDialogSql(form, obj, table->GetDeleteSql(form->GetBrowser()));
	}
	return 0;
}


queryToolUpdateFactory::queryToolUpdateFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolDataFactory(list)
{
	mnu->Append(id, _("UPDATE Script"), _("Start query tool with UPDATE script."));
}


wxWindow *queryToolUpdateFactory::StartDialog(frmMain *form, pgObject *obj)
{
	if (obj->IsCreatedBy(tableFactory))
	{
		pgTable *table = (pgTable *)obj;
		return StartDialogSql(form, obj, table->GetUpdateSql(form->GetBrowser()));
	}
	else if (obj->IsCreatedBy(viewFactory))
	{
		pgView *view = (pgView *)obj;
		return StartDialogSql(form, obj, view->GetUpdateSql(form->GetBrowser()));
	}

	return 0;
}


bool queryToolUpdateFactory::CheckEnable(pgObject *obj)
{
	if (!queryToolDataFactory::CheckEnable(obj))
		return false;
	if (obj->IsCreatedBy(tableFactory))
		return true;
	pgView *view = (pgView *)obj;

	return view->HasUpdateRule();
}


queryToolInsertFactory::queryToolInsertFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar) : queryToolDataFactory(list)
{
	mnu->Append(id, _("INSERT Script"), _("Start query tool with INSERT script."));
}


wxWindow *queryToolInsertFactory::StartDialog(frmMain *form, pgObject *obj)
{
	if (obj->IsCreatedBy(tableFactory))
	{
		pgTable *table = (pgTable *)obj;
		return StartDialogSql(form, obj, table->GetInsertSql(form->GetBrowser()));
	}
	else if (obj->IsCreatedBy(viewFactory))
	{
		pgView *view = (pgView *)obj;
		return StartDialogSql(form, obj, view->GetInsertSql(form->GetBrowser()));
	}
	return 0;
}

bool queryToolInsertFactory::CheckEnable(pgObject *obj)
{
	if (!queryToolDataFactory::CheckEnable(obj))
		return false;
	if (obj->IsCreatedBy(tableFactory))
		return true;
	pgView *view = (pgView *)obj;

	return view->HasInsertRule();
}

void frmQuery::SaveExplainAsImage(wxCommandEvent &ev)
{
	wxFileDialog *dlg = new wxFileDialog(this, _("Save Explain As image file"), lastDir, lastFilename,
	                                     wxT("Bitmap files (*.bmp)|*.bmp|JPEG files (*.jpeg)|*.jpeg|PNG files (*.png)|*.png"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (dlg->ShowModal() == wxID_OK)
	{
		lastFilename = dlg->GetFilename();
		lastDir = dlg->GetDirectory();
		lastPath = dlg->GetPath();
		int index = dlg->GetFilterIndex();

		wxString     strType;
		wxBitmapType imgType;
		switch (index)
		{
				// bmp
			case 0:
				strType = wxT(".bmp");
				imgType = wxBITMAP_TYPE_BMP;
				break;
				// jpeg
			case 1:
				strType = wxT(".jpeg");
				imgType = wxBITMAP_TYPE_JPEG;
				break;
				// default (png)
			default:
				// png
			case 2:
				strType = wxT(".png");
				imgType = wxBITMAP_TYPE_PNG;
				break;
		}

		if (!lastPath.Contains(wxT(".")))
			lastPath += strType;

		if (ev.GetId() == MNU_SAVEAS_IMAGE_GQB)
			controller->getView()->SaveAsImage(lastPath, imgType);
		else if (ev.GetId() == MNU_SAVEAS_IMAGE_EXPLAIN)
			explainCanvas->SaveAsImage(lastPath, imgType);
	}
}

///////////////////////////////////////////////////////

pgScriptTimer::pgScriptTimer(frmQuery *parent) :
	m_parent(parent)
{

}

void pgScriptTimer::Notify()
{
	// Write script output
	m_parent->writeScriptOutput();
}

