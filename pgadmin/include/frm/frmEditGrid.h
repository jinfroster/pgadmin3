//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// frmEditGrid.h - The SQL Edit Grid form
//
//////////////////////////////////////////////////////////////////////////

#ifndef __FRMEDITGRID_H
#define __FRMEDITGRID_H

#include <wx/grid.h>
#include <wx/stc/stc.h>
// wxAUI
#include <wx/aui/aui.h>

#define CTL_EDITGRID 357
#include "dlg/dlgClasses.h"
#include "ctl/ctlSQLGrid.h"

//
// This number MUST be incremented if changing any of the default perspectives
//
#define FRMEDITGRID_PERSPECTIVE_VER wxT("8189")

#ifdef __WXMAC__
#define FRMEDITGRID_DEFAULT_PERSPECTIVE wxT("layout2|name=toolBar;caption=Tool bar;state=16788208;dir=1;layer=10;row=0;pos=0;prop=100000;bestw=240;besth=23;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=limitBar;caption=Limit bar;state=16788208;dir=1;layer=10;row=0;pos=243;prop=100000;bestw=120;besth=21;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=415;floaty=793;floatw=-1;floath=-1|name=sqlGrid;caption=Data grid;state=1020;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=300;besth=200;minw=200;minh=100;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=scratchPad;caption=Scratch pad;state=16779260;dir=3;layer=0;row=0;pos=0;prop=100000;bestw=300;besth=150;minw=200;minh=100;maxw=-1;maxh=-1;floatx=347;floaty=725;floatw=-1;floath=-1|dock_size(1,10,0)=25|dock_size(5,0,0)=84|dock_size(3,0,0)=173|")
#else
#ifdef __WXGTK__
#define FRMEDITGRID_DEFAULT_PERSPECTIVE wxT("layout2|name=toolBar;caption=Tool bar;state=16788208;dir=1;layer=10;row=0;pos=0;prop=100000;bestw=305;besth=30;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=limitBar;caption=Limit bar;state=16788208;dir=1;layer=10;row=0;pos=243;prop=100000;bestw=120;besth=30;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=415;floaty=793;floatw=-1;floath=-1|name=sqlGrid;caption=Data grid;state=1020;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=300;besth=200;minw=200;minh=100;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=scratchPad;caption=Scratch pad;state=16779260;dir=3;layer=0;row=0;pos=0;prop=100000;bestw=300;besth=150;minw=200;minh=100;maxw=-1;maxh=-1;floatx=347;floaty=725;floatw=-1;floath=-1|dock_size(1,10,0)=25|dock_size(5,0,0)=84|dock_size(3,0,0)=173|")
#else
#define FRMEDITGRID_DEFAULT_PERSPECTIVE wxT("layout2|name=toolBar;caption=Tool bar;state=16788208;dir=1;layer=10;row=0;pos=0;prop=100000;bestw=232;besth=23;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=limitBar;caption=Limit bar;state=16788208;dir=1;layer=10;row=0;pos=243;prop=100000;bestw=100;besth=21;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=415;floaty=793;floatw=-1;floath=-1|name=sqlGrid;caption=Data grid;state=1020;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=300;besth=200;minw=200;minh=100;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=scratchPad;caption=Scratch pad;state=16779260;dir=3;layer=0;row=0;pos=0;prop=100000;bestw=300;besth=150;minw=200;minh=100;maxw=-1;maxh=-1;floatx=347;floaty=725;floatw=-1;floath=-1|dock_size(1,10,0)=25|dock_size(5,0,0)=84|dock_size(3,0,0)=173|")
#endif
#endif

// Position of status line fields
enum
{
    EGSTATUSPOS_ICON,
    EGSTATUSPOS_ROWS,
    EGSTATUSPOS_MSGS
};
#define EGSTATUSPOS_COUNT 3



class sqlCell
{
public:
	sqlCell()
	{
		ClearCell();
	}

	void SetCell(long r, long c)
	{
		row = r;
		col = c;
	}
	void ClearCell()
	{
		row = -1;
		col = -1;
	}
	bool IsSet()
	{
		return row != -1 && col != -1;
	}

	long GetRow()
	{
		return row;
	}
	long GetCol()
	{
		return col;
	}

private:
	long row;
	long col;
};



class ctlSQLEditGrid : public ctlSQLGrid
{
public:
	ctlSQLEditGrid(wxFrame *parent, wxWindowID id, const wxPoint &pos, const wxSize &size);

	sqlTable *GetTable()
	{
		return (sqlTable *)wxGrid::GetTable();
	}
	//wxSize GetBestSize(int row, int col);
	void ResizeEditor(int row, int col);
	wxArrayInt GetSelectedRows() const;
	bool CheckRowPresent(int row);
	virtual bool IsColText(int col);
};

class frmMain;
class pgSchemaObject;

class frmEditGrid : public pgFrame
{
public:
	frmEditGrid(frmMain *form, const wxString &_title, pgConn *conn, pgSchemaObject *obj, bool ascending = true);
	~frmEditGrid();

	void ShowForm(bool filter = false);
	void Go();
	wxString GetSortCols() const
	{
		return orderBy;
	} ;
	void SetSortCols(const wxString &cols);
	wxString GetFilter() const
	{
		return rowFilter;
	} ;
	void SetFilter(const wxString &filter);
	int GetLimit() const
	{
		return limit;
	} ;
	void SetLimit(const int rowlimit);
	wxMenu *GetFileMenu()
	{
		return fileMenu;
	};
	wxMenu *GetEditMenu()
	{
		return editMenu;
	};
	void SetStatusTextRows(const int numRows);

private:
	void OnEraseBackground(wxEraseEvent &event);
	void OnSize(wxSizeEvent &event);

	void OnCloseWindow(wxCloseEvent &event);
	void OnClose(wxCommandEvent &event);
	void OnHelp(wxCommandEvent &event);
	void OnContents(wxCommandEvent &event);
	void OnRefresh(wxCommandEvent &event);
	void OnDelete(wxCommandEvent &event);
	void OnOptions(wxCommandEvent &event);
	void OnSave(wxCommandEvent &event);
	bool DoSave();
	void CancelChange();
	void OnUndo(wxCommandEvent &event);
	void OnCellChange(wxGridEvent &event);
	void OnGridSelectCells(wxGridRangeSelectEvent &event);
	void OnEditorShown(wxGridEvent &event);
	void OnEditorHidden(wxGridEvent &event);
	void OnKey(wxKeyEvent &event);
	void OnCopy(wxCommandEvent &event);
	void OnIncludeFilter(wxCommandEvent &event);
	void OnExcludeFilter(wxCommandEvent &event);
	void OnRemoveFilters(wxCommandEvent &event);
	void OnAscSort(wxCommandEvent &event);
	void OnDescSort(wxCommandEvent &event);
	void OnRemoveSort(wxCommandEvent &event);
	void OnPaste(wxCommandEvent &event);
	void OnLabelDoubleClick(wxGridEvent &event);
	void OnLabelRightClick(wxGridEvent &event);
	void OnCellRightClick(wxGridEvent &event);
	void Abort();
	void OnToggleScratchPad(wxCommandEvent &event);
	void OnToggleLimitBar(wxCommandEvent &event);
	void OnToggleToolBar(wxCommandEvent &event);
	void OnAuiUpdate(wxAuiManagerEvent &event);
	void OnDefaultView(wxCommandEvent &event);

	wxAuiManager manager;
	ctlSQLEditGrid *sqlGrid;

	frmMain *mainForm;
	pgConn *connection;
	pgQueryThread *thread;
	wxMenu *fileMenu, *editMenu, *viewMenu, *toolsMenu, *helpMenu;
	ctlMenuToolbar *toolBar;
	wxComboBox *cbLimit;
	wxTextCtrl *scratchPad;

	char relkind;
	OID relid;
	bool hasOids;
	wxString tableName;
	wxString primaryKeyColNumbers;
	wxString orderBy;
	bool autoOrderBy;
	wxString rowFilter;
	int limit;
	sqlCell *editorCell;
	bool closing;
	wxStaticBitmap *mStatusBitmap;

	DECLARE_EVENT_TABLE()
};


class editGridFactoryBase : public contextActionFactory
{
public:
	bool CheckEnable(pgObject *obj);

protected:
	editGridFactoryBase(menuFactoryList *list) : contextActionFactory(list)
	{
		pkAscending = true;
		rowlimit = 0;
	}
	wxWindow *ViewData(frmMain *form, pgObject *obj, bool filter);
	int rowlimit;
	bool pkAscending;
};


class editGridFactory : public editGridFactoryBase
{
public:
	editGridFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar);
	wxWindow *StartDialog(frmMain *form, pgObject *obj);
};


class editGridFilteredFactory : public editGridFactoryBase
{
public:
	editGridFilteredFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar);
	wxWindow *StartDialog(frmMain *form, pgObject *obj);
};

class editGridLimitedFactory : public editGridFactoryBase
{
public:
	editGridLimitedFactory(menuFactoryList *list, wxMenu *mnu, ctlMenuToolbar *toolbar, int limit, bool ascending);
	wxWindow *StartDialog(frmMain *form, pgObject *obj);
};

#endif


