//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
// RCS-ID:      $Id$
// Copyright (C) 2002 - 2006, The pgAdmin Development Team
// This software is released under the Artistic Licence
//
// dlgView.h - View property 
//
//////////////////////////////////////////////////////////////////////////


#ifndef __DLG_VIEWPROP
#define __DLG_VIEWPROP

#include "dlgProperty.h"

class pgSchema;
class pgView;
class ctlSQLBox;

class dlgView : public dlgSecurityProperty
{
public:
    dlgView(pgaFactory *factory, frmMain *frame, pgView *v, pgSchema *sch);
    int Go(bool modal);

    void CheckChange();
    wxString GetSql();
    pgObject *CreateObject(pgCollection *collection);
    pgObject *GetObject();

private:
    pgSchema *schema;
    pgView *view;
    wxString oldDefinition;

    DECLARE_EVENT_TABLE()
};


#endif