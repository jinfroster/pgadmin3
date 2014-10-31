//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2014, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// sqlTable.cpp - Data source for SQL Grid and related classes
//
//////////////////////////////////////////////////////////////////////////

#include "pgAdmin3.h"

// wxWindows headers
#include <wx/wx.h>
#include <wx/clipbrd.h>

#include "ctl/sqlTable.h"




sqlCellAttr::sqlCellAttr()
{
	isPrimaryKey = false;
	needResize = false;

	colourOdd = wxColour(255,255,255);
	colourOddNull = wxColour(255,255,229);
	colourEven = wxColour(229,255,229);
	colourEvenNull = wxColour(242,255,229);

	attrOdd = new wxGridCellAttr;
	attrOdd->SetBackgroundColour(colourOdd);
	attrOddNull = new wxGridCellAttr;
	attrOddNull->SetBackgroundColour(colourOddNull);
	attrEven = new wxGridCellAttr;
	attrEven->SetBackgroundColour(colourEven);
	attrEvenNull = new wxGridCellAttr;
	attrEvenNull->SetBackgroundColour(colourEvenNull);
}

wxString sqlCellAttr::Quote(pgConn *conn, const wxString &value)
{
	wxString str;
	if (value.IsEmpty())
		str = wxT("NULL");
	else if (numeric)
		str = conn->qtDbString(value);
	else if (value == wxT("\\'\\'"))
		str = conn->qtDbString(wxT("''"));
	else if (value == wxT("''"))
		str = wxT("''");
	else if (type == PGOID_TYPE_BOOL)
		str = value;
	else if (type == PGOID_TYPE_BIT)
		// Don't cast this one
		return wxT("B'") + value + wxT("'");
	else
		str = conn->qtDbString(value);

	return str + wxT("::") + displayTypeName;
}

int sqlCellAttr::size()
{
	if (typlen == -1 && typmod > 0)
	{
		return (typmod - 4) >> 16;
	}
	else
		return typlen;
}

int sqlCellAttr::precision()
{
	if (typlen == -1 && typmod > 0)
	{
		return (typmod - 4) & 0x7fff;
	}
	else
		return -1;
}

void sqlCellAttr::SetReadOnly(bool iReadOnly)
{
	readOnly = iReadOnly;
	attrOdd->SetReadOnly(readOnly);
	attrOddNull->SetReadOnly(readOnly);
	attrEven->SetReadOnly(readOnly);
	attrEvenNull->SetReadOnly(readOnly);
}

void sqlCellAttr::SetEditor(wxGridCellEditor *editor)
{
	attrOdd->SetEditor(editor);
	attrOddNull->SetEditor(editor);
	attrEven->SetEditor(editor);
	attrEvenNull->SetEditor(editor);
}

sqlCellAttr::~sqlCellAttr()
{
	attrOdd->DecRef();
	attrOddNull->DecRef();
	attrEven->DecRef();
	attrEvenNull->DecRef();
}


cacheLinePool::cacheLinePool(int initialLines)
{
	ptr = new cacheLine*[initialLines];
	if (ptr)
	{
		anzLines = initialLines;
		memset(ptr, 0, sizeof(cacheLine *)*anzLines);
	}
	else
	{
		anzLines = 0;
		wxLogError(__("Out of Memory for cacheLinePool"));
	}
}

cacheLinePool::~cacheLinePool()
{
	if (ptr)
	{
		while (anzLines--)
		{
			if (ptr[anzLines])
				delete ptr[anzLines];
		}
		delete[] ptr;
	}
}

void cacheLinePool::Delete(int lineNo)
{
	if (ptr && lineNo >= 0 && lineNo < anzLines)
	{
#if 1
		if (ptr[lineNo])
			delete ptr[lineNo];

		if (lineNo < anzLines - 1)
		{
			// beware: overlapping copy
			memmove(ptr + lineNo, ptr + lineNo + 1, sizeof(cacheLine *) * (anzLines - lineNo - 1));
		}
#else
		cacheLine *c;
		c = ptr[0];
		ptr[0] = ptr[1];
		ptr[1] = c;
#endif
		ptr[anzLines - 1] = 0;
	}
}

cacheLine *cacheLinePool::Get(int lineNo)
{
	if (lineNo < 0) return 0;

	if (lineNo >= anzLines)
	{
		cacheLine **old = ptr;
		int oldAnz = anzLines;
		anzLines = lineNo + 100;
		ptr = new cacheLine*[anzLines];
		if (!ptr)
		{
			anzLines = 0;
			wxLogError(__("Out of Memory for cacheLinePool"));
		}
		else
		{
			if (oldAnz)
			{
				memcpy(ptr, old, sizeof(cacheLine *)*oldAnz);
				delete[] old;
			}
			memset(ptr + oldAnz, 0, anzLines - oldAnz);
		}
	}

	if (lineNo < anzLines)
	{
		if (!ptr[lineNo])
			ptr[lineNo] = new cacheLine();
		return ptr[lineNo];
	}
	return 0;
}

bool cacheLinePool::IsFilled(int lineNo)
{
	return (lineNo < anzLines && ptr[lineNo]);
}



sqlTable::sqlTable(pgConn *conn, pgQueryThread *_thread, const wxString &tabName, const OID _relid, bool _hasOid, const wxString &_pkCols, char _relkind)
{
	connection = conn;
	primaryKeyColNumbers = _pkCols;
	relid = _relid;
	relkind = _relkind;
	tableName = tabName;
	hasOids = _hasOid;
	thread = _thread;

	rowsCached = 0;
	rowsAdded = 0;
	rowsStored = 0;
	rowsDeleted = 0;


	dataPool = 0;
	addPool = new cacheLinePool(500);       // arbitrary initial size
	lastRow = -1;
	int i;
	lineIndex = 0;

	nRows = thread->DataSet()->NumRows();
	nCols = thread->DataSet()->NumCols();

	columns = new sqlCellAttr[nCols];
	savedLine.cols = new wxString[nCols];

	// Get the "real" column list, including any dropped columns, as
	// key positions etc do not ignore these.
	pgSet *allColsSet = connection->ExecuteSet(
							wxT("SELECT attisdropped FROM pg_attribute")
							wxT(" WHERE attnum > 0 AND attrelid=") + NumToStr(relid) + wxT("::oid\n")
							wxT(" ORDER BY attnum"));

	int x = 1;
	if (allColsSet)
	{
		for (i = 0; i < allColsSet->NumRows(); i++)
		{
			if (allColsSet->GetVal(wxT("attisdropped")) == wxT("t"))
			{
				colMap.Add(0);
			}
			else
			{
				colMap.Add(x);
				x++;
			}
			allColsSet->MoveNext();
		}
	}

	pgSet *colSet = connection->ExecuteSet(
	                    wxT("SELECT n.nspname AS nspname, relname, format_type(t.oid,NULL) AS typname, format_type(t.oid, att.atttypmod) AS displaytypname, ")
	                    wxT("nt.nspname AS typnspname, attname, attnum, COALESCE(b.oid, t.oid) AS basetype, attnotnull, atthasdef, adsrc,\n")
	                    wxT("       CASE WHEN t.typbasetype::oid=0 THEN att.atttypmod else t.typtypmod END AS typmod,\n")
	                    wxT("       CASE WHEN t.typbasetype::oid=0 THEN att.attlen else t.typlen END AS typlen,\n")
	                    wxT("       pg_catalog.col_description(att.attrelid, att.attnum) AS description\n")
	                    wxT("  FROM pg_attribute att\n")
	                    wxT("  JOIN pg_type t ON t.oid=att.atttypid\n")
	                    wxT("  JOIN pg_namespace nt ON nt.oid=t.typnamespace\n")
	                    wxT("  JOIN pg_class c ON c.oid=attrelid\n")
	                    wxT("  JOIN pg_namespace n ON n.oid=relnamespace\n")
	                    wxT("  LEFT OUTER JOIN pg_type b ON b.oid=t.typbasetype\n")
	                    wxT("  LEFT OUTER JOIN pg_attrdef def ON adrelid=attrelid AND adnum=attnum\n")
	                    wxT(" WHERE attnum > 0 AND NOT attisdropped AND attrelid=") + NumToStr(relid) + wxT("::oid\n")
	                    wxT(" ORDER BY attnum"));



	bool canInsert = false;
	if (colSet)
	{
		if (hasOids)
		{
			columns[0].name = wxT("oid");
			columns[0].numeric = true;
			columns[0].SetReadOnly(true);
			columns[0].type = PGTYPCLASS_NUMERIC;
			columns[0].description = wxEmptyString;
			columns[0].hasDefault = true;
			columns[0].notNull = true;
		}

		for (i = (hasOids ? 1 : 0) ; i < nCols ; i++)
		{
			wxGridCellEditor *editor = 0;

			columns[i].name = colSet->GetVal(wxT("attname"));
			columns[i].typeName = colSet->GetVal(wxT("typname"));
			columns[i].displayTypeName = colSet->GetVal(wxT("displaytypname"));
			columns[i].description = colSet->GetVal(wxT("description"));
			columns[i].hasDefault = colSet->GetBool(wxT("atthasdef"));
			columns[i].notNull = colSet->GetBool(wxT("attnotnull"));

			// Special case for character datatypes. We always cast them to text to avoid
			// truncation issues with casts like ::character(3)
			if (columns[i].typeName == wxT("character") || columns[i].typeName == wxT("character varying") || columns[i].typeName == wxT("\"char\""))
				columns[i].typeName = wxT("text");

			columns[i].type = (Oid)colSet->GetOid(wxT("basetype"));
			if ((columns[i].type == PGOID_TYPE_INT4 || columns[i].type == PGOID_TYPE_INT8 || columns[i].type == PGOID_TYPE_INT2)
			        && columns[i].hasDefault)
			{
				wxString adsrc = colSet->GetVal(wxT("adsrc"));
				if (adsrc ==  wxT("nextval('") + colSet->GetVal(wxT("relname")) + wxT("_") + columns[i].name + wxT("_seq'::text)") ||
						adsrc ==  wxT("nextval('") + colSet->GetVal(wxT("nspname")) + wxT(".") + colSet->GetVal(wxT("relname")) + wxT("_") + columns[i].name + wxT("_seq'::text)") ||
						adsrc ==  wxT("nextval('") + colSet->GetVal(wxT("relname")) + wxT("_") + columns[i].name + wxT("_seq'::regclass)") ||
						adsrc ==  wxT("nextval('") + colSet->GetVal(wxT("nspname")) + wxT(".") + colSet->GetVal(wxT("relname")) + wxT("_") + columns[i].name + wxT("_seq'::regclass)"))
				{
					if (columns[i].type == PGOID_TYPE_INT4)
						columns[i].type = (Oid)PGOID_TYPE_SERIAL;
					else if (columns[i].type == PGOID_TYPE_INT8)
						columns[i].type = (Oid)PGOID_TYPE_SERIAL8;
					else
						columns[i].type = (Oid)PGOID_TYPE_SERIAL2;
				}
			}
			columns[i].typlen = colSet->GetLong(wxT("typlen"));
			columns[i].typmod = colSet->GetLong(wxT("typmod"));

			switch (columns[i].type)
			{
				case PGOID_TYPE_BOOL:
					columns[i].numeric = false;
					columns[i].SetReadOnly(false);
					editor = new sqlGridBoolEditor();
					break;
				case PGOID_TYPE_INT8:
				case PGOID_TYPE_SERIAL8:
					SetNumberEditor(i, 20);
					break;
				case PGOID_TYPE_INT2:
				case PGOID_TYPE_SERIAL2:
					SetNumberEditor(i, 5);
					break;
				case PGOID_TYPE_INT4:
				case PGOID_TYPE_SERIAL:
					SetNumberEditor(i, 10);
					break;
				case PGOID_TYPE_OID:
				case PGOID_TYPE_TID:
				case PGOID_TYPE_XID:
				case PGOID_TYPE_CID:
					SetNumberEditor(i, 10);
					break;
				case PGOID_TYPE_FLOAT4:
				case PGOID_TYPE_FLOAT8:
					columns[i].numeric = true;
					columns[i].SetReadOnly(false);
					editor = new sqlGridNumericEditor();
					break;
				case PGOID_TYPE_MONEY:
					columns[i].numeric = false;
					columns[i].SetReadOnly(false);
					editor = new wxGridCellTextEditor();
					break;
				case PGOID_TYPE_NUMERIC:
				{
					columns[i].numeric = true;
					columns[i].SetReadOnly(false);
					int len = columns[i].size();
					int prec = columns[i].precision();
					if (prec > 0)
						len -= (prec);
					editor = new sqlGridNumericEditor(len, prec);
					break;
				}
				case PGOID_TYPE_BYTEA:
					columns[i].numeric = false;
					columns[i].SetReadOnly(true);
					break;
				case PGOID_TYPE_DATE:
				case PGOID_TYPE_TIME:
				case PGOID_TYPE_TIMETZ:
				case PGOID_TYPE_TIMESTAMP:
				case PGOID_TYPE_TIMESTAMPTZ:
				case PGOID_TYPE_INTERVAL:
					columns[i].numeric = false;
					columns[i].SetReadOnly(false);
					editor = new sqlGridTextEditor();
					break;
				case PGOID_TYPE_CHAR:
				case PGOID_TYPE_NAME:
				case PGOID_TYPE_TEXT:
				default:
					columns[i].numeric = false;
					columns[i].SetReadOnly(false);
					columns[i].needResize = true;
					editor = new sqlGridTextEditor();
					break;
			}
			if (editor)
				columns[i].SetEditor(editor);

			if (relkind != 'r' || (!hasOids && primaryKeyColNumbers.IsNull()))
			{
				// for security reasons, we need oid or pk to enable updates. If none is available,
				// the table definition can be considered faulty.
				columns[i].SetReadOnly(true);
			}
			if (!columns[i].readOnly)
				canInsert = true;

			wxStringTokenizer collist(primaryKeyColNumbers, wxT(","));
			long cn = 0;
			long attnum = colSet->GetLong(wxT("attnum"));

			while (cn < attnum && collist.HasMoreTokens())
			{
				cn = StrToLong(collist.GetNextToken());
				if (cn == attnum)
					columns[i].isPrimaryKey = true;
			}

			colSet->MoveNext();
		}
		delete colSet;
	}
	else
	{
		// um, we never should reach here because this would mean
		// that datatypes are unreadable.
		// *if* we reach here, namespace info is missing.
		for (i = 0 ; i < nCols ; i++)
		{
			columns[i].typeName = thread->DataSet()->ColType(i);
			columns[i].name = thread->DataSet()->ColName(i);
		}
	}

	if (nRows)
	{
		dataPool = new cacheLinePool(nRows);
		lineIndex = new int[nRows];
		for (i = 0 ; i < nRows ; i++)
			lineIndex[i] = i;
	}

	if (canInsert)
	{
		// an empty line waiting for inserts
		rowsAdded = 1;
	}
}

sqlTable::~sqlTable()
{
	if (thread)
		delete thread;
	if (dataPool)
		delete dataPool;

	delete addPool;

	delete[] columns;

	if (lineIndex)
		delete[] lineIndex;
}

int sqlTable::GetNumberCols()
{
	return nCols;
}

int sqlTable::GetNumberRows()
{
	return nRows + rowsAdded - rowsDeleted;
}

int sqlTable::GetNumberStoredRows()
{
	return nRows + rowsStored - rowsDeleted;
}

bool sqlTable::IsColText(int col)
{
	return !columns[col].numeric && !(columns[col].type == PGOID_TYPE_BOOL);
}

bool sqlTable::IsColBoolean(int col)
{
	return (columns[col].type == PGOID_TYPE_BOOL);
}

wxString sqlTable::GetColLabelValue(int col)
{
	wxString label = columns[col].name + wxT("\n");
	if (columns[col].isPrimaryKey)
		label += wxT("[PK] ");

	switch (columns[col].type)
	{
		case (Oid)PGOID_TYPE_SERIAL:
			label += wxT("serial");
			break;
		case (Oid)PGOID_TYPE_SERIAL8:
			label += wxT("bigserial");
			break;
		case (Oid)PGOID_TYPE_SERIAL2:
			label += wxT("smallserial");
			break;
		default:
			label += columns[col].displayTypeName;
			break;
	}
	return label;
}

wxString sqlTable::GetColLabelValueUnformatted(int col)
{
	return columns[col].name;
}

wxString sqlTable::GetColDescription(int col)
{
	if (!(0 <= col && col < nCols))
		return wxEmptyString;
	wxString descr = settings->GetColumnDescrFormat();
	descr.Replace(wxT("%i"), wxString::Format(wxT("%d"), col));

	// column attributes
	if(descr.Find(wxT("%a")) != wxNOT_FOUND) {
		wxString attr = wxT("");
		bool par = false;
		if (columns[col].isPrimaryKey)
		{
			attr += wxT("(");
			attr += _("PK");
			par = true;
		}
		else if (columns[col].notNull)
		{
			if (!par)
			{
				attr += wxT("(");
				par = true;
			}
			else
				attr += wxT(", ");
			attr += _("NOT NULL");
		}
		if (columns[col].hasDefault)
		{
			if (!par)
			{
				attr += wxT("(");
				par = true;
			}
			else
				attr += wxT(", ");
			attr += _("DEFAULT");
		}
		if (par)
			attr += wxT(")");

		descr.Replace(wxT("%a"), attr);
	}

	wxString type;
	switch (columns[col].type)
	{
		case (Oid)PGOID_TYPE_SERIAL:
			type = wxT("serial");
			break;
		case (Oid)PGOID_TYPE_SERIAL8:
			type = wxT("bigserial");
			break;
		case (Oid)PGOID_TYPE_SERIAL2:
			type = wxT("smallserial");
			break;
		default:
			type = columns[col].displayTypeName;
			break;
	}
	descr.Replace(_("%t"), type);

	descr.Replace(_("%n"), columns[col].name);
	descr.Replace(_("%d"), columns[col].description);
	return descr;
}

wxString sqlTable::GetRowLabelValue(int row)
{
	wxString label;
	if (row < nRows - rowsDeleted || GetLine(row)->stored)
		label.Printf(wxT("%d"), row + 1);
	else
		label = wxT("*");
	return label;
}

void sqlTable::SetNumberEditor(int col, int len)
{
	columns[col].numeric = true;
	columns[col].SetReadOnly(false);
	columns[col].SetEditor(new sqlGridNumericEditor(len, 0));
}

bool sqlTable::CheckInCache(int row)
{
	if (row > nRows - rowsDeleted + rowsAdded)
		return false;
	if (row >= nRows - rowsDeleted)
		return true;

	return dataPool->IsFilled(row);
}

cacheLine *sqlTable::GetLine(int row)
{
	cacheLine *line;
	if (row < nRows - rowsDeleted)
		line = dataPool->Get(lineIndex[row]);
	else
		line = addPool->Get(row - (nRows - rowsDeleted));

	return line;
}

wxString sqlTable::MakeKey(cacheLine *line)
{
	wxString whereClause;
	if (!primaryKeyColNumbers.IsEmpty())
	{
		wxStringTokenizer collist(primaryKeyColNumbers, wxT(","));
		long cn;
		int offset;

		if (hasOids)
			offset = 0;
		else
			offset = 1;

		while (collist.HasMoreTokens())
		{
			cn = StrToLong(collist.GetNextToken());

			// Translate the column location to the real location in the actual columns still present
			cn = colMap[cn - 1];

			wxString colval = line->cols[cn - offset];
			if (colval.IsEmpty())
				return wxEmptyString;

			if (colval == wxT("''") && columns[cn - offset].typeName == wxT("text"))
				colval = wxEmptyString;

			if (!whereClause.IsEmpty())
				whereClause += wxT(" AND ");
			whereClause += qtIdent(columns[cn - offset].name) + wxT(" = ") + connection->qtDbString(colval);

			if (columns[cn - offset].typeName != wxT(""))
			{
				whereClause += wxT("::");
				whereClause += columns[cn - offset].displayTypeName;
			}
		}
	}
	else if (hasOids)
		whereClause = wxT("oid = ") + line->cols[0];

	return whereClause;
}

void sqlTable::UndoLine(int row)
{
	if (lastRow >= 0 && row >= 0)
	{
		cacheLine *line = GetLine(row);
		if (line)
		{
			int i;
			for (i = 0 ; i < nCols ; i++)
				line->cols[i] = savedLine.cols[i];
			ctlMenuToolbar *tb = (ctlMenuToolbar *)((wxFrame *)GetView()->GetParent())->GetToolBar();
			if (tb)
			{
				tb->EnableTool(MNU_SAVE, false);
				tb->EnableTool(MNU_UNDO, false);
			}
			wxMenu *fm = ((frmEditGrid *)GetView()->GetParent())->GetFileMenu();
			if (fm)
				fm->Enable(MNU_SAVE, false);
			wxMenu *em = ((frmEditGrid *)GetView()->GetParent())->GetEditMenu();
			if (em)
				em->Enable(MNU_UNDO, false);
		}
	}
	lastRow = -1;
}

bool sqlTable::StoreLine()
{
	bool done = false;

	GetView()->BeginBatch();
	if (lastRow >= 0)
	{
		cacheLine *line = GetLine(lastRow);

		int i;
		wxString colList, valList;

		if (line->stored)
		{
			// UPDATE

			for (i = (hasOids ? 1 : 0) ; i < nCols ; i++)
			{
				if (savedLine.cols[i] != line->cols[i])
				{
					if (!valList.IsNull())
						valList += wxT(", ");
					valList += qtIdent(columns[i].name) + wxT("=") + columns[i].Quote(connection, line->cols[i]);
				}
			}

			if (valList.IsEmpty())
				done = true;
			else
			{
				wxString key = MakeKey(&savedLine);
				wxASSERT(!key.IsEmpty());
				done = connection->ExecuteVoid(wxT(
												   "UPDATE ") + tableName + wxT(
												   " SET ") + valList + wxT(
												   " WHERE ") + key);
			}
		}
		else
		{
			// INSERT

			for (i = 0 ; i < nCols ; i++)
			{
				if (!columns[i].readOnly && !line->cols[i].IsEmpty())
				{
					if (!colList.IsNull())
					{
						valList += wxT(", ");
						colList += wxT(", ");
					}
					colList += qtIdent(columns[i].name);

					valList += columns[i].Quote(connection, line->cols[i]);
				}
			}

			if (!valList.IsEmpty())
			{
				pgSet *set = connection->ExecuteSet(
								 wxT("INSERT INTO ") + tableName
								 + wxT("(") + colList
								 + wxT(") VALUES (") + valList
								 + wxT(")"));
				if (set)
				{
					if (set->GetInsertedCount() > 0)
					{
						if (hasOids)
							line->cols[0] = NumToStr((long)set->GetInsertedOid());
						delete set;

						done = true;
						rowsStored++;
						((frmEditGrid *)GetView()->GetParent())->SetStatusTextRows(GetNumberStoredRows());

						if (rowsAdded == rowsStored)
							GetView()->AppendRows();

						// Read back what we inserted to get default vals
						wxString key = MakeKey(line);

						if (key.IsEmpty())
						{
							// That's a problem: obviously, the key generated isn't present
							// because it's serial or default or otherwise generated in the backend
							// we don't get.
							// That's why the whole line is declared readonly.

							line->readOnly = true;
						}
						else
						{
							set = connection->ExecuteSet(
									  wxT("SELECT * FROM ") + tableName +
									  wxT(" WHERE ") + key);
							if (set)
							{
								for (i = (hasOids ? 1 : 0) ; i < nCols ; i++)
								{
									line->cols[i] = set->GetVal(columns[i].name);
								}
								delete set;
							}
						}
					}
				}
			}
		}
		if (done)
		{
			line->stored = true;
			lastRow = -1;
		}
		else
			GetView()->SelectRow(lastRow);
	}

	GetView()->EndBatch();

	return done;
}

void sqlTable::SetValue(int row, int col, const wxString &value)
{
	cacheLine *line = GetLine(row);

	if (!line)
	{
		// Bad problem, no line!
		return;
	}

	if (row != lastRow)
	{
		if (lastRow >= 0)
			StoreLine();

		if (!line->cols) {
			line->cols = new wxString[nCols];
			line->nulls = new bool[nCols];
		}
		// remember line contents for later reference in update ... where
		int i;
		for (i = 0 ; i < nCols ; i++)
			savedLine.cols[i] = line->cols[i];
		lastRow = row;
	}
	ctlMenuToolbar *tb = (ctlMenuToolbar *)((wxFrame *)GetView()->GetParent())->GetToolBar();
	if (tb)
	{
		tb->EnableTool(MNU_SAVE, true);
		tb->EnableTool(MNU_UNDO, true);
	}
	wxMenu *fm = ((frmEditGrid *)GetView()->GetParent())->GetFileMenu();
	if (fm)
		fm->Enable(MNU_SAVE, true);
	wxMenu *em = ((frmEditGrid *)GetView()->GetParent())->GetEditMenu();
	if (em)
		em->Enable(MNU_UNDO, true);
	line->cols[col] = value;
}


bool sqlTable::GetIsNull(int row, int col)
{
	//wxLogInfo(wxT("sqlTable::GetIsNull(%d, %d)"), row, col);
	bool isNull = false;
	cacheLine *line;
	if (row < nRows - rowsDeleted)
		line = dataPool->Get(lineIndex[row]);
	else
		line = addPool->Get(row - (nRows - rowsDeleted));

	if (line && line->nulls) {
		isNull = line->nulls[col];
	}
	return isNull;
}

wxString sqlTable::GetValue(int row, int col)
{
	//wxLogInfo(wxT("sqlTable::GetValue(%d, %d)"), row, col);
	wxString val;
	cacheLine *line;
	if (row < nRows - rowsDeleted)
		line = dataPool->Get(lineIndex[row]);
	else
		line = addPool->Get(row - (nRows - rowsDeleted));

	if (!line)
	{
		// Bad problem, no line!
		return val;
	}

	if (!line->cols)
	{
		line->cols = new wxString[nCols];
		line->nulls = new bool[nCols];
		if (row < nRows - rowsDeleted)
		{
			if (!thread)
			{
				wxLogError(__("Unexpected empty cache line: dataSet already closed."));
				return val;
			}

			line->stored = true;
			if (lineIndex[row] != thread->DataSet()->CurrentPos() - 1)
				thread->DataSet()->Locate(lineIndex[row] + 1);

			int i;
			for (i = 0 ; i < nCols ; i++)
			{
				wxString val;
				bool isNull = false;
				if (thread->DataSet()->ColType(i) == wxT("bytea"))
					val = _("<binary data>");
				else
				{
					val = thread->DataSet()->GetVal(i);
					if (val.IsEmpty())
					{
						if (!thread->DataSet()->IsNull(i))
							val = wxT("''");
						else
						{
							isNull = true;
							if (settings->GetIndicateNull()) {
								val = wxT("<NULL>");
							}
						}
					}
					else if (val == wxT("''"))
						val = wxT("\\'\\'");
				}
				line->cols[i] = val;
				line->nulls[i] = isNull;
			}
			rowsCached++;

			if (rowsCached == nRows)
			{
				delete thread;
				thread = 0;
			}
		}
	}
	if (columns[col].type == PGOID_TYPE_BOOL)
	{
		if (line->cols[col] != wxEmptyString)
			line->cols[col] = (StrToBool(line->cols[col]) ? wxT("TRUE") : wxT("FALSE"));
	}

	val = line->cols[col];
	return val;
}

bool sqlTable::AppendRows(size_t rows)
{
	rowsAdded += rows;
	GetLine(nRows + rowsAdded - rowsDeleted - 1);

	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, rows);
	GetView()->ProcessTableMessage(msg);

	return true;
}

bool sqlTable::DeleteRows(size_t pos, size_t rows)
{
	size_t i = pos;
	size_t rowsDone = 0;

	while (i < pos + rows)
	{
		cacheLine *line = GetLine(pos);
		if (!line)
			break;

		// If line->cols is null, it probably means we need to force the cacheline to be populated.
		if (!line->cols)
		{
			GetValue(pos, 0);
			line = GetLine(pos);
		}

		if (line->stored)
		{
			wxString key = MakeKey(line);
			wxASSERT(!key.IsEmpty());
			bool done = connection->ExecuteVoid(wxT(
													"DELETE FROM ") + tableName + wxT(" WHERE ") + key);
			if (!done)
				break;

			if ((int)pos < nRows - rowsDeleted)
			{
				rowsDeleted++;
				if ((int)pos < nRows - rowsDeleted)
					memmove(lineIndex + pos, lineIndex + pos + 1, sizeof(*lineIndex) * (nRows - rowsDeleted - pos));
			}
			else
			{
				rowsAdded--;
				if (GetLine(pos)->stored)
					rowsStored--;
				addPool->Delete(pos - (nRows - rowsDeleted));
			}
			rowsDone++;
		}
		else
		{
			// last empty line won't be deleted, just cleared
			int j;
			for (j = 0 ; j < nCols ; j++)
				line->cols[j] = wxT("");
		}
		i++;
	}

	if (rowsDone > 0 && GetView())
	{
		wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, pos, rowsDone);
		GetView()->ProcessTableMessage(msg);
	}
	return (rowsDone != 0);
}

bool sqlTable::Paste()
{
	int row, col;
	int start, pos, len;
	wxArrayString data;
	wxString text, quoteChar, colSep;
	bool inQuotes, inData, skipSerial;

	if (!this)
		return false;

	if (wxTheClipboard->Open())
	{
		if (wxTheClipboard->IsSupported(wxDF_TEXT))
		{
			wxTextDataObject textData;
			wxTheClipboard->GetData(textData);
			text = textData.GetText();
		}
		else
		{
			wxTheClipboard->Close();
			return false;
		}
		wxTheClipboard->Close();
	}
	else
	{
		return false;
	}

	start = pos = 0;
	len = text.Len();
	quoteChar = settings->GetCopyQuoteChar();
	colSep = settings->GetCopyColSeparator();
	inQuotes = inData = false;

	while (pos < len && !(text[pos] == '\n' && !inQuotes))
	{
		if (!inData)
		{
			if (text[pos] == quoteChar)
			{
				inQuotes = inData = true;
				pos++;
				start++;
				continue;
			}
			else
			{
				inQuotes = false;
			}
			inData = true;
		}

		if (inQuotes && text[pos] == quoteChar &&
				(text[pos + 1] == colSep || text[pos + 1] == '\r' || text[pos + 1] == '\n'))
		{
			data.Add(text.Mid(start, pos - start));
			start = (pos += 2);
			inData = false;
		}
		else if (!inQuotes && text[pos] == colSep)
		{
			data.Add(text.Mid(start, pos - start));
			start = ++pos;
			inData = false;
		}
		else
		{
			pos++;
		}
	}
	if (start < pos)
	{
		if (inQuotes && text[pos - 1] == quoteChar)
			data.Add(text.Mid(start, pos - start - 1));
		else
			data.Add(text.Mid(start, pos - start));
	}

	row = GetNumberRows() - 1;
	skipSerial = false;

	for (col = 0; col < nCols; col++)
	{
		if (columns[col].type == (unsigned int)PGOID_TYPE_SERIAL ||
			columns[col].type == (unsigned int)PGOID_TYPE_SERIAL8 ||
			columns[col].type == (unsigned int)PGOID_TYPE_SERIAL2)
		{
			wxMessageDialog msg(GetView()->GetParent(),
								_("This table contains serial columns. Do you want to use the values in the clipboard for these columns?"),
								_("Paste Data"), wxYES_NO | wxICON_QUESTION);
			if (msg.ShowModal() != wxID_YES)
			{
				skipSerial = true;
			}
			break;
		}
	}

	bool pasted = false;
	for (col = (hasOids ? 1 : 0); col < nCols && col < (int)data.GetCount(); col++)
	{
		if (!(skipSerial && (columns[col].type == (unsigned int)PGOID_TYPE_SERIAL ||
							 columns[col].type == (unsigned int)PGOID_TYPE_SERIAL8 ||
							 columns[col].type == (unsigned int)PGOID_TYPE_SERIAL2)))
		{
			SetValue(row, col, data.Item(col));
			GetView()->SetGridCursor(row, col);
			GetView()->MakeCellVisible(row, col);
			pasted = true;
		}
	}
	GetView()->ForceRefresh();

	return pasted;
}

wxGridCellAttr *sqlTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind  kind)
{
	//wxLogInfo(wxT("sqlTable::GetAttr(%d, %d)"), row, col);
	wxGridCellAttr *attrDefault;
	if (GetIsNull(row, col))
		attrDefault =  ( row % 2 ) ? columns[col].attrEvenNull : columns[col].attrOddNull;
	else
		attrDefault =  ( row % 2 ) ? columns[col].attrEven : columns[col].attrOdd;

	cacheLine *line = GetLine(row);
	if (line && line->readOnly)
	{
		wxGridCellAttr *attr = new wxGridCellAttr(attrDefault);
		attr->SetReadOnly();
		return attr;
	}
	else
	{
		attrDefault->IncRef();
		return attrDefault;
	}
}

