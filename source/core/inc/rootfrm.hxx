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
#ifndef INCLUDED_SW_SOURCE_CORE_INC_ROOTFRM_HXX
#define INCLUDED_SW_SOURCE_CORE_INC_ROOTFRM_HXX

#include "layfrm.hxx"
#include <viewsh.hxx>
#include <doc.hxx>
#include <IDocumentTimerAccess.hxx>

class SwContentFrm;
class SwViewShell;
class SdrPage;
class SwFrameFormat;
class SwPaM;
class SwCursor;
class SwShellCrsr;
class SwTableCursor;
class SwLayVout;
class SwDestroyList;
class SwCurrShells;
class SwViewOption;
class SwSelectionList;
struct SwPosition;
struct SwCrsrMoveState;

#define INV_SIZE    1
#define INV_PRTAREA 2
#define INV_POS     4
#define INV_TABLE   8
#define INV_SECTION 16
#define INV_LINENUM 32
#define INV_DIRECTION 64

#include <vector>

/// The root element of a Writer document layout.
class SwRootFrm: public SwLayoutFrm
{
    // Needs to disable the Superfluous temporarily
    friend void AdjustSizeChgNotify( SwRootFrm *pRoot );

    // Maintains the mpLastPage (Cut() and Paste() of SwPageFrm
    friend inline void SetLastPage( SwPageFrm* );

    // For creating and destroying of the virtual output device manager
    friend void _FrmInit(); // Creates mpVout
    friend void _FrmFinit(); // Destroys mpVout

    std::vector<SwRect> maPageRects;// returns the current rectangle for each page frame
                                    // the rectangle is extended to the top/bottom/left/right
                                    // for pages located at the outer margins
    SwRect  maPagesArea;            // the area covered by the pages
    long    mnViewWidth;            // the current page layout bases on this view width
    sal_uInt16  mnColumns;          // the current page layout bases on this number of columns
    bool    mbBookMode;             // the current page layout is in book view
    bool    mbSidebarChanged;       // the notes sidebar state has changed

    bool    mbNeedGrammarCheck;     // true when sth needs to be checked (not necessarily started yet!)

    static SwLayVout     *mpVout;
    static bool           mbInPaint; // Protection against double Paints
    static bool           mbNoVirDev;// No virt. Device for SystemPaints

    bool    mbCheckSuperfluous   :1; // Search for empty Pages?
    bool    mbIdleFormat         :1; // Trigger Idle Formatter?
    bool    mbBrowseWidthValid   :1; // Is mnBrowseWidth valid?
    bool    mbTurboAllowed       :1;
    bool    mbAssertFlyPages     :1; // Insert more Pages for Flys if needed?
    bool    mbIsVirtPageNum      :1; // Do we have a virtual pagenumber?
    bool    mbIsNewLayout        :1; // Layout loaded or newly created
    bool    mbCallbackActionEnabled:1; // No Action in Notification desired
                                      // @see dcontact.cxx, ::Changed()
    bool    mbLayoutFreezed;

    /**
     * For BrowseMode
     * mnBrowseWidth is the outer margin of the object most to the right.
     * The page's right edge should not be smaller than this value.
     */
    long    mnBrowseWidth;

    /// If we only have to format one ContentFrm, its in mpTurbo
    const SwContentFrm *mpTurbo;

    /// We should not need to always struggle to find the last page, so store it here
    SwPageFrm *mpLastPage;

    /** [ Comment from the original StarOffice checkin ]:
     * The root takes care of the shell access. Via the document
     * it should be possible to get at the root frame, and thus always
     * have access to the shell.
     * the pointer mpCurrShell is the pointer to any of the shells for
     * the document.
     * Because sometimes it matters which shell is used, it is necessary to
     * know the active shell.
     * this is approximated by setting the pointer mpCurrShell when a
     * shell gets the focus (FEShell). Acditionally the pointer will be
     * set temporarily by SwCurrShell typically via  SET_CURR_SHELL
     * The macro and class can be found in the SwViewShell. These object can
     * be created nested (also for different kinds of Shells). They are
     * collected into the Array mpCurrShells.
     * Furthermore it can happen that a shell is activated while a curshell
     * object is still 'active'. This one will be entered into mpWaitingCurrShell
     * and will be activated by the last d'tor of CurrShell.
     * One other problem is the destruction of a shell while it is active.
     * The pointer mpCurrShell is then reset to an arbitrary other shell.
     * If at the time of the destruction of a shell, which is still referneced
     * by a curshell object, that will be cleaned up as well.
     */
    friend class CurrShell;
    friend void SetShell( SwViewShell *pSh );
    friend void InitCurrShells( SwRootFrm *pRoot );
    SwViewShell *mpCurrShell;
    SwViewShell *mpWaitingCurrShell;
    SwCurrShells *mpCurrShells;

    /// One Page per DrawModel per Document; is always the size of the Root
    SdrPage *mpDrawPage;

    SwDestroyList* mpDestroy;

    sal_uInt16  mnPhyPageNums; /// Page count
    sal_uInt16 mnAccessibleShells; // Number of accessible shells

    void ImplCalcBrowseWidth();
    void ImplInvalidateBrowseWidth();

    void _DeleteEmptySct(); // Destroys the registered SectionFrms
    void _RemoveFromList( SwSectionFrm* pSct ); // Removes SectionFrms from the Delete List

    virtual void DestroyImpl() SAL_OVERRIDE;
    virtual ~SwRootFrm();

protected:

    virtual void MakeAll() SAL_OVERRIDE;

public:

    /// Remove MasterObjects from the Page (called by the ctors)
    static void RemoveMasterObjs( SdrPage *pPg );

    void AllCheckPageDescs() const;
    void AllInvalidateAutoCompleteWords() const;
    void AllAddPaintRect() const;
    void AllRemoveFootnotes() ;
    void AllInvalidateSmartTagsOrSpelling(bool bSmartTags) const;

    /// Output virtual Device (e.g. for animations)
    static bool FlushVout();

    /// Save Clipping if exactly the ClipRect is outputted
    static bool HasSameRect( const SwRect& rRect );

    SwRootFrm( SwFrameFormat*, SwViewShell* );
    void Init(SwFrameFormat*);

    SwViewShell *GetCurrShell() const { return mpCurrShell; }
    void DeRegisterShell( SwViewShell *pSh );

    /**
     * Set up Start-/EndAction for all Shells on a as high as possible
     * (Shell section) level.
     * For the StarONE binding, which does not know the Shells directly.
     * The ChangeLinkd of the CrsrShell (UI notifications) is called
     * automatically in the EndAllAction.
     */
    void StartAllAction();
    void EndAllAction( bool bVirDev = false );

    /**
     * Certain UNO Actions (e.g. table cursor) require that all Actions are reset temporarily
     * In order for that to work, every SwViewShell needs to remember its old Action counter
     */
    void UnoRemoveAllActions();
    void UnoRestoreAllActions();

    const SdrPage* GetDrawPage() const { return mpDrawPage; }
          SdrPage* GetDrawPage()       { return mpDrawPage; }
          void     SetDrawPage( SdrPage* pNew ){ mpDrawPage = pNew; }

    virtual bool  GetCrsrOfst( SwPosition *, Point&,
                               SwCrsrMoveState* = 0, bool bTestBackground = false ) const SAL_OVERRIDE;

    virtual void Paint( SwRect const&,
                        SwPrintData const*const pPrintData = NULL ) const SAL_OVERRIDE;
    virtual SwTwips ShrinkFrm( SwTwips, bool bTst = false, bool bInfo = false ) SAL_OVERRIDE;
    virtual SwTwips GrowFrm  ( SwTwips, bool bTst = false, bool bInfo = false ) SAL_OVERRIDE;
#ifdef DBG_UTIL
    virtual void Cut() SAL_OVERRIDE;
    virtual void Paste( SwFrm* pParent, SwFrm* pSibling = 0 ) SAL_OVERRIDE;
#endif

    virtual bool FillSelection( SwSelectionList& rList, const SwRect& rRect ) const SAL_OVERRIDE;

    Point  GetNextPrevContentPos( const Point &rPoint, bool bNext ) const;

    virtual Size ChgSize( const Size& aNewSize ) SAL_OVERRIDE;

    void SetIdleFlags()
    {
        mbIdleFormat = true;

        SwViewShell* pCurrShell = GetCurrShell();
        // May be NULL if called from SfxBaseModel::dispose
        // (this happens in the build test 'rtfexport').
        if (pCurrShell != NULL)
            pCurrShell->GetDoc()->getIDocumentTimerAccess().StartBackgroundJobs();
    }
    bool IsIdleFormat()  const { return mbIdleFormat; }
    void ResetIdleFormat()     { mbIdleFormat = false; }

    bool IsNeedGrammarCheck() const         { return mbNeedGrammarCheck; }
    void SetNeedGrammarCheck( bool bVal )
    {
        mbNeedGrammarCheck = bVal;

        if ( bVal )
        {
            SwViewShell* pCurrShell = GetCurrShell();
            // May be NULL if called from SfxBaseModel::dispose
            // (this happens in the build test 'rtfexport').
            if (pCurrShell != NULL)
                pCurrShell->GetDoc()->getIDocumentTimerAccess().StartBackgroundJobs();
        }
    }

    /// Makes sure that all requested page-bound Flys find a Page
    void SetAssertFlyPages() { mbAssertFlyPages = true; }
    void AssertFlyPages();
    bool IsAssertFlyPages()  { return mbAssertFlyPages; }

    /**
     * Makes sure that, starting from the passed Page, all page-bound Frames
     * are on the right Page (pagenumber).
     */
    static void AssertPageFlys( SwPageFrm * );

    /// Invalidate all Content, Size or PrtArea
    void InvalidateAllContent( sal_uInt8 nInvalidate = INV_SIZE );

    /**
     * Invalidate/re-calculate the position of all floating
     * screen objects (Writer fly frames and drawing objects), which are
     * anchored to paragraph or to character.
    */
    void InvalidateAllObjPos();

    /// Remove superfluous Pages
    void SetSuperfluous()      { mbCheckSuperfluous = true; }
    bool IsSuperfluous() const { return mbCheckSuperfluous; }
    void RemoveSuperfluous();

    /**
     * Query/set the current Page and the collective Page count
     * We'll format as much as necessary
     */
    sal_uInt16  GetCurrPage( const SwPaM* ) const;
    sal_uInt16  SetCurrPage( SwCursor*, sal_uInt16 nPageNum );
    Point   GetPagePos( sal_uInt16 nPageNum ) const;
    sal_uInt16  GetPageNum() const      { return mnPhyPageNums; }
    void    DecrPhyPageNums()       { --mnPhyPageNums; }
    void    IncrPhyPageNums()       { ++mnPhyPageNums; }
    bool    IsVirtPageNum() const   { return mbIsVirtPageNum; }
    inline  void SetVirtPageNum( const bool bOf ) const;
    bool    IsDummyPage( sal_uInt16 nPageNum ) const;

    /**
     * Point rPt: The point that should be used to find the page
     * Size pSize: If given, we return the (first) page that overlaps with the
     * rectangle defined by rPt and pSize
     * bool bExtend: Extend each page to the left/right/top/botton up to the
     * next page margin
     */
    const SwPageFrm* GetPageAtPos( const Point& rPt, const Size* pSize = 0, bool bExtend = false ) const;

    void CalcFrmRects( SwShellCrsr& );

    /**
     * Calculates the cells included from the current selection
     *
     * @returns false: There was no result because of an invalid layout
     * @returns true: Everything worked fine.
     */
    bool MakeTableCrsrs( SwTableCursor& );

    void DisallowTurbo()  const { const_cast<SwRootFrm*>(this)->mbTurboAllowed = false; }
    void ResetTurboFlag() const { const_cast<SwRootFrm*>(this)->mbTurboAllowed = true; }
    bool IsTurboAllowed() const { return mbTurboAllowed; }
    void SetTurbo( const SwContentFrm *pContent ) { mpTurbo = pContent; }
    void ResetTurbo() { mpTurbo = 0; }
    const SwContentFrm *GetTurbo() { return mpTurbo; }

    /// Update the footernumbers of all Pages
    void UpdateFootnoteNums(); // Only for page by page numnbering!

    /// Remove all footnotes (but no references)
    void RemoveFootnotes( SwPageFrm *pPage = 0, bool bPageOnly = false,
                     bool bEndNotes = false );
    void CheckFootnotePageDescs( bool bEndNote );

    const SwPageFrm *GetLastPage() const { return mpLastPage; }
          SwPageFrm *GetLastPage()       { return mpLastPage; }

    static bool IsInPaint() { return mbInPaint; }

    static void SetNoVirDev( const bool bNew ) { mbNoVirDev = bNew; }

    inline long GetBrowseWidth() const;
    void SetBrowseWidth( long n ) { mbBrowseWidthValid = true; mnBrowseWidth = n;}
    inline void InvalidateBrowseWidth();

    bool IsNewLayout() const { return mbIsNewLayout; }
    void ResetNewLayout()    { mbIsNewLayout = false;}

    /**
     * Empty SwSectionFrms are registered here for deletion and
     * destroyed later on or deregistered.
     */
    void InsertEmptySct( SwSectionFrm* pDel );
    void DeleteEmptySct() { if( mpDestroy ) _DeleteEmptySct(); }
    void RemoveFromList( SwSectionFrm* pSct ) { if( mpDestroy ) _RemoveFromList( pSct ); }
#ifdef DBG_UTIL
    bool IsInDelList( SwSectionFrm* pSct ) const;
#endif

    void SetCallbackActionEnabled( bool b ) { mbCallbackActionEnabled = b; }
    bool IsCallbackActionEnabled() const    { return mbCallbackActionEnabled; }

    bool IsAnyShellAccessible() const { return mnAccessibleShells > 0; }
    void AddAccessibleShell() { ++mnAccessibleShells; }
    void RemoveAccessibleShell() { --mnAccessibleShells; }

    /**
     * Get page frame by phyiscal page number
     * looping through the lowers, which are page frame, in order to find the
     * page frame with the given physical page number.
     * if no page frame is found, 0 is returned.
     * Note: Empty page frames are also returned.
     *
     * @param _nPageNum: physical page number of page frame to be searched and
     *                   returned.
     *
     * @return pointer to the page frame with the given physical page number
    */
    SwPageFrm* GetPageByPageNum( sal_uInt16 _nPageNum ) const;

    void CheckViewLayout( const SwViewOption* pViewOpt, const SwRect* pVisArea );
    bool IsLeftToRightViewLayout() const;
    const SwRect& GetPagesArea() const { return maPagesArea; }
    void SetSidebarChanged() { mbSidebarChanged = true; }

    bool IsLayoutFreezed() const { return mbLayoutFreezed; }
    void FreezeLayout( bool freeze ) { mbLayoutFreezed = freeze; }
};

inline long SwRootFrm::GetBrowseWidth() const
{
    if ( !mbBrowseWidthValid )
        const_cast<SwRootFrm*>(this)->ImplCalcBrowseWidth();
    return mnBrowseWidth;
}

inline void SwRootFrm::InvalidateBrowseWidth()
{
    if ( mbBrowseWidthValid )
        ImplInvalidateBrowseWidth();
}

inline  void SwRootFrm::SetVirtPageNum( const bool bOf) const
{
    const_cast<SwRootFrm*>(this)->mbIsVirtPageNum = bOf;
}

#endif // INCLUDED_SW_SOURCE_CORE_INC_ROOTFRM_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
