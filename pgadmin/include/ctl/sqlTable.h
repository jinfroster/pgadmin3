//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// sqlTable.h - Data source for SQL Grid and related classes
//
//////////////////////////////////////////////////////////////////////////

#ifndef SQLTABLE_H
#define  SQLTABLE_H

// wxWindows headers
#include <wx/wx.h>


// we cannot derive from wxGridCellAttr because destructor is private but not virtual
class sqlCellAttr
{
public:
	sqlCellAttr();
	~sqlCellAttr();

	int size();
	int precision();
	void SetReadOnly(bool iReadOnly = true);
	void SetEditor(wxGridCellEditor *editor);

	wxColour colourOdd;
	wxColour colourOddNull;
	wxColour colourEven;
	wxColour colourEvenNull;
	wxGridCellAttr *attrOdd;
	wxGridCellAttr *attrOddNull;
	wxGridCellAttr *attrEven;
	wxGridCellAttr *attrEvenNull;

	wxString Quote(pgConn *conn, const wxString &value);
	OID type;
	long typlen, typmod;
	wxString name, typeName, displayTypeName, description;
	bool readOnly, numeric, isPrimaryKey, needResize, hasDefault, notNull;
};



class cacheLine
{
public:
	cacheLine()
	{
		cols = 0;
		stored = false;
		readOnly = false;
	}
	~cacheLine()
	{
		if (cols) delete[] cols;
		if (nulls) delete[] nulls;
	}

	wxString *cols;
	bool *nulls;
	bool stored, readOnly;
};


class cacheLinePool
{
public:
	cacheLinePool(int initialLines);
	~cacheLinePool();
	cacheLine *operator[] (int line)
	{
		return Get(line);
	}
	cacheLine *Get(int lineNo);
	bool IsFilled(int lineNo);
	void Delete(int lineNo);

private:
	cacheLine **ptr;
	int anzLines;
};



class sqlTable : public wxGridTableBase
{
public:
	sqlTable(pgConn *conn, pgQueryThread *thread, const wxString &tabName, const OID relid, bool _hasOid, const wxString &_pkCols, char _relkind);
	~sqlTable();
	bool StoreLine();
	void UndoLine(int row);

	int GetNumberRows();
	int GetNumberStoredRows();
	int GetNumberCols();
	wxString GetColLabelValue(int col);
	wxString GetColLabelValueUnformatted(int col);
	wxString GetColDescription(int col);
	wxString GetRowLabelValue(int row);
	wxGridCellAttr *GetAttr(int row, int col, wxGridCellAttr::wxAttrKind kind);

	wxString GetValue(int row, int col);
	bool GetIsNull(int row, int col);
	void SetValue(int row, int col, const wxString &value);

	bool IsEmptyCell(int row, int col)
	{
		return false;
	}
	bool needsResizing(int col)
	{
		return columns[col].needResize;
	}
	bool AppendRows(size_t rows);
	bool DeleteRows(size_t pos, size_t rows);
	int  LastRow()
	{
		return lastRow;
	}
	bool IsColText(int col);
	bool IsColBoolean(int col);

	bool CheckInCache(int row);
	bool IsLineSaved(int row)
	{
		return GetLine(row)->stored;
	}

	bool Paste();

private:
	pgQueryThread *thread;
	pgConn *connection;
	bool hasOids;
	char relkind;
	wxString tableName;
	OID relid;
	wxString primaryKeyColNumbers;

	cacheLine *GetLine(int row);
	wxString MakeKey(cacheLine *line);
	void SetNumberEditor(int col, int len);

	cacheLinePool *dataPool, *addPool;
	cacheLine savedLine;
	int lastRow;

	int *lineIndex;     // reindex of lines in dataSet to handle deleted rows

	int nCols;          // columns from dataSet
	int nRows;          // rows initially returned by dataSet
	int rowsCached;     // rows read from dataset; if nRows=rowsCached, dataSet can be deleted
	int rowsAdded;      // rows added (never been in dataSet)
	int rowsStored;     // rows added and stored to db
	int rowsDeleted;    // rows deleted from initial dataSet
	sqlCellAttr *columns;

	wxArrayInt colMap;

	friend class ctlSQLEditGrid;
};

#endif
