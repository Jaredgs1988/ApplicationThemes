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
#ifndef INCLUDED_SW_SOURCE_UI_DBUI_MMADDRESSBLOCKPAGE_HXX
#define INCLUDED_SW_SOURCE_UI_DBUI_MMADDRESSBLOCKPAGE_HXX

#include <svtools/wizardmachine.hxx>
#include <vcl/button.hxx>
#include <svtools/stdctrl.hxx>
#include <mailmergehelper.hxx>
#include <sfx2/basedlgs.hxx>
#include <vcl/edit.hxx>
#include <vcl/layout.hxx>
#include <vcl/lstbox.hxx>
#include <svtools/svmedit.hxx>
#include <svtools/headbar.hxx>
#include <svtools/treelistbox.hxx>
#include <vcl/combobox.hxx>
#include <svl/lstner.hxx>
class SwMailMergeWizard;
class SwMailMergeConfigItem;

class SwMailMergeAddressBlockPage : public svt::OWizardPage
{
    VclPtr<PushButton>         m_pAddressListPB;
    VclPtr<FixedText>          m_pCurrentAddressFI;

    VclPtr<VclContainer>       m_pStep2;
    VclPtr<VclContainer>       m_pStep3;
    VclPtr<VclContainer>       m_pStep4;

    VclPtr<FixedText>          m_pSettingsFI;
    VclPtr<CheckBox>           m_pAddressCB;
    VclPtr<SwAddressPreview>   m_pSettingsWIN;
    VclPtr<PushButton>         m_pSettingsPB;
    VclPtr<CheckBox>           m_pHideEmptyParagraphsCB;

    VclPtr<PushButton>         m_pAssignPB;

    VclPtr<SwAddressPreview>   m_pPreviewWIN;
    VclPtr<FixedText>          m_pDocumentIndexFI;
    VclPtr<PushButton>         m_pPrevSetIB;
    VclPtr<PushButton>         m_pNextSetIB;

    OUString            m_sDocument;
    OUString            m_sCurrentAddress;
    OUString            m_sChangeAddress;

    VclPtr<SwMailMergeWizard>  m_pWizard;

    DECL_LINK(AddressListHdl_Impl, void *);
    DECL_LINK(SettingsHdl_Impl, PushButton*);
    DECL_LINK(AssignHdl_Impl, PushButton*);
    DECL_LINK(AddressBlockHdl_Impl, CheckBox*);
    DECL_LINK(InsertDataHdl_Impl, ImageButton*);
    DECL_LINK(AddressBlockSelectHdl_Impl, void *);
    DECL_LINK(HideParagraphsHdl_Impl, CheckBox*);

    void                EnableAddressBlock(bool bAll, bool bSelective);

    virtual void        ActivatePage() SAL_OVERRIDE;
    virtual bool        commitPage( ::svt::WizardTypes::CommitPageReason _eReason ) SAL_OVERRIDE;
    virtual bool        canAdvance() const SAL_OVERRIDE;

public:
    SwMailMergeAddressBlockPage(SwMailMergeWizard* _pParent);
    virtual ~SwMailMergeAddressBlockPage();
    virtual void dispose() SAL_OVERRIDE;
    SwMailMergeWizard* GetWizard() { return m_pWizard; }
};

class SwSelectAddressBlockDialog : public SfxModalDialog
{
    VclPtr<SwAddressPreview>   m_pPreview;
    VclPtr<PushButton>         m_pNewPB;
    VclPtr<PushButton>         m_pCustomizePB;
    VclPtr<PushButton>         m_pDeletePB;

    VclPtr<RadioButton>        m_pNeverRB;
    VclPtr<RadioButton>        m_pAlwaysRB;
    VclPtr<RadioButton>        m_pDependentRB;
    VclPtr<Edit>               m_pCountryED;

    com::sun::star::uno::Sequence< OUString>    m_aAddressBlocks;
    SwMailMergeConfigItem& m_rConfig;

    DECL_LINK(NewCustomizeHdl_Impl, PushButton*);
    DECL_LINK(DeleteHdl_Impl, PushButton*);
    DECL_LINK(IncludeHdl_Impl, RadioButton*);

    using Window::SetSettings;

public:
    SwSelectAddressBlockDialog(vcl::Window* pParent, SwMailMergeConfigItem& rConfig);
    virtual ~SwSelectAddressBlockDialog();
    virtual void dispose() SAL_OVERRIDE;

    void    SetAddressBlocks(const com::sun::star::uno::Sequence< OUString>& rBlocks,
                                sal_uInt16 nSelected);
    const com::sun::star::uno::Sequence< OUString>&    GetAddressBlocks();

    void                SetSettings(bool bIsCountry, const OUString& sCountry);
    bool            IsIncludeCountry() const {return !m_pNeverRB->IsChecked();}
    OUString     GetCountry() const;
};

class SwCustomizeAddressBlockDialog;
class DDListBox : public SvTreeListBox
{
    VclPtr<SwCustomizeAddressBlockDialog>   m_pParentDialog;
public:
    DDListBox(vcl::Window* pParent, const WinBits nStyle);
    virtual ~DDListBox();
    virtual void dispose() SAL_OVERRIDE;

    void SetAddressDialog(SwCustomizeAddressBlockDialog *pParent);

    virtual void        StartDrag( sal_Int8 nAction, const Point& rPosPixel ) SAL_OVERRIDE;
};

#define MOVE_ITEM_LEFT           1
#define MOVE_ITEM_RIGHT          2
#define MOVE_ITEM_UP             4
#define MOVE_ITEM_DOWN           8

class AddressMultiLineEdit : public VclMultiLineEdit, public SfxListener
{
    Link<>                          m_aSelectionLink;
    VclPtr<SwCustomizeAddressBlockDialog>  m_pParentDialog;

    using VclMultiLineEdit::Notify;

    using VclMultiLineEdit::SetText;

protected:
    bool            PreNotify( NotifyEvent& rNEvt ) SAL_OVERRIDE;
public:
    AddressMultiLineEdit(vcl::Window* pParent, WinBits nWinStyle = WB_LEFT | WB_BORDER);
    virtual ~AddressMultiLineEdit();
    virtual void    dispose() SAL_OVERRIDE;

    void            SetAddressDialog(SwCustomizeAddressBlockDialog *pParent);

    virtual void    Notify( SfxBroadcaster& rBC, const SfxHint& rHint ) SAL_OVERRIDE;

    virtual Size    GetOptimalSize() const SAL_OVERRIDE;

    void            SetSelectionChangedHdl( const Link<>& rLink ) {m_aSelectionLink = rLink;}

    void            SetText( const OUString& rStr ) SAL_OVERRIDE;
    OUString        GetAddress();

    void            InsertNewEntry( const OUString& rStr );
    void            InsertNewEntryAtPosition( const OUString& rStr, sal_uLong nPara, sal_uInt16 nIndex );
    void            RemoveCurrentEntry();

    void            MoveCurrentItem(sal_uInt16 nMove);
    sal_uInt16      IsCurrentItemMoveable();
    bool            HasCurrentItem();
    OUString        GetCurrentItem();
    void            SelectCurrentItem();
};

class SwCustomizeAddressBlockDialog : public SfxModalDialog
{
    friend class DDListBox;
    friend class AddressMultiLineEdit;
public:
    enum DialogType
    {
        ADDRESSBLOCK_NEW,
        ADDRESSBLOCK_EDIT,
        GREETING_FEMALE,
        GREETING_MALE
    };
private:
    VclPtr<FixedText>              m_pAddressElementsFT;
    VclPtr<DDListBox>              m_pAddressElementsLB;

    VclPtr<PushButton>             m_pInsertFieldIB;
    VclPtr<PushButton>             m_pRemoveFieldIB;

    VclPtr<FixedText>              m_pDragFT;
    VclPtr<AddressMultiLineEdit>   m_pDragED;
    VclPtr<PushButton>             m_pUpIB;
    VclPtr<PushButton>             m_pLeftIB;
    VclPtr<PushButton>             m_pRightIB;
    VclPtr<PushButton>             m_pDownIB;

    VclPtr<FixedText>              m_pFieldFT;
    VclPtr<ComboBox>               m_pFieldCB;
    TextFilter              m_aTextFilter;

    VclPtr<SwAddressPreview>       m_pPreviewWIN;

    VclPtr<OKButton>               m_pOK;

    ::std::vector<OUString>   m_aSalutations;
    ::std::vector<OUString>   m_aPunctuations;

    OUString                m_sCurrentSalutation;
    OUString                m_sCurrentPunctuation;
    OUString                m_sCurrentText;

    SwMailMergeConfigItem&  m_rConfigItem;
    DialogType              m_eType;

    DECL_LINK(OKHdl_Impl, void *);
    DECL_LINK(ListBoxSelectHdl_Impl, DDListBox*);
    DECL_LINK(EditModifyHdl_Impl, void *);
    DECL_LINK(ImageButtonHdl_Impl, ImageButton*);
    DECL_LINK(SelectionChangedHdl_Impl, AddressMultiLineEdit*);
    DECL_LINK(FieldChangeHdl_Impl, void *);

    bool            HasItem_Impl(sal_Int32 nUserData);
    sal_Int32       GetSelectedItem_Impl();
    void            UpdateImageButtons_Impl();

public:
    SwCustomizeAddressBlockDialog(vcl::Window* pParent, SwMailMergeConfigItem& rConfig, DialogType);
    virtual ~SwCustomizeAddressBlockDialog();
    virtual void dispose() SAL_OVERRIDE;

    void            SetAddress(const OUString& rAddress);
    OUString GetAddress();
};

class SwAssignFieldsControl;
class SwAssignFieldsDialog : public SfxModalDialog
{
    VclPtr<FixedText>               m_pMatchingFI;
    VclPtr<SwAssignFieldsControl>   m_pFieldsControl;

    VclPtr<FixedText>               m_pPreviewFI;
    VclPtr<SwAddressPreview>        m_pPreviewWIN;

    VclPtr<OKButton>                m_pOK;

    OUString                m_sNone;
    OUString                m_rPreviewString;

    SwMailMergeConfigItem&  m_rConfigItem;

    ::com::sun::star::uno::Sequence< OUString > CreateAssignments();
    DECL_LINK(OkHdl_Impl, void *);
    DECL_LINK(AssignmentModifyHdl_Impl, void*);

public:
    SwAssignFieldsDialog(vcl::Window* pParent,
                SwMailMergeConfigItem& rConfigItem,
                const OUString& rPreview,
                bool bIsAddressBlock);
    virtual ~SwAssignFieldsDialog();
    virtual void dispose() SAL_OVERRIDE;
};
#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
