/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <vcl/lstbox.hxx>
#include <svl/stritem.hxx>
#include <sfx2/request.hxx>
#include <sfx2/htmlmode.hxx>
#include <dbfld.hxx>
#include <flddat.hxx>
#include <fmtfld.hxx>
#include <viewopt.hxx>
#include <fldedt.hxx>
#include <docsh.hxx>
#include <swmodule.hxx>
#include <view.hxx>
#include <wrtsh.hxx>
#include <expfld.hxx>
#include <fldtdlg.hxx>
#include <fldpage.hxx>
#include <docufld.hxx>
#include <cmdid.h>
#include <globals.hrc>
#include <sfx2/bindings.hxx>
#include <calbck.hxx>

using namespace ::com::sun::star;

SwFieldPage::SwFieldPage(vcl::Window *pParent, const OString& rID,
    const OUString& rUIXMLDescription, const SfxItemSet &rAttrSet)
    : SfxTabPage(pParent, rID, rUIXMLDescription, &rAttrSet)
    , m_pCurField(0)
    , m_pWrtShell(0)
    , m_nTypeSel(LISTBOX_ENTRY_NOTFOUND)
    , m_nSelectionSel(LISTBOX_ENTRY_NOTFOUND)
    , m_bFieldEdit(false)
    , m_bInsert(true)
    , m_bFieldDlgHtmlMode(false)
    , m_bRefresh(false)
    , m_bFirstHTMLInit(true)
{
}

SwFieldPage::~SwFieldPage()
{
}

// initialise TabPage
void SwFieldPage::Init()
{
    SwDocShell* pDocSh = static_cast<SwDocShell*>(SfxObjectShell::Current());
    bool bNewMode = 0 != (::GetHtmlMode(pDocSh) & HTMLMODE_ON);

    m_bFieldEdit = 0 == GetTabDialog();

    // newly initialise FieldManager. important for
    // Dok-Switch (fldtdlg:ReInitTabPage)
    m_pCurField = m_aMgr.GetCurField();

    if( bNewMode != m_bFieldDlgHtmlMode )
    {
        m_bFieldDlgHtmlMode = bNewMode;

        // initialise Rangelistbox
        if( m_bFieldDlgHtmlMode && m_bFirstHTMLInit )
        {
            m_bFirstHTMLInit = false;
            SwWrtShell *pSh = m_pWrtShell;
            if(! pSh)
                pSh = ::GetActiveWrtShell();
            if(pSh)
            {
                SwDoc* pDoc = pSh->GetDoc();
                pSh->InsertFieldType( SwSetExpFieldType( pDoc,
                                    OUString("HTML_ON"), 1));
                pSh->InsertFieldType( SwSetExpFieldType(pDoc,
                                    OUString("HTML_OFF"), 1));
            }
        }
    }
}

// newly initialise page
void SwFieldPage::ActivatePage()
{
    EnableInsert(m_bInsert);
}

// complete reset; edit new field
void SwFieldPage::EditNewField( bool bOnlyActivate )
{
    if( !bOnlyActivate )
    {
        m_nTypeSel = LISTBOX_ENTRY_NOTFOUND;
    }
    m_nSelectionSel = LISTBOX_ENTRY_NOTFOUND;
    m_bRefresh = true;
    Reset(0);
    m_bRefresh = false;
}

// insert field
bool SwFieldPage::InsertField(sal_uInt16 nTypeId, sal_uInt16 nSubType, const OUString& rPar1,
                            const OUString& rPar2, sal_uLong nFormatId,
                            sal_Unicode cSeparator, bool bIsAutomaticLanguage)
{
    bool bRet = false;
    SwView* pView = GetActiveView();
    SwWrtShell *pSh = m_pWrtShell ? m_pWrtShell : pView->GetWrtShellPtr();

    if (!IsFieldEdit())   // insert new field
    {
        SwInsertField_Data aData(nTypeId, nSubType, rPar1, rPar2, nFormatId, 0, cSeparator, bIsAutomaticLanguage );
        //#i26566# provide parent for SwWrtShell::StartInputFieldDlg
        aData.m_pParent = &GetTabDialog()->GetOKButton();
        bRet = m_aMgr.InsertField( aData );

        uno::Reference< frame::XDispatchRecorder > xRecorder =
                pView->GetViewFrame()->GetBindings().GetRecorder();
        if ( xRecorder.is() )
        {
            bool bRecordDB = TYP_DBFLD == nTypeId ||
                            TYP_DBSETNUMBERFLD == nTypeId ||
                            TYP_DBNUMSETFLD == nTypeId ||
                            TYP_DBNEXTSETFLD == nTypeId ||
                            TYP_DBNAMEFLD == nTypeId ;

            SfxRequest aReq( pView->GetViewFrame(),
                    bRecordDB ?  FN_INSERT_DBFIELD : FN_INSERT_FIELD );
            if(bRecordDB)
            {
                aReq.AppendItem(SfxStringItem
                        (FN_INSERT_DBFIELD,rPar1.getToken(0, DB_DELIM)));
                aReq.AppendItem(SfxStringItem
                        (FN_PARAM_1,rPar1.getToken(1, DB_DELIM)));
                aReq.AppendItem(SfxInt32Item
                        (FN_PARAM_3,rPar1.getToken(1, DB_DELIM).toInt32()));
                aReq.AppendItem(SfxStringItem
                        (FN_PARAM_2,rPar1.getToken(3, DB_DELIM)));
            }
            else
            {
                aReq.AppendItem(SfxStringItem(FN_INSERT_FIELD, rPar1));
                aReq.AppendItem(SfxStringItem
                        (FN_PARAM_3, OUString(cSeparator)));
                aReq.AppendItem(SfxUInt16Item(FN_PARAM_FIELD_SUBTYPE, nSubType));
            }
            aReq.AppendItem(SfxUInt16Item(FN_PARAM_FIELD_TYPE   , nTypeId));
            aReq.AppendItem(SfxStringItem(FN_PARAM_FIELD_CONTENT, rPar2));
            aReq.AppendItem(SfxUInt32Item(FN_PARAM_FIELD_FORMAT , nFormatId));
            aReq.Done();
        }

    }
    else    // change field
    {
        SwField *const pTmpField = m_pCurField->CopyField();

        OUString sPar1(rPar1);
        OUString sPar2(rPar2);
        switch( nTypeId )
        {
        case TYP_DATEFLD:
        case TYP_TIMEFLD:
            nSubType = static_cast< sal_uInt16 >(((nTypeId == TYP_DATEFLD) ? DATEFLD : TIMEFLD) |
                       ((nSubType == DATE_VAR) ? 0 : FIXEDFLD));
            break;

        case TYP_DBNAMEFLD:
        case TYP_DBNEXTSETFLD:
        case TYP_DBNUMSETFLD:
        case TYP_DBSETNUMBERFLD:
            {
                sal_Int32 nPos = 0;
                SwDBData aData;

                aData.sDataSource = rPar1.getToken(0, DB_DELIM, nPos);
                aData.sCommand = rPar1.getToken(0, DB_DELIM, nPos);
                aData.nCommandType = rPar1.getToken(0, DB_DELIM, nPos).toInt32();
                sPar1 = rPar1.copy(nPos);

                static_cast<SwDBNameInfField*>(pTmpField)->SetDBData(aData);
            }
            break;

        case TYP_DBFLD:
            {
                SwDBData aData;
                aData.sDataSource = rPar1.getToken(0, DB_DELIM);
                aData.sCommand = rPar1.getToken(1, DB_DELIM);
                aData.nCommandType = rPar1.getToken(2, DB_DELIM).toInt32();
                OUString sColumn = rPar1.getToken(3, DB_DELIM);

                SwDBFieldType* pOldTyp = static_cast<SwDBFieldType*>(pTmpField->GetTyp());
                SwDBFieldType* pTyp = static_cast<SwDBFieldType*>(pSh->InsertFieldType(
                        SwDBFieldType(pSh->GetDoc(), sColumn, aData)));

                SwIterator<SwFormatField,SwFieldType> aIter( *pOldTyp );

                for( SwFormatField* pFormatField = aIter.First(); pFormatField; pFormatField = aIter.Next() )
                {
                    if( pFormatField->GetField() == m_pCurField)
                    {
                        pFormatField->RegisterToFieldType(*pTyp);
                        pTmpField->ChgTyp(pTyp);
                        break;
                    }
                }
            }
            break;

        case TYP_SEQFLD:
            {
                SwSetExpFieldType* pTyp = static_cast<SwSetExpFieldType*>(pTmpField->GetTyp());
                pTyp->SetOutlineLvl( static_cast< sal_uInt8 >(nSubType & 0xff));
                pTyp->SetDelimiter(OUString(cSeparator));

                nSubType = nsSwGetSetExpType::GSE_SEQ;
            }
            break;

        case TYP_INPUTFLD:
            {
                // User- or SetField ?
                if (m_aMgr.GetFieldType(RES_USERFLD, sPar1) == 0 &&
                !(pTmpField->GetSubType() & INP_TXT)) // SETEXPFLD
                {
                    SwSetExpField* pField = static_cast<SwSetExpField*>(pTmpField);
                    pField->SetPromptText(sPar2);
                    sPar2 = pField->GetPar2();
                }
            }
            break;
        case TYP_DOCINFOFLD:
            {
                if( nSubType == nsSwDocInfoSubType::DI_CUSTOM )
                {
                    SwDocInfoField* pDocInfo = static_cast<SwDocInfoField*>( pTmpField );
                    pDocInfo->SetName( rPar1 );
                }
            }
            break;
        }

        pSh->StartAllAction();

        pTmpField->SetSubType(nSubType);
        pTmpField->SetAutomaticLanguage(bIsAutomaticLanguage);

        m_aMgr.UpdateCurField( nFormatId, sPar1, sPar2, pTmpField );

        m_pCurField = m_aMgr.GetCurField();

        switch (nTypeId)
        {
            case TYP_HIDDENTXTFLD:
            case TYP_HIDDENPARAFLD:
                m_aMgr.EvalExpFields(pSh);
                break;
        }

        pSh->SetUndoNoResetModified();
        pSh->EndAllAction();
    }

    return bRet;
}

void SwFieldPage::SavePos( const ListBox* pLst1, const ListBox* pLst2,
                         const ListBox* pLst3 )
{
    const ListBox* aLBArr [ coLBCount ] = { pLst1, pLst2, pLst3 };

    const ListBox** ppLB = aLBArr;
    for( int i = 0; i < coLBCount; ++i, ++ppLB )
        if( (*ppLB) && (*ppLB)->GetEntryCount() )
            m_aLstStrArr[ i ] = (*ppLB)->GetSelectEntry();
        else
            m_aLstStrArr[ i ].clear();
}

void SwFieldPage::RestorePos(ListBox* pLst1, ListBox* pLst2, ListBox* pLst3)
{
    sal_Int32 nPos = 0;
    ListBox* aLBArr [ coLBCount ] = { pLst1, pLst2, pLst3 };
    ListBox** ppLB = aLBArr;
    for( int i = 0; i < coLBCount; ++i, ++ppLB )
        if( (*ppLB) && (*ppLB)->GetEntryCount() && !m_aLstStrArr[ i ].isEmpty() &&
            LISTBOX_ENTRY_NOTFOUND !=
                        ( nPos = (*ppLB)->GetEntryPos(m_aLstStrArr[ i ] ) ) )
            (*ppLB)->SelectEntryPos( nPos );
}

// Insert new fields
IMPL_LINK( SwFieldPage, InsertHdl, Button *, pBtn )
{
    SwFieldDlg *pDlg = static_cast<SwFieldDlg*>(GetTabDialog());
    if (pDlg)
    {
        pDlg->InsertHdl();

        if (pBtn)
            pBtn->GrabFocus();  // because of InputField-Dlg
    }
    else
    {
        SwFieldEditDlg *pEditDlg = static_cast<SwFieldEditDlg *>(GetParentDialog());
        pEditDlg->InsertHdl();
    }

    return 0;
}

// enable/disable "Insert"-Button
void SwFieldPage::EnableInsert(bool bEnable)
{
    SwFieldDlg *pDlg = static_cast<SwFieldDlg*>(GetTabDialog());
    if (pDlg)
    {
        if (pDlg->GetCurTabPage() == this)
            pDlg->EnableInsert(bEnable);
    }
    else
    {
        SwFieldEditDlg *pEditDlg = static_cast<SwFieldEditDlg *>(GetParentDialog());
        pEditDlg->EnableInsert(bEnable);
    }

    m_bInsert = bEnable;
}

IMPL_LINK_NOARG(SwFieldPage, NumFormatHdl)
{
    InsertHdl();

    return 0;
}

void SwFieldPage::SetWrtShell( SwWrtShell* pShell )
{
    m_pWrtShell = pShell;
    m_aMgr.SetWrtShell( pShell );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
