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

#include <stdlib.h>

#include <libxml/xmlwriter.h>

#include <node.hxx>
#include <doc.hxx>
#include <IDocumentUndoRedo.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <pam.hxx>
#include <txtfld.hxx>
#include <fmtfld.hxx>
#include <hints.hxx>
#include <numrule.hxx>
#include <ndtxt.hxx>
#include <ndnotxt.hxx>
#include <swtable.hxx>
#include <tblsel.hxx>
#include <section.hxx>
#include <ddefld.hxx>
#include <swddetbl.hxx>
#include <frame.hxx>
#include <txtatr.hxx>
#include <tox.hxx>
#include <fmtrfmrk.hxx>
#include <fmtftn.hxx>

#include <docsh.hxx>
#include <svl/smplhint.hxx>

typedef std::vector<SwStartNode*> SwSttNdPtrs;

// function to determine the highest level in the given range
sal_uInt16 HighestLevel( SwNodes & rNodes, const SwNodeRange & rRange );

/** Constructor
 *
 * creates the base sections (PostIts, Inserts, AutoText, RedLines, Content)
 *
 * @param pDocument TODO: provide documentation
 */
SwNodes::SwNodes( SwDoc* pDocument )
    : vIndices(nullptr), pMyDoc( pDocument )
{
    bInNodesDel = bInDelUpdOutl = bInDelUpdNum = false;

    OSL_ENSURE( pMyDoc, "in which Doc am I?" );

    sal_uLong nPos = 0;
    SwStartNode* pSttNd = new SwStartNode( *this, nPos++ );
    pEndOfPostIts = new SwEndNode( *this, nPos++, *pSttNd );

    SwStartNode* pTmp = new SwStartNode( *this, nPos++ );
    pEndOfInserts = new SwEndNode( *this, nPos++, *pTmp );

    pTmp = new SwStartNode( *this, nPos++ );
    pTmp->pStartOfSection = pSttNd;
    pEndOfAutotext = new SwEndNode( *this, nPos++, *pTmp );

    pTmp = new SwStartNode( *this, nPos++ );
    pTmp->pStartOfSection = pSttNd;
    pEndOfRedlines = new SwEndNode( *this, nPos++, *pTmp );

    pTmp = new SwStartNode( *this, nPos++ );
    pTmp->pStartOfSection = pSttNd;
    pEndOfContent = new SwEndNode( *this, nPos++, *pTmp );

    pOutlineNds = new SwOutlineNodes;
}

/** Destructor
 *
 * Deletes all nodes whose pointer are in a dynamic array. This should be no
 * problem as nodes cannot be created outside this array and, thus, cannot be
 * part of multiple arrays.
 */
SwNodes::~SwNodes()
{
    delete pOutlineNds;

    {
        SwNodeIndex aNdIdx( *this );
        while( true )
        {
            SwNode *pNode = &aNdIdx.GetNode();
            if( pNode == pEndOfContent )
                break;

            ++aNdIdx;
            delete pNode;
        }
    }

    // here, all SwNodeIndices must be unregistered
    delete pEndOfContent;
}

void SwNodes::ChgNode( SwNodeIndex& rDelPos, sal_uLong nSz,
                        SwNodeIndex& rInsPos, bool bNewFrms )
{
    // no need for frames in the UndoArea
    SwNodes& rNds = rInsPos.GetNodes();
    const SwNode* pPrevInsNd = rNds[ rInsPos.GetIndex() -1 ];

    // declare all fields as invalid, updating will happen
    // in the idle-handler of the doc
    if( GetDoc()->getIDocumentFieldsAccess().SetFieldsDirty( true, &rDelPos.GetNode(), nSz ) &&
        rNds.GetDoc() != GetDoc() )
        rNds.GetDoc()->getIDocumentFieldsAccess().SetFieldsDirty( true, NULL, 0 );

    // NEVER include nodes from the RedLineArea
    sal_uLong nNd = rInsPos.GetIndex();
    bool bInsOutlineIdx = !(
            rNds.GetEndOfRedlines().StartOfSectionNode()->GetIndex() < nNd &&
            nNd < rNds.GetEndOfRedlines().GetIndex() );

    if( &rNds == this ) // if in the same node array -> move
    {
        // Move order: from front to back, so that new entries are added at
        // first position, thus, deletion position stays the same
        const sal_uLong nDiff = rDelPos.GetIndex() < rInsPos.GetIndex() ? 0 : 1;

        for( sal_uLong n = rDelPos.GetIndex(); nSz; n += nDiff, --nSz )
        {
            SwNodeIndex aDelIdx( *this, n );
            SwNode& rNd = aDelIdx.GetNode();

            // #i57920# - correction of refactoring done by cws swnumtree:
            // - <SwTextNode::SetLevel( NO_NUMBERING ) is deprecated and
            //   set <IsCounted> state of the text node to <false>, which
            //   isn't correct here.
            if ( rNd.IsTextNode() )
            {
                SwTextNode* pTextNode = rNd.GetTextNode();

                pTextNode->RemoveFromList();

                if (pTextNode->IsOutline())
                {
                    const SwNodePtr pSrch = (SwNodePtr)&rNd;
                    pOutlineNds->erase( pSrch );
                }
            }

            BigPtrArray::Move( aDelIdx.GetIndex(), rInsPos.GetIndex() );

            if( rNd.IsTextNode() )
            {
                SwTextNode& rTextNd = static_cast<SwTextNode&>(rNd);

                rTextNd.AddToList();

                if (bInsOutlineIdx && rTextNd.IsOutline())
                {
                    const SwNodePtr pSrch = (SwNodePtr)&rNd;
                    pOutlineNds->insert( pSrch );
                }
                rTextNd.InvalidateNumRule();

//FEATURE::CONDCOLL
                if( RES_CONDTXTFMTCOLL == rTextNd.GetTextColl()->Which() )
                    rTextNd.ChkCondColl();
//FEATURE::CONDCOLL
            }
            else if( rNd.IsContentNode() )
                static_cast<SwContentNode&>(rNd).InvalidateNumRule();
        }
    }
    else
    {
        bool bSavePersData(GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(rNds));
        bool bRestPersData(GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(*this));
        SwDoc* pDestDoc = rNds.GetDoc() != GetDoc() ? rNds.GetDoc() : 0;
        OSL_ENSURE(!pDestDoc, "SwNodes::ChgNode(): "
            "the code to handle text fields here looks broken\n"
            "if the target is in a different document.");
        if( !bRestPersData && !bSavePersData && pDestDoc )
            bSavePersData = bRestPersData = true;

        OUString sNumRule;
        SwNodeIndex aInsPos( rInsPos );
        for( sal_uLong n = 0; n < nSz; n++ )
        {
            SwNode* pNd = &rDelPos.GetNode();

            // NoTextNode keep their persistent data
            if( pNd->IsNoTextNode() )
            {
                if( bSavePersData )
                    static_cast<SwNoTextNode*>(pNd)->SavePersistentData();
            }
            else if( pNd->IsTextNode() )
            {
                SwTextNode* pTextNd = static_cast<SwTextNode*>(pNd);

                // remove outline index from old nodes array
                if (pTextNd->IsOutline())
                {
                    pOutlineNds->erase( pNd );
                }

                // copy rules if needed
                if( pDestDoc )
                {
                    const SwNumRule* pNumRule = pTextNd->GetNumRule();
                    if( pNumRule && sNumRule != pNumRule->GetName() )
                    {
                        sNumRule = pNumRule->GetName();
                        SwNumRule* pDestRule = pDestDoc->FindNumRulePtr( sNumRule );
                        if( pDestRule )
                            pDestRule->SetInvalidRule( true );
                        else
                            pDestDoc->MakeNumRule( sNumRule, pNumRule );
                    }
                }
                else
                    // if movement into the UndoNodes-array, update numbering
                    pTextNd->InvalidateNumRule();

                pTextNd->RemoveFromList();
            }

            RemoveNode( rDelPos.GetIndex(), 1, false ); // move indices
            SwContentNode * pCNd = pNd->GetContentNode();
            rNds.InsertNode( pNd, aInsPos );

            if( pCNd )
            {
                SwTextNode* pTextNd = pCNd->GetTextNode();
                if( pTextNd )
                {
                    SwpHints * const pHts = pTextNd->GetpSwpHints();
                    // OultineNodes set the new nodes in the array
                    if (bInsOutlineIdx && pTextNd->IsOutline())
                    {
                        rNds.pOutlineNds->insert( pTextNd );
                    }

                    pTextNd->AddToList();

                    // special treatment for fields
                    if( pHts && pHts->Count() )
                    {
                        bool const bToUndo = !pDestDoc &&
                            GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(rNds);
                        for( size_t i = pHts->Count(); i; )
                        {
                            SwTextAttr * const pAttr = pHts->GetTextHint( --i );
                            switch ( pAttr->Which() )
                            {
                            case RES_TXTATR_FIELD:
                            case RES_TXTATR_ANNOTATION:
                            case RES_TXTATR_INPUTFIELD:
                                {
                                    SwTextField* pTextField = static_txtattr_cast<SwTextField*>(pAttr);
                                    rNds.GetDoc()->getIDocumentFieldsAccess().InsDelFieldInFieldLst( !bToUndo, *pTextField );

                                    const SwFieldType* pTyp = pTextField->GetFormatField().GetField()->GetTyp();
                                    if ( RES_POSTITFLD == pTyp->Which() )
                                    {
                                        rNds.GetDoc()->GetDocShell()->Broadcast(
                                            SwFormatFieldHint(
                                                &pTextField->GetFormatField(),
                                                ( pTextField->GetFormatField().IsFieldInDoc()
                                                  ? SwFormatFieldHintWhich::INSERTED
                                                  : SwFormatFieldHintWhich::REMOVED ) ) );
                                    }
                                    else if( RES_DDEFLD == pTyp->Which() )
                                    {
                                        if( bToUndo )
                                            const_cast<SwDDEFieldType*>(static_cast<const SwDDEFieldType*>(pTyp))->DecRefCnt();
                                        else
                                            const_cast<SwDDEFieldType*>(static_cast<const SwDDEFieldType*>(pTyp))->IncRefCnt();
                                    }
                                    static_cast<SwFormatField&>(pAttr->GetAttr())
                                        .InvalidateField();
                                }
                                break;

                            case RES_TXTATR_FTN:
                                static_cast<SwFormatFootnote&>(pAttr->GetAttr())
                                    .InvalidateFootnote();
                                break;

                            case RES_TXTATR_TOXMARK:
                                static_cast<SwTOXMark&>(pAttr->GetAttr())
                                    .InvalidateTOXMark();
                                break;

                            case RES_TXTATR_REFMARK:
                                static_cast<SwFormatRefMark&>(pAttr->GetAttr())
                                    .InvalidateRefMark();
                                break;

                            case RES_TXTATR_META:
                            case RES_TXTATR_METAFIELD:
                                {
                                    SwTextMeta *const pTextMeta(
                                        static_txtattr_cast<SwTextMeta*>(pAttr));
                                    // force removal of UNO object
                                    pTextMeta->ChgTextNode(0);
                                    pTextMeta->ChgTextNode(pTextNd);
                                }
                                break;

                            default:
                                break;
                            }
                        }
                    }
                    //FEATURE::CONDCOLL
                    if( RES_CONDTXTFMTCOLL == pTextNd->GetTextColl()->Which() )
                        pTextNd->ChkCondColl();
                    //FEATURE::CONDCOLL
                }
                else
                {
                    // Moved into different Docs? Persist data again!
                    if( pCNd->IsNoTextNode() && bRestPersData )
                        static_cast<SwNoTextNode*>(pCNd)->RestorePersistentData();
                }
            }
        }
    }

    // declare all fields as invalid, updating will happen
    // in the idle-handler of the doc
    GetDoc()->getIDocumentFieldsAccess().SetFieldsDirty( true, NULL, 0 );
    if( rNds.GetDoc() != GetDoc() )
        rNds.GetDoc()->getIDocumentFieldsAccess().SetFieldsDirty( true, NULL, 0 );

    if( bNewFrms )
        bNewFrms = &GetDoc()->GetNodes() == (const SwNodes*)&rNds &&
                    GetDoc()->getIDocumentLayoutAccess().GetCurrentViewShell();

    if( bNewFrms )
    {
        // get the frames:
        SwNodeIndex aIdx( *pPrevInsNd, 1 );
        SwNodeIndex aFrmNdIdx( aIdx );
        SwNode* pFrmNd = rNds.FindPrvNxtFrmNode( aFrmNdIdx,
                                        rNds[ rInsPos.GetIndex() - 1 ] );

        if( !pFrmNd && aFrmNdIdx > rNds.GetEndOfExtras().GetIndex() )
        {
            OSL_ENSURE( false, "here, something wrong happened" );
            aFrmNdIdx = rNds.GetEndOfContent();
            pFrmNd = SwNodes::GoPrevSection( &aFrmNdIdx, true, false );
            if( pFrmNd && !static_cast<SwContentNode*>(pFrmNd)->HasWriterListeners() )
                pFrmNd = 0;
            OSL_ENSURE( pFrmNd, "ChgNode() - no FrameNode found" );
        }
        if( pFrmNd )
            while( aIdx != rInsPos )
            {
                SwContentNode* pCNd = aIdx.GetNode().GetContentNode();
                if( pCNd )
                {
                    if( pFrmNd->IsTableNode() )
                        static_cast<SwTableNode*>(pFrmNd)->MakeFrms( aIdx );
                    else if( pFrmNd->IsSectionNode() )
                        static_cast<SwSectionNode*>(pFrmNd)->MakeFrms( aIdx );
                    else
                        static_cast<SwContentNode*>(pFrmNd)->MakeFrms( *pCNd );
                    pFrmNd = pCNd;
                }
                ++aIdx;
            }
    }
}

// TODO: provide documentation
/** move the node pointer
 *
 * Move the node pointer from "(inclusive) start position to (exclusive) end
 * position" to target position.
 * If the target is in front of the first or in the area between first and
 * last element to move, nothing happens.
 * If the area to move is empty or the end position is before the start
 * position, nothing happens.
 *
 * @param aRange range to move (excluding end node)
 * @param rNodes
 * @param aIndex
 * @param bNewFrms
 * @return
 */
bool SwNodes::_MoveNodes( const SwNodeRange& aRange, SwNodes & rNodes,
                    const SwNodeIndex& aIndex, bool bNewFrms )
{
    SwNode * pAktNode;
    if( aIndex == 0 ||
        ( (pAktNode = &aIndex.GetNode())->GetStartNode() &&
          !pAktNode->StartOfSectionIndex() ))
        return false;

    SwNodeRange aRg( aRange );

    // skip "simple" start or end nodes
    while( ND_STARTNODE == (pAktNode = &aRg.aStart.GetNode())->GetNodeType()
            || ( pAktNode->IsEndNode() &&
                !pAktNode->pStartOfSection->IsSectionNode() ) )
        ++aRg.aStart;
    --aRg.aStart;

    // if aEnd-1 points to no ContentNode, search previous one
    --aRg.aEnd;
    while( ( (( pAktNode = &aRg.aEnd.GetNode())->GetStartNode() &&
            !pAktNode->IsSectionNode() ) ||
            ( pAktNode->IsEndNode() &&
            ND_STARTNODE == pAktNode->pStartOfSection->GetNodeType()) ) &&
            aRg.aEnd > aRg.aStart )
        --aRg.aEnd;

    // if in same array, check insertion position
    if( aRg.aStart >= aRg.aEnd )
        return false;

    if( this == &rNodes )
    {
        if( ( aIndex.GetIndex()-1 >= aRg.aStart.GetIndex() &&
              aIndex.GetIndex()-1 < aRg.aEnd.GetIndex()) ||
            ( aIndex.GetIndex()-1 == aRg.aEnd.GetIndex() ) )
            return false;
    }

    sal_uLong nInsPos = 0; // counter for tmp array

    // array as a stack, storing all StartOfSelections
    SwSttNdPtrs aSttNdStack;
    SwSttNdPtrs::size_type nLevel = 0; // level counter

    // set start index
    SwNodeIndex  aIdx( aIndex );

    SwStartNode* pStartNode = aIdx.GetNode().pStartOfSection;
    aSttNdStack.insert( aSttNdStack.begin(), pStartNode );

    SwNodeRange aOrigInsPos( aIdx, -1, aIdx ); // original insertion position

    // call DelFrms/MakeFrms for the upmost SectionNode
    int nSectNdCnt = 0;
    bool bSaveNewFrms = bNewFrms;

    // continue until everything has been moved
    while( aRg.aStart < aRg.aEnd )
        switch( (pAktNode = &aRg.aEnd.GetNode())->GetNodeType() )
        {
        case ND_ENDNODE:
            {
                if( nInsPos ) // move everything until here
                {
                    // delete and copy. CAUTION: all indices after
                    // "aRg.aEnd+1" will be moved as well!
                    SwNodeIndex aSwIndex( aRg.aEnd, 1 );
                    ChgNode( aSwIndex, nInsPos, aIdx, bNewFrms );
                    aIdx -= nInsPos;
                    nInsPos = 0;
                }

                SwStartNode* pSttNd = pAktNode->pStartOfSection;
                if( pSttNd->IsTableNode() )
                {
                    SwTableNode* pTableNd = static_cast<SwTableNode*>(pSttNd);

                    // move the whole table/range
                    nInsPos = (aRg.aEnd.GetIndex() -
                                    pSttNd->GetIndex() )+1;
                    aRg.aEnd -= nInsPos;

                    // NEVER include nodes from the RedLineArea
                    sal_uLong nNd = aIdx.GetIndex();
                    bool bInsOutlineIdx = !( rNodes.GetEndOfRedlines().
                            StartOfSectionNode()->GetIndex() < nNd &&
                            nNd < rNodes.GetEndOfRedlines().GetIndex() );

                    if( bNewFrms )
                        // delete all frames
                        pTableNd->DelFrms();
                    if( &rNodes == this ) // move into self?
                    {
                        // move all Start/End/ContentNodes
                        // ContentNodes: delete also the frames!
                        pTableNd->pStartOfSection = aIdx.GetNode().pStartOfSection;
                        for( sal_uLong n = 0; n < nInsPos; ++n )
                        {
                            SwNodeIndex aMvIdx( aRg.aEnd, 1 );
                            SwContentNode* pCNd = 0;
                            SwNode* pTmpNd = &aMvIdx.GetNode();
                            if( pTmpNd->IsContentNode() )
                            {
                                pCNd = static_cast<SwContentNode*>(pTmpNd);
                                if( pTmpNd->IsTextNode() )
                                    static_cast<SwTextNode*>(pTmpNd)->RemoveFromList();

                                // remove outline index from old nodes array
                                if (pCNd->IsTextNode() && pCNd->GetTextNode()->IsOutline())
                                {
                                    pOutlineNds->erase( pCNd );
                                }
                                else
                                    pCNd = 0;
                            }

                            BigPtrArray::Move( aMvIdx.GetIndex(), aIdx.GetIndex() );

                            if( bInsOutlineIdx && pCNd )
                                pOutlineNds->insert( pCNd );
                            if( pTmpNd->IsTextNode() )
                                static_cast<SwTextNode*>(pTmpNd)->AddToList();
                        }
                    }
                    else
                    {
                        // get StartNode
                        // Even aIdx points to a startnode, we need the startnode
                        // of the environment of aIdx (#i80941)
                        SwStartNode* pSttNode = aIdx.GetNode().pStartOfSection;

                        // get all boxes with content because their indices
                        // pointing to the StartNodes need to be reset
                        // (copying the array and deleting all found ones eases
                        // searching)
                        SwNodeIndex aMvIdx( aRg.aEnd, 1 );
                        for( sal_uLong n = 0; n < nInsPos; ++n )
                        {
                            SwNode* pNd = &aMvIdx.GetNode();

                            const bool bOutlNd = pNd->IsTextNode() && pNd->GetTextNode()->IsOutline();
                            // delete outline indices from old node array
                            if( bOutlNd )
                                pOutlineNds->erase( pNd );

                            RemoveNode( aMvIdx.GetIndex(), 1, false );
                            pNd->pStartOfSection = pSttNode;
                            rNodes.InsertNode( pNd, aIdx );

                            // set correct indices in Start/EndNodes
                            if( bInsOutlineIdx && bOutlNd )
                                // and put them into the new node array
                                rNodes.pOutlineNds->insert( pNd );
                            else if( pNd->IsStartNode() )
                                pSttNode = static_cast<SwStartNode*>(pNd);
                            else if( pNd->IsEndNode() )
                            {
                                pSttNode->pEndOfSection = static_cast<SwEndNode*>(pNd);
                                if( pSttNode->IsSectionNode() )
                                    static_cast<SwSectionNode*>(pSttNode)->NodesArrChgd();
                                pSttNode = pSttNode->pStartOfSection;
                            }
                        }

                        if( pTableNd->GetTable().IsA( TYPE( SwDDETable ) ))
                        {
                            SwDDEFieldType* pTyp = static_cast<SwDDETable&>(pTableNd->
                                                GetTable()).GetDDEFieldType();
                            if( pTyp )
                            {
                                if( rNodes.IsDocNodes() )
                                    pTyp->IncRefCnt();
                                else
                                    pTyp->DecRefCnt();
                            }
                        }

                        if (GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(
                                    rNodes))
                        {
                            SwFrameFormat* pTableFormat = pTableNd->GetTable().GetFrameFormat();
                            SwPtrMsgPoolItem aMsgHint( RES_REMOVE_UNO_OBJECT,
                                                        pTableFormat );
                            pTableFormat->ModifyNotification( &aMsgHint, &aMsgHint );
                        }
                    }
                    if( bNewFrms )
                    {
                        SwNodeIndex aTmp( aIdx );
                        pTableNd->MakeFrms( &aTmp );
                    }
                    aIdx -= nInsPos;
                    nInsPos = 0;
                }
                else if( pSttNd->GetIndex() < aRg.aStart.GetIndex() )
                {
                    // SectionNode: not the whole section will be moved, thus,
                    //              move only the ContentNodes
                    // StartNode:   create a new section at the given position
                    do {        // middle check loop
                        if( !pSttNd->IsSectionNode() )
                        {
                            // create StartNode and EndNode at InsertPos
                            SwStartNode* pTmp = new SwStartNode( aIdx,
                                                    ND_STARTNODE,
/*?? NodeType ??*/                                  SwNormalStartNode );

                            nLevel++; // put the index to StartNode on the stack
                            aSttNdStack.insert( aSttNdStack.begin() + nLevel, pTmp );

                            // create EndNode
                            new SwEndNode( aIdx, *pTmp );
                        }
                        else if (GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(
                                    rNodes))
                        {
                            // use placeholder in UndoNodes array
                            new SwPlaceholderNode(aIdx);
                        }
                        else
                        {
                            // JP 18.5.2001 (Bug 70454) creating new section?
                            --aRg.aEnd;
                            break;

                        }

                        --aRg.aEnd;
                        --aIdx;
                    } while( false );
                }
                else
                {
                    // move StartNode and EndNode in total

                    // if Start is exactly the Start of the area,
                    // then the Node needs to be re-visited
                    if( &aRg.aStart.GetNode() == pSttNd )
                        --aRg.aStart;

                    SwSectionNode* pSctNd = pSttNd->GetSectionNode();
                    if( bNewFrms && pSctNd )
                        pSctNd->DelFrms();

                    RemoveNode( aRg.aEnd.GetIndex(), 1, false ); // EndNode loeschen
                    sal_uLong nSttPos = pSttNd->GetIndex();

                    // this StartNode will be removed later
                    SwStartNode* pTmpSttNd = new SwStartNode( *this, nSttPos+1 );
                    pTmpSttNd->pStartOfSection = pSttNd->pStartOfSection;

                    RemoveNode( nSttPos, 1, false ); // SttNode loeschen

                    pSttNd->pStartOfSection = aIdx.GetNode().pStartOfSection;
                    rNodes.InsertNode( pSttNd, aIdx  );
                    rNodes.InsertNode( pAktNode, aIdx );
                    --aIdx;
                    pSttNd->pEndOfSection = static_cast<SwEndNode*>(pAktNode);

                    --aRg.aEnd;

                    nLevel++; // put the index pointing to the StartNode onto the stack
                    aSttNdStack.insert( aSttNdStack.begin() + nLevel, pSttNd );

                    // reset remaining indices if SectionNode
                    if( pSctNd )
                    {
                        pSctNd->NodesArrChgd();
                        ++nSectNdCnt;
                        bNewFrms = false;
                    }
                }
            }
            break;

        case ND_SECTIONNODE:
            if( !nLevel &&
                GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(rNodes))
            {
                // here, a SectionDummyNode needs to be inserted at the current position
                if( nInsPos ) // move everything until here
                {
                    // delete and copy. CAUTION: all indices after
                    // "aRg.aEnd+1" will be moved as well!
                    SwNodeIndex aSwIndex( aRg.aEnd, 1 );
                    ChgNode( aSwIndex, nInsPos, aIdx, bNewFrms );
                    aIdx -= nInsPos;
                    nInsPos = 0;
                }
                new SwPlaceholderNode(aIdx);
                --aRg.aEnd;
                --aIdx;
                break;
            }
            // no break !!
        case ND_TABLENODE:
        case ND_STARTNODE:
            {
                // empty section -> nothing to do
                //  and only if it's a top level section
                if( !nInsPos && !nLevel )
                {
                    --aRg.aEnd;
                    break;
                }

                if( !nLevel ) // level is decreasing
                {
                    // create decrease
                    SwNodeIndex aTmpSIdx( aOrigInsPos.aStart, 1 );
                    SwStartNode* pTmpStt = new SwStartNode( aTmpSIdx,
                                ND_STARTNODE,
                                static_cast<SwStartNode*>(pAktNode)->GetStartNodeType() );

                    --aTmpSIdx;

                    SwNodeIndex aTmpEIdx( aOrigInsPos.aEnd );
                    new SwEndNode( aTmpEIdx, *pTmpStt );
                    --aTmpEIdx;
                    ++aTmpSIdx;

                    // set correct StartOfSection
                    ++aRg.aEnd;
                    {
                        SwNodeIndex aCntIdx( aRg.aEnd );
                        for( sal_uLong n = 0; n < nInsPos; n++, ++aCntIdx)
                            aCntIdx.GetNode().pStartOfSection = pTmpStt;
                    }

                    // also set correct StartNode for all decreased nodes
                    while( aTmpSIdx < aTmpEIdx )
                        if( 0 != (( pAktNode = &aTmpEIdx.GetNode())->GetEndNode()) )
                            aTmpEIdx = pAktNode->StartOfSectionIndex();
                        else
                        {
                            pAktNode->pStartOfSection = pTmpStt;
                            --aTmpEIdx;
                        }

                    --aIdx;     // after the inserted StartNode
                    --aRg.aEnd; // before StartNode
                    // copy array. CAUTION: all indices after
                    // "aRg.aEnd+1" will be moved as well!
                    SwNodeIndex aSwIndex( aRg.aEnd, 1 );
                    ChgNode( aSwIndex, nInsPos, aIdx, bNewFrms );
                    aIdx -= nInsPos+1;
                    nInsPos = 0;
                }
                else // all nodes between StartNode and EndNode were moved
                {
                    OSL_ENSURE( pAktNode == aSttNdStack[nLevel] ||
                            ( pAktNode->IsStartNode() &&
                                aSttNdStack[nLevel]->IsSectionNode()),
                             "wrong StartNode" );

                    SwNodeIndex aSwIndex( aRg.aEnd, 1 );
                    ChgNode( aSwIndex, nInsPos, aIdx, bNewFrms );
                    aIdx -= nInsPos+1; // before inserted StartNode
                    nInsPos = 0;

                    // remove pointer from node array
                    RemoveNode( aRg.aEnd.GetIndex(), 1, true );
                    --aRg.aEnd;

                    SwSectionNode* pSectNd = aSttNdStack[ nLevel ]->GetSectionNode();
                    if( pSectNd && !--nSectNdCnt )
                    {
                        SwNodeIndex aTmp( *pSectNd );
                        pSectNd->MakeFrms( &aTmp );
                        bNewFrms = bSaveNewFrms;
                    }
                    aSttNdStack.erase( aSttNdStack.begin() + nLevel ); // remove from stack
                    nLevel--;
                }

                // delete all resulting empty start/end node pairs
                SwNode* pTmpNode = (*this)[ aRg.aEnd.GetIndex()+1 ]->GetEndNode();
                if( pTmpNode && ND_STARTNODE == (pAktNode = &aRg.aEnd.GetNode())
                    ->GetNodeType() && pAktNode->StartOfSectionIndex() &&
                    pTmpNode->StartOfSectionNode() == pAktNode )
                {
                    DelNodes( aRg.aEnd, 2 );
                    --aRg.aEnd;
                }
            }
            break;

        case ND_TEXTNODE:
            //Add special function to text node.
            {
                if( bNewFrms && pAktNode->GetContentNode() )
                    static_cast<SwContentNode*>(pAktNode)->DelFrms();
                pAktNode->pStartOfSection = aSttNdStack[ nLevel ];
                nInsPos++;
                --aRg.aEnd;
            }
            break;
        case ND_GRFNODE:
        case ND_OLENODE:
            {
                if( bNewFrms && pAktNode->GetContentNode() )
                    static_cast<SwContentNode*>(pAktNode)->DelFrms();

                pAktNode->pStartOfSection = aSttNdStack[ nLevel ];
                nInsPos++;
                --aRg.aEnd;
            }
            break;

        case ND_PLACEHOLDER:
            if (GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(*this))
            {
                if( &rNodes == this ) // inside UndoNodesArray
                {
                    // move everything
                    pAktNode->pStartOfSection = aSttNdStack[ nLevel ];
                    nInsPos++;
                }
                else // move into "normal" node array
                {
                    // than a SectionNode (start/end) is needed at the current
                    // InsPos; if so skip it, otherwise ignore current node
                    if( nInsPos ) // move everything until here
                    {
                        // delete and copy. CAUTION: all indices after
                        // "aRg.aEnd+1" will be moved as well!
                        SwNodeIndex aSwIndex( aRg.aEnd, 1 );
                        ChgNode( aSwIndex, nInsPos, aIdx, bNewFrms );
                        aIdx -= nInsPos;
                        nInsPos = 0;
                    }
                    SwNode* pTmpNd = &aIdx.GetNode();
                    if( pTmpNd->IsSectionNode() ||
                        pTmpNd->StartOfSectionNode()->IsSectionNode() )
                        --aIdx; // skip
                }
            }
            else {
                OSL_FAIL( "How can this node be in the node array?" );
            }
            --aRg.aEnd;
            break;

        default:
            OSL_FAIL( "Unknown node type" );
            break;
        }

    if( nInsPos ) // copy remaining rest
    {
        // rest should be ok
        SwNodeIndex aSwIndex( aRg.aEnd, 1 );
        ChgNode( aSwIndex, nInsPos, aIdx, bNewFrms );
    }
    ++aRg.aEnd; // again, exclusive end

    // delete all resulting empty start/end node pairs
    if( ( pAktNode = &aRg.aStart.GetNode())->GetStartNode() &&
        pAktNode->StartOfSectionIndex() &&
        aRg.aEnd.GetNode().GetEndNode() )
            DelNodes( aRg.aStart, 2 );

    // initialize numbering update
    ++aOrigInsPos.aStart;
    // Moved in same node array? Then call update top down!
    if( this == &rNodes &&
        aRg.aEnd.GetIndex() >= aOrigInsPos.aStart.GetIndex() )
    {
        UpdateOutlineIdx( aOrigInsPos.aStart.GetNode() );
        UpdateOutlineIdx( aRg.aEnd.GetNode() );
    }
    else
    {
        UpdateOutlineIdx( aRg.aEnd.GetNode() );
        rNodes.UpdateOutlineIdx( aOrigInsPos.aStart.GetNode() );
    }

    return true;
}

/** create a start/end section pair
 *
 * Other nodes might be in between.
 *
 * After this method call, the start node of pRange will be pointing to the
 * first node after the start section node and the end node will be the index
 * of the end section node. If this method is called multiple times with the
 * same input, multiple sections containing the previous ones will be created
 * (no content nodes between start or end node).
 *
 * @note Start and end node of the range must be on the same level but MUST
 *       NOT be on the top level.
 *
 * @param [IN,OUT] pRange the range (excl. end)
 * @param eSttNdTyp type of the start node
 */
void SwNodes::SectionDown(SwNodeRange *pRange, SwStartNodeType eSttNdTyp )
{
    if( pRange->aStart >= pRange->aEnd ||
        pRange->aEnd >= Count() ||
        !CheckNodesRange( pRange->aStart, pRange->aEnd ))
        return;

    // If the beginning of a range is before or at a start node position, so
    // delete it, otherwise empty S/E or E/S nodes would be created.
    // For other nodes, create a new start node.
    SwNode * pAktNode = &pRange->aStart.GetNode();
    SwNodeIndex aTmpIdx( *pAktNode->StartOfSectionNode() );

    if( pAktNode->GetEndNode() )
        DelNodes( pRange->aStart, 1 ); // prevent empty section
    else
    {
        // insert a new StartNode
        SwNode* pSttNd = new SwStartNode( pRange->aStart, ND_STARTNODE, eSttNdTyp );
        pRange->aStart = *pSttNd;
        aTmpIdx = pRange->aStart;
    }

    // If the end of a range is before or at a StartNode, so delete it,
    // otherwise empty S/E or E/S nodes would be created.
    // For other nodes, insert a new end node.
    --pRange->aEnd;
    if( pRange->aEnd.GetNode().GetStartNode() )
        DelNodes( pRange->aEnd, 1 );
    else
    {
        ++pRange->aEnd;
        // insert a new EndNode
        new SwEndNode( pRange->aEnd, *pRange->aStart.GetNode().GetStartNode() );
    }
    --pRange->aEnd;

    SectionUpDown( aTmpIdx, pRange->aEnd );
}

/** increase level of the given range
 *
 * The range contained in pRange will be lifted to the next higher level.
 * This is done by adding a end node at pRange.start and a start node at
 * pRange.end. Furthermore all indices for this range will be updated.
 *
 * After this method call, the start node of pRange will be pointing to the
 * first node inside the lifted range and the end node will be pointing to the
 * last position inside the lifted range.
 *
 * @param [IN,OUT] pRange the range of nodes where the level should be increased
 */
void SwNodes::SectionUp(SwNodeRange *pRange)
{
    if( pRange->aStart >= pRange->aEnd ||
        pRange->aEnd >= Count() ||
        !CheckNodesRange( pRange->aStart, pRange->aEnd ) ||
        !( HighestLevel( *this, *pRange ) > 1 ))
        return;

    // If the beginning of a range is before or at a start node position, so
    // delete it, otherwise empty S/E or E/S nodes would be created.
    // For other nodes, create a new start node.
    SwNode * pAktNode = &pRange->aStart.GetNode();
    SwNodeIndex aIdx( *pAktNode->StartOfSectionNode() );
    if( pAktNode->IsStartNode() )       // selbst StartNode
    {
        SwEndNode* pEndNd = pRange->aEnd.GetNode().GetEndNode();
        if( pAktNode == pEndNd->pStartOfSection )
        {
            // there was a pairwise reset, adjust only those in the range
            SwStartNode* pTmpSttNd = pAktNode->pStartOfSection;
            RemoveNode( pRange->aStart.GetIndex(), 1, true );
            RemoveNode( pRange->aEnd.GetIndex(), 1, true );

            SwNodeIndex aTmpIdx( pRange->aStart );
            while( aTmpIdx < pRange->aEnd )
            {
                pAktNode = &aTmpIdx.GetNode();
                pAktNode->pStartOfSection = pTmpSttNd;
                if( pAktNode->IsStartNode() )
                    aTmpIdx = pAktNode->EndOfSectionIndex() + 1;
                else
                    ++aTmpIdx;
            }
            return ;
        }
        DelNodes( pRange->aStart, 1 );
    }
    else if( aIdx == pRange->aStart.GetIndex()-1 ) // before StartNode
        DelNodes( aIdx, 1 );
    else
        new SwEndNode( pRange->aStart, *aIdx.GetNode().GetStartNode() );

    // If the end of a range is before or at a StartNode, so delete it,
    // otherwise empty S/E or E/S nodes would be created.
    // For other nodes, insert a new end node.
    SwNodeIndex aTmpIdx( pRange->aEnd );
    if( pRange->aEnd.GetNode().IsEndNode() )
        DelNodes( pRange->aEnd, 1 );
    else
    {
        new SwStartNode( pRange->aEnd );
/*?? which NodeType ??*/
        aTmpIdx = *pRange->aEnd.GetNode().EndOfSectionNode();
        --pRange->aEnd;
    }

    SectionUpDown( aIdx, aTmpIdx );
}

/** correct indices after movement
 *
 * Update all indices after movement so that the levels are consistent again.
 *
 * @param aStart index of the start node
 * @param aEnd index of the end point
 *
 * @see SwNodes::SectionUp
 * @see SwNodes::SectionDown
 */
void SwNodes::SectionUpDown( const SwNodeIndex & aStart, const SwNodeIndex & aEnd )
{
    SwNodeIndex aTmpIdx( aStart, +1 );
    // array forms a stack, holding all StartOfSelections
    SwSttNdPtrs aSttNdStack;
    SwStartNode* pTmp = aStart.GetNode().GetStartNode();
    aSttNdStack.push_back( pTmp );

    // loop until the first start node that needs to be change was found
    // (the indices are updated from the end node backwards to the start)
    for( ;; ++aTmpIdx )
    {
        SwNode * pAktNode = &aTmpIdx.GetNode();
        pAktNode->pStartOfSection = aSttNdStack[ aSttNdStack.size()-1 ];

        if( pAktNode->GetStartNode() )
        {
            pTmp = static_cast<SwStartNode*>(pAktNode);
            aSttNdStack.push_back( pTmp );
        }
        else if( pAktNode->GetEndNode() )
        {
            SwStartNode* pSttNd = aSttNdStack[ aSttNdStack.size() - 1 ];
            pSttNd->pEndOfSection = static_cast<SwEndNode*>(pAktNode);
            aSttNdStack.pop_back();
            if( !aSttNdStack.empty() )
                continue; // still enough EndNodes on the stack

            else if( aTmpIdx < aEnd ) // too many StartNodes
                // if the end is not reached, yet, get the start of the section above
            {
                aSttNdStack.insert( aSttNdStack.begin(), pSttNd->pStartOfSection );
            }
            else // finished, as soon as out of the range
                break;
        }
    }
}

/** delete nodes
 *
 * This is a specific implementation of a delete function for a variable array.
 * It is necessary as there might be inconsistencies after deleting start or
 * end nodes. This method can clean those up.
 *
 * @param rIndex position to delete at (unchanged afterwards)
 * @param nNodes number of nodes to delete (default: 1)
 */
void SwNodes::Delete(const SwNodeIndex &rIndex, sal_uLong nNodes)
{
    int nLevel = 0; // level counter
    SwNode * pAktNode;

    sal_uLong nCnt = Count() - rIndex.GetIndex() - 1;
    if( nCnt > nNodes ) nCnt = nNodes;

    if( nCnt == 0 ) // no count -> return
        return;

    SwNodeRange aRg( rIndex, 0, rIndex, nCnt-1 );
    // check if [rIndex..rIndex + nCnt] is larger than the range
    if( ( !aRg.aStart.GetNode().StartOfSectionIndex() &&
            !aRg.aStart.GetIndex() ) ||
            ! CheckNodesRange( aRg.aStart, aRg.aEnd ) )
        return;

    // if aEnd is not on a ContentNode, search the previous one
    while( ( pAktNode = &aRg.aEnd.GetNode())->GetStartNode() ||
             ( pAktNode->GetEndNode() &&
                !pAktNode->pStartOfSection->IsTableNode() ))
        --aRg.aEnd;

    nCnt = 0;
//TODO: check/improve comment
    // increase start so that we are able to use "<" (using "<=" might cause
    // problems if aEnd == aStart and aEnd is deleted, so aEnd <= aStart)
    --aRg.aStart;

    bool bSaveInNodesDel = bInNodesDel;
    bInNodesDel = true;
    bool bUpdateOutline = false;

    // loop until everything is deleted
    while( aRg.aStart < aRg.aEnd )
    {
        pAktNode = &aRg.aEnd.GetNode();

        if( pAktNode->GetEndNode() )
        {
            // delete the whole section?
            if( pAktNode->StartOfSectionIndex() > aRg.aStart.GetIndex() )
            {
                SwTableNode* pTableNd = pAktNode->pStartOfSection->GetTableNode();
                if( pTableNd )
                    pTableNd->DelFrms();

                SwNode *pNd, *pChkNd = pAktNode->pStartOfSection;
                sal_uInt16 nIdxPos;
                do {
                    pNd = &aRg.aEnd.GetNode();

                    if( pNd->IsTextNode() )
                    {
                        SwTextNode *const pTextNode(pNd->GetTextNode());
                        if (pTextNode->IsOutline() &&
                                pOutlineNds->Seek_Entry( pNd, &nIdxPos ))
                        {
                            // remove outline indices
                            pOutlineNds->erase(nIdxPos);
                            bUpdateOutline = true;
                        }
                        pTextNode->InvalidateNumRule();
                    }
                    else if( pNd->IsEndNode() &&
                            pNd->pStartOfSection->IsTableNode() )
                        static_cast<SwTableNode*>(pNd->pStartOfSection)->DelFrms();

                    --aRg.aEnd;
                    nCnt++;

                } while( pNd != pChkNd );
            }
            else
            {
                RemoveNode( aRg.aEnd.GetIndex()+1, nCnt, true ); // delete
                nCnt = 0;
                --aRg.aEnd; // before the EndNode
                nLevel++;
            }
        }
        else if( pAktNode->GetStartNode() ) // found StartNode
        {
            if( nLevel == 0 ) // decrease one level
            {
                if( nCnt )
                {
                    // now delete array
                    ++aRg.aEnd;
                    RemoveNode( aRg.aEnd.GetIndex(), nCnt, true );
                    nCnt = 0;
                }
            }
            else // remove all nodes between start and end node (incl. both)
            {
                RemoveNode( aRg.aEnd.GetIndex(), nCnt + 2, true ); // delete array
                nCnt = 0;
                nLevel--;
            }

            // after deletion, aEnd might point to a EndNode...
            // delete all empty start/end node pairs
            SwNode* pTmpNode = aRg.aEnd.GetNode().GetEndNode();
            --aRg.aEnd;
            while(  pTmpNode &&
                    ( pAktNode = &aRg.aEnd.GetNode())->GetStartNode() &&
                    pAktNode->StartOfSectionIndex() )
            {
                // remove end and start node
                DelNodes( aRg.aEnd, 2 );
                pTmpNode = aRg.aEnd.GetNode().GetEndNode();
                --aRg.aEnd;
            }
        }
        else // "normal" node, so insert into TmpArray
        {
            SwTextNode* pTextNd = pAktNode->GetTextNode();
            if( pTextNd )
            {
                if( pTextNd->IsOutline())
                {
                    // delete outline indices
                    pOutlineNds->erase( pTextNd );
                    bUpdateOutline = true;
                }
                pTextNd->InvalidateNumRule();
            }
            else if( pAktNode->IsContentNode() )
                static_cast<SwContentNode*>(pAktNode)->InvalidateNumRule();

            --aRg.aEnd;
            nCnt++;
        }
    }

    ++aRg.aEnd;
    if( nCnt != 0 )
        RemoveNode( aRg.aEnd.GetIndex(), nCnt, true ); // delete the rest

    // delete all empty start/end node pairs
    while( aRg.aEnd.GetNode().GetEndNode() &&
            ( pAktNode = &aRg.aStart.GetNode())->GetStartNode() &&
            pAktNode->StartOfSectionIndex() )
    // but none of the holy 5. (???)
    {
        DelNodes( aRg.aStart, 2 );  // delete start and end node
        --aRg.aStart;
    }

    bInNodesDel = bSaveInNodesDel;

    if( !bInNodesDel )
    {
        // update numbering
        if( bUpdateOutline || bInDelUpdOutl )
        {
            UpdateOutlineIdx( aRg.aEnd.GetNode() );
            bInDelUpdOutl = false;
        }

    }
    else
    {
        if( bUpdateOutline )
            bInDelUpdOutl = true;
    }
}

/** get section level at the given position
 *
 * @note The first node in an array should always be a start node.
 *       Because of this, there is a special treatment here based on the
 *       assumption that this is true in this context as well.
 *
 * @param rIdx position of the node
 * @return section level at the given position
 */
sal_uInt16 SwNodes::GetSectionLevel(const SwNodeIndex &rIdx)
{
    // special treatment for 1st Node
    if(rIdx == 0) return 1;
    // no recursion! This calles a SwNode::GetSectionLevel (missing "s")
    return rIdx.GetNode().GetSectionLevel();
}

void SwNodes::GoStartOfSection(SwNodeIndex *pIdx)
{
    // after the next start node
    SwNodeIndex aTmp( *pIdx->GetNode().StartOfSectionNode(), +1 );

    // If index points to no ContentNode, than go to one.
    // If there is no further available, do not change the index' position!
    while( !aTmp.GetNode().IsContentNode() )
    {   // go from this StartNode (can only be one) to its end
        if( *pIdx <= aTmp )
            return;     // ERROR: already after the section
        aTmp = aTmp.GetNode().EndOfSectionIndex()+1;
        if( *pIdx <= aTmp )
            return;     // ERROR: already after the section
    }
    (*pIdx) = aTmp;     // is on a ContentNode
}

void SwNodes::GoEndOfSection(SwNodeIndex *pIdx)
{
    if( !pIdx->GetNode().IsEndNode() )
        (*pIdx) = *pIdx->GetNode().EndOfSectionNode();
}

SwContentNode* SwNodes::GoNext(SwNodeIndex *pIdx) const
{
    if( pIdx->GetIndex() >= Count() - 1 )
        return 0;

    SwNodeIndex aTmp(*pIdx, +1);
    SwNode* pNd = 0;
    while( aTmp < Count()-1 && !( pNd = &aTmp.GetNode())->IsContentNode() )
        ++aTmp;

    if( aTmp == Count()-1 )
        pNd = 0;
    else
        (*pIdx) = aTmp;
    return static_cast<SwContentNode*>(pNd);
}

SwContentNode* SwNodes::GoPrevious(SwNodeIndex *pIdx)
{
    if( !pIdx->GetIndex() )
        return 0;

    SwNodeIndex aTmp( *pIdx, -1 );
    SwNode* pNd = 0;
    while( aTmp.GetIndex() && !( pNd = &aTmp.GetNode())->IsContentNode() )
        --aTmp;

    if( !aTmp.GetIndex() )
        pNd = 0;
    else
        (*pIdx) = aTmp;
    return static_cast<SwContentNode*>(pNd);
}

inline bool TstIdx( sal_uLong nSttIdx, sal_uLong nEndIdx, sal_uLong nStt, sal_uLong nEnd )
{
    return nStt < nSttIdx && nEnd >= nSttIdx &&
            nStt < nEndIdx && nEnd >= nEndIdx;
}

/** Check if the given range is inside the defined ranges
 *
 * The defined ranges are Content, AutoText, PostIts, Inserts, and Redlines.
 *
 * @param rStt start index of the range
 * @param rEnd end index of the range
 * @return <true> if valid range
 */
bool SwNodes::CheckNodesRange( const SwNodeIndex& rStt, const SwNodeIndex& rEnd ) const
{
    sal_uLong nStt = rStt.GetIndex(), nEnd = rEnd.GetIndex();
    if( TstIdx( nStt, nEnd, pEndOfContent->StartOfSectionIndex(),
                pEndOfContent->GetIndex() )) return true;
    if( TstIdx( nStt, nEnd, pEndOfAutotext->StartOfSectionIndex(),
                pEndOfAutotext->GetIndex() )) return true;
    if( TstIdx( nStt, nEnd, pEndOfPostIts->StartOfSectionIndex(),
                pEndOfPostIts->GetIndex() )) return true;
    if( TstIdx( nStt, nEnd, pEndOfInserts->StartOfSectionIndex(),
                pEndOfInserts->GetIndex() )) return true;
    if( TstIdx( nStt, nEnd, pEndOfRedlines->StartOfSectionIndex(),
                pEndOfRedlines->GetIndex() )) return true;

    return false;       // is somewhere in the middle, ERROR
}

/** Delete a number of nodes
 *
 * @param rStart starting position in this nodes array
 * @param nCnt number of nodes to delete
 */
void SwNodes::DelNodes( const SwNodeIndex & rStart, sal_uLong nCnt )
{
    sal_uLong nSttIdx = rStart.GetIndex();

    if( !nSttIdx && nCnt == GetEndOfContent().GetIndex()+1 )
    {
        // The whole nodes array will be destroyed, you're in the Doc's DTOR!
        // The initial start/end nodes should be only destroyed in the SwNodes' DTOR!
        SwNode* aEndNdArr[] = { pEndOfContent,
                                pEndOfPostIts, pEndOfInserts,
                                pEndOfAutotext, pEndOfRedlines,
                                0
                              };

        SwNode** ppEndNdArr = aEndNdArr;
        while( *ppEndNdArr )
        {
            nSttIdx = (*ppEndNdArr)->StartOfSectionIndex() + 1;
            sal_uLong nEndIdx = (*ppEndNdArr)->GetIndex();

            if( nSttIdx != nEndIdx )
                RemoveNode( nSttIdx, nEndIdx - nSttIdx, true );

            ++ppEndNdArr;
        }
    }
    else
    {
        int bUpdateNum = 0;
        for( sal_uLong n = nSttIdx, nEnd = nSttIdx + nCnt; n < nEnd; ++n )
        {
            SwNode* pNd = (*this)[ n ];

            if (pNd->IsTextNode() && pNd->GetTextNode()->IsOutline())
            {
                // remove the outline indices
                sal_uInt16 nIdxPos;
                if( pOutlineNds->Seek_Entry( pNd, &nIdxPos ))
                {
                    pOutlineNds->erase(nIdxPos);
                    bUpdateNum = 1;
                }
            }
            if( pNd->IsContentNode() )
            {
                static_cast<SwContentNode*>(pNd)->InvalidateNumRule();
                static_cast<SwContentNode*>(pNd)->DelFrms();
            }
        }
        RemoveNode( nSttIdx, nCnt, true );

        // update numbering
        if( bUpdateNum )
            UpdateOutlineIdx( rStart.GetNode() );
    }
}

struct HighLevel
{
    sal_uInt16 nLevel, nTop;
    explicit HighLevel( sal_uInt16 nLv ) : nLevel( nLv ), nTop( nLv ) {}
};

static bool lcl_HighestLevel( const SwNodePtr& rpNode, void * pPara )
{
    HighLevel * pHL = static_cast<HighLevel*>(pPara);
    if( rpNode->GetStartNode() )
        pHL->nLevel++;
    else if( rpNode->GetEndNode() )
        pHL->nLevel--;
    if( pHL->nTop > pHL->nLevel )
        pHL->nTop = pHL->nLevel;
    return true;

}

/** Calculate the highest level in a range
 *
 * @param rNodes the nodes array
 * @param rRange the range to inspect
 * @return the highest level
 */
sal_uInt16 HighestLevel( SwNodes & rNodes, const SwNodeRange & rRange )
{
    HighLevel aPara( SwNodes::GetSectionLevel( rRange.aStart ));
    rNodes.ForEach( rRange.aStart, rRange.aEnd, lcl_HighestLevel, &aPara );
    return aPara.nTop;

}

/** move a range
 *
 * @param rPam the range to move
 * @param rPos to destination position in the given nodes array
 * @param rNodes the node array to move the range into
 */
void SwNodes::MoveRange( SwPaM & rPam, SwPosition & rPos, SwNodes& rNodes )
{
    SwPosition * const pStt = rPam.Start();
    SwPosition * const pEnd = rPam.End();

    if( !rPam.HasMark() || *pStt >= *pEnd )
        return;

    if( this == &rNodes && *pStt <= rPos && rPos < *pEnd )
        return;

    SwNodeIndex aEndIdx( pEnd->nNode );
    SwNodeIndex aSttIdx( pStt->nNode );
    SwTextNode *const pSrcNd = aSttIdx.GetNode().GetTextNode();
    SwTextNode * pDestNd = rPos.nNode.GetNode().GetTextNode();
    bool bSplitDestNd = true;
    bool bCopyCollFormat = pDestNd && pDestNd->GetText().isEmpty();

    if( pSrcNd )
    {
        // if the first node is a TextNode, than there must
        // be also a TextNode in the NodesArray to store the content
        if( !pDestNd )
        {
            pDestNd = rNodes.MakeTextNode( rPos.nNode, pSrcNd->GetTextColl() );
            --rPos.nNode;
            rPos.nContent.Assign( pDestNd, 0 );
            bCopyCollFormat = true;
        }
        bSplitDestNd = pDestNd->Len() > rPos.nContent.GetIndex() ||
                        pEnd->nNode.GetNode().IsTextNode();

        // move the content into the new node
        bool bOneNd = pStt->nNode == pEnd->nNode;
        const sal_Int32 nLen =
                ( (bOneNd) ? pEnd->nContent.GetIndex() : pSrcNd->Len() )
                - pStt->nContent.GetIndex();

        if( !pEnd->nNode.GetNode().IsContentNode() )
        {
            bOneNd = true;
            sal_uLong nSttNdIdx = pStt->nNode.GetIndex() + 1;
            const sal_uLong nEndNdIdx = pEnd->nNode.GetIndex();
            for( ; nSttNdIdx < nEndNdIdx; ++nSttNdIdx )
            {
                if( (*this)[ nSttNdIdx ]->IsContentNode() )
                {
                    bOneNd = false;
                    break;
                }
            }
        }

        // templates must be copied/set after a split
        if( !bOneNd && bSplitDestNd )
        {
            if( !rPos.nContent.GetIndex() )
            {
                bCopyCollFormat = true;
            }
            if( rNodes.IsDocNodes() )
            {
                SwDoc* const pInsDoc = pDestNd->GetDoc();
                ::sw::UndoGuard const ug(pInsDoc->GetIDocumentUndoRedo());
                pInsDoc->getIDocumentContentOperations().SplitNode( rPos, false );
            }
            else
            {
                pDestNd->SplitContentNode( rPos );
            }

            if( rPos.nNode == aEndIdx )
            {
                --aEndIdx;
            }
            bSplitDestNd = true;

            pDestNd = rNodes[ rPos.nNode.GetIndex() - 1 ]->GetTextNode();
            if( nLen )
            {
                pSrcNd->CutText( pDestNd, SwIndex( pDestNd, pDestNd->Len()),
                            pStt->nContent, nLen );
            }
        }
        else if ( nLen )
        {
            pSrcNd->CutText( pDestNd, rPos.nContent, pStt->nContent, nLen );
        }

        if( bCopyCollFormat )
        {
            SwDoc* const pInsDoc = pDestNd->GetDoc();
            ::sw::UndoGuard const undoGuard(pInsDoc->GetIDocumentUndoRedo());
            pSrcNd->CopyCollFormat( *pDestNd );
            bCopyCollFormat = false;
        }

        if( bOneNd )
        {
            // Correct the PaM, because it might have happened that the move
            // went over the node borders (so the data might be in different nodes).
            // Also, a selection is invalidated.
            pEnd->nContent = pStt->nContent;
            rPam.DeleteMark();
            GetDoc()->GetDocShell()->Broadcast( SwFormatFieldHint( 0,
                rNodes.IsDocNodes() ? SwFormatFieldHintWhich::INSERTED : SwFormatFieldHintWhich::REMOVED ) );
            return;
        }

        ++aSttIdx;
    }
    else if( pDestNd )
    {
        if( rPos.nContent.GetIndex() )
        {
            if( rPos.nContent.GetIndex() == pDestNd->Len() )
            {
                ++rPos.nNode;
            }
            else if( rPos.nContent.GetIndex() )
            {
                // if the EndNode is split than correct the EndIdx
                const bool bCorrEnd = aEndIdx == rPos.nNode;

                // if no text is attached to the TextNode, split it
                if( rNodes.IsDocNodes() )
                {
                    SwDoc* const pInsDoc = pDestNd->GetDoc();
                    ::sw::UndoGuard const ug(pInsDoc->GetIDocumentUndoRedo());
                    pInsDoc->getIDocumentContentOperations().SplitNode( rPos, false );
                }
                else
                {
                    pDestNd->SplitContentNode( rPos );
                }

                if ( bCorrEnd )
                {
                    --aEndIdx;
                }
            }
        }
        // at the end only an empty TextNode is left over
        bSplitDestNd = true;
    }

    SwTextNode* const pEndSrcNd = aEndIdx.GetNode().GetTextNode();
    if ( pEndSrcNd )
    {
        {
            // at the end of this range a new TextNode will be created
            if( !bSplitDestNd )
            {
                if( rPos.nNode < rNodes.GetEndOfContent().GetIndex() )
                {
                    ++rPos.nNode;
                }

                pDestNd =
                    rNodes.MakeTextNode( rPos.nNode, pEndSrcNd->GetTextColl() );
                --rPos.nNode;
                rPos.nContent.Assign( pDestNd, 0 );
            }
            else
            {
                pDestNd = rPos.nNode.GetNode().GetTextNode();
            }

            if (pDestNd && pEnd->nContent.GetIndex())
            {
                // move the content into the new node
                SwIndex aIdx( pEndSrcNd, 0 );
                pEndSrcNd->CutText( pDestNd, rPos.nContent, aIdx,
                                pEnd->nContent.GetIndex());
            }

            if (pDestNd && bCopyCollFormat)
            {
                SwDoc* const pInsDoc = pDestNd->GetDoc();
                ::sw::UndoGuard const ug(pInsDoc->GetIDocumentUndoRedo());
                pEndSrcNd->CopyCollFormat( *pDestNd );
            }
        }
    }
    else
    {
        if ( pSrcNd && aEndIdx.GetNode().IsContentNode() )
        {
            ++aEndIdx;
        }
        if( !bSplitDestNd )
        {
            ++rPos.nNode;
            rPos.nContent.Assign( rPos.nNode.GetNode().GetContentNode(), 0 );
        }
    }

    if( aEndIdx != aSttIdx )
    {
        // move the nodes into the NodesArary
        const sal_uLong nSttDiff = aSttIdx.GetIndex() - pStt->nNode.GetIndex();
        SwNodeRange aRg( aSttIdx, aEndIdx );
        _MoveNodes( aRg, rNodes, rPos.nNode );

        // if in the same node array, all indices are now at new positions (so correct them)
        if( &rNodes == this )
        {
            pStt->nNode = aRg.aEnd.GetIndex() - nSttDiff;
        }
    }

    // if the StartNode was moved to whom the cursor pointed, so
    // the content must be registered in the current content!
    if ( &pStt->nNode.GetNode() == &GetEndOfContent() )
    {
        const bool bSuccess = GoPrevious( &pStt->nNode );
        OSL_ENSURE( bSuccess, "Move() - no ContentNode here" );
        (void) bSuccess;
    }
    pStt->nContent.Assign( pStt->nNode.GetNode().GetContentNode(),
                            pStt->nContent.GetIndex() );
    // Correct the PaM, because it might have happened that the move
    // went over the node borders (so the data might be in different nodes).
    // Also, a selection is invalidated.
    *pEnd = *pStt;
    rPam.DeleteMark();
    GetDoc()->GetDocShell()->Broadcast( SwFormatFieldHint( 0,
                rNodes.IsDocNodes() ? SwFormatFieldHintWhich::INSERTED : SwFormatFieldHintWhich::REMOVED ) );
}

///@see SwNodes::_MoveNodes (TODO: seems to be C&P programming here)
void SwNodes::_CopyNodes( const SwNodeRange& rRange,
            const SwNodeIndex& rIndex, bool bNewFrms, bool bTableInsDummyNode ) const
{
    SwDoc* pDoc = rIndex.GetNode().GetDoc();

    SwNode * pAktNode;
    if( rIndex == 0 ||
        ( (pAktNode = &rIndex.GetNode())->GetStartNode() &&
          !pAktNode->StartOfSectionIndex() ))
        return;

    SwNodeRange aRg( rRange );

    // skip "simple" StartNodes or EndNodes
    while( ND_STARTNODE == (pAktNode = & aRg.aStart.GetNode())->GetNodeType()
            || ( pAktNode->IsEndNode() &&
                !pAktNode->pStartOfSection->IsSectionNode() ) )
        ++aRg.aStart;

    // if aEnd-1 points to no ContentNode, search previous one
    --aRg.aEnd;
    // #i107142#: if aEnd is start node of a special section, do nothing.
    // Otherwise this could lead to crash: going through all previous
    // special section nodes and then one before the first.
    if (aRg.aEnd.GetNode().StartOfSectionIndex() != 0)
    {
        while( ((pAktNode = & aRg.aEnd.GetNode())->GetStartNode() &&
                !pAktNode->IsSectionNode() ) ||
                ( pAktNode->IsEndNode() &&
                ND_STARTNODE == pAktNode->pStartOfSection->GetNodeType()) )
        {
            --aRg.aEnd;
        }
    }
    ++aRg.aEnd;

    // if in same array, check insertion position
    if( aRg.aStart >= aRg.aEnd )
        return;

    // when inserting into the source range, nothing need to be done
    OSL_ENSURE( &aRg.aStart.GetNodes() == this,
                "aRg should use thisnodes array" );
    OSL_ENSURE( &aRg.aStart.GetNodes() == &aRg.aEnd.GetNodes(),
               "Range across different nodes arrays? You deserve punishment!");
    if( &rIndex.GetNodes() == &aRg.aStart.GetNodes() &&
        rIndex.GetIndex() >= aRg.aStart.GetIndex() &&
        rIndex.GetIndex() < aRg.aEnd.GetIndex() )
            return;

    SwNodeIndex aInsPos( rIndex );
    SwNodeIndex aOrigInsPos( rIndex, -1 ); // original insertion position
    int nLevel = 0;                        // level counter

    for( sal_uLong nNodeCnt = aRg.aEnd.GetIndex() - aRg.aStart.GetIndex();
            nNodeCnt > 0; --nNodeCnt )
    {
        pAktNode = &aRg.aStart.GetNode();
        switch( pAktNode->GetNodeType() )
        {
        case ND_TABLENODE:
            // Does it copy a table in(to) a footnote?
            if( aInsPos < pDoc->GetNodes().GetEndOfInserts().GetIndex() &&
                    pDoc->GetNodes().GetEndOfInserts().StartOfSectionIndex()
                    < aInsPos.GetIndex() )
            {
                sal_uLong nDistance =
                    ( pAktNode->EndOfSectionIndex() -
                        aRg.aStart.GetIndex() );
                if (nDistance < nNodeCnt)
                    nNodeCnt -= nDistance;
                else
                    nNodeCnt = 1;

                // insert a DummyNode for a TableNode
                if( bTableInsDummyNode )
                    new SwPlaceholderNode(aInsPos);

                // copy all of the table's nodes into the current cell
                for( ++aRg.aStart; aRg.aStart.GetIndex() <
                    pAktNode->EndOfSectionIndex();
                    ++aRg.aStart )
                {
                    // insert a DummyNode for the box-StartNode?
                    if( bTableInsDummyNode )
                        new SwPlaceholderNode(aInsPos);

                    SwStartNode* pSttNd = aRg.aStart.GetNode().GetStartNode();
                    _CopyNodes( SwNodeRange( *pSttNd, + 1,
                                            *pSttNd->EndOfSectionNode() ),
                                aInsPos, bNewFrms, false );

                    // insert a DummyNode for the box-EndNode?
                    if( bTableInsDummyNode )
                        new SwPlaceholderNode(aInsPos);
                    aRg.aStart = *pSttNd->EndOfSectionNode();
                }
                // insert a DummyNode for the table-EndNode
                if( bTableInsDummyNode )
                    new SwPlaceholderNode(aInsPos);
                aRg.aStart = *pAktNode->EndOfSectionNode();
            }
            else
            {
                SwNodeIndex nStt( aInsPos, -1 );
                SwTableNode* pTableNd = static_cast<SwTableNode*>(pAktNode)->
                                        MakeCopy( pDoc, aInsPos );
                sal_uLong nDistance = aInsPos.GetIndex() - nStt.GetIndex() - 2;
                if (nDistance < nNodeCnt)
                    nNodeCnt -= nDistance;
                else
                    nNodeCnt = 1;

                aRg.aStart = pAktNode->EndOfSectionIndex();

                if( bNewFrms && pTableNd )
                {
                    nStt = aInsPos;
                    pTableNd->MakeFrms( &nStt );
                }
            }
            break;

        case ND_SECTIONNODE:
            // If the end of the section is outside the copy range,
            // the section node will skipped, not copied!
            // If someone want to change this behaviour, he has to adjust the function
            // lcl_NonCopyCount(..) in ndcopy.cxx which relies on it.
            if( pAktNode->EndOfSectionIndex() < aRg.aEnd.GetIndex() )
            {
                // copy of the whole section, so create a new SectionNode
                SwNodeIndex nStt( aInsPos, -1 );
                SwSectionNode* pSectNd = static_cast<SwSectionNode*>(pAktNode)->
                                    MakeCopy( pDoc, aInsPos );

                sal_uLong nDistance = aInsPos.GetIndex() - nStt.GetIndex() - 2;
                if (nDistance < nNodeCnt)
                    nNodeCnt -= nDistance;
                else
                    nNodeCnt = 1;
                aRg.aStart = pAktNode->EndOfSectionIndex();

                if( bNewFrms && pSectNd &&
                    !pSectNd->GetSection().IsHidden() )
                    pSectNd->MakeFrms( &nStt );
            }
            break;

        case ND_STARTNODE:
            {
                SwStartNode* pTmp = new SwStartNode( aInsPos, ND_STARTNODE,
                            static_cast<SwStartNode*>(pAktNode)->GetStartNodeType() );
                new SwEndNode( aInsPos, *pTmp );
                --aInsPos;
                nLevel++;
            }
            break;

        case ND_ENDNODE:
            if( nLevel ) // complete section
            {
                --nLevel;
                ++aInsPos; // EndNode already exists
            }
            else if( !pAktNode->pStartOfSection->IsSectionNode() )
            {
                // create a section at the original InsertPosition
                SwNodeRange aTmpRg( aOrigInsPos, 1, aInsPos );
                pDoc->GetNodes().SectionDown( &aTmpRg,
                        pAktNode->pStartOfSection->GetStartNodeType() );
            }
            break;

        case ND_TEXTNODE:
        case ND_GRFNODE:
        case ND_OLENODE:
            {
                SwContentNode* pNew = static_cast<SwContentNode*>(pAktNode)->MakeCopy(
                                            pDoc, aInsPos );
                // frames are always created as default, so delete if needed
                if( !bNewFrms )
                    pNew->DelFrms();
            }
            break;

        case ND_PLACEHOLDER:
            if (GetDoc()->GetIDocumentUndoRedo().IsUndoNodes(*this))
            {
                // than a SectionNode (start/end) is needed at the current
                // InsPos; if so skip it, otherwise ignore current node
                SwNode *const pTmpNd = & aInsPos.GetNode();
                if( pTmpNd->IsSectionNode() ||
                    pTmpNd->StartOfSectionNode()->IsSectionNode() )
                    ++aInsPos;  // skip
            }
            else {
                OSL_FAIL( "How can this node be in the node array?" );
            }
            break;

        default:
            OSL_FAIL( "Unknown node type" );
        }
        ++aRg.aStart;
    }
}

void SwNodes::_DelDummyNodes( const SwNodeRange& rRg )
{
    SwNodeIndex aIdx( rRg.aStart );
    while( aIdx.GetIndex() < rRg.aEnd.GetIndex() )
    {
        if (ND_PLACEHOLDER == aIdx.GetNode().GetNodeType())
            RemoveNode( aIdx.GetIndex(), 1, true );
        else
            ++aIdx;
    }
}

SwStartNode* SwNodes::MakeEmptySection( const SwNodeIndex& rIdx,
                                        SwStartNodeType eSttNdTyp )
{
    SwStartNode* pSttNd = new SwStartNode( rIdx, ND_STARTNODE, eSttNdTyp );
    new SwEndNode( rIdx, *pSttNd );
    return pSttNd;
}

SwStartNode* SwNodes::MakeTextSection( const SwNodeIndex & rWhere,
                                        SwStartNodeType eSttNdTyp,
                                        SwTextFormatColl *pColl,
                                        SwAttrSet* pAutoAttr )
{
    SwStartNode* pSttNd = new SwStartNode( rWhere, ND_STARTNODE, eSttNdTyp );
    new SwEndNode( rWhere, *pSttNd );
    MakeTextNode( SwNodeIndex( rWhere, - 1 ), pColl, pAutoAttr );
    return pSttNd;
}

//TODO: provide better documentation
/** go to next section that is not protected nor hidden
 *
 * @note if !bSkipHidden and !bSkipProtect, use GoNext/GoPrevious
 *
 * @param pIdx
 * @param bSkipHidden
 * @param bSkipProtect
 * @return
 * @see SwNodes::GoNext
 * @see SwNodes::GoPrevious
 * @see SwNodes::GoNextSection (TODO: seems to be C&P programming here)
*/
SwContentNode* SwNodes::GoNextSection( SwNodeIndex * pIdx,
                            bool bSkipHidden, bool bSkipProtect ) const
{
    bool bFirst = true;
    SwNodeIndex aTmp( *pIdx );
    const SwNode* pNd;
    while( aTmp < Count() - 1 )
    {
        pNd = & aTmp.GetNode();
        if (ND_SECTIONNODE == pNd->GetNodeType())
        {
            const SwSection& rSect = static_cast<const SwSectionNode*>(pNd)->GetSection();
            if( (bSkipHidden && rSect.IsHiddenFlag()) ||
                (bSkipProtect && rSect.IsProtectFlag()) )
                // than skip the section
                aTmp = *pNd->EndOfSectionNode();
        }
        else if( bFirst )
        {
            if( pNd->pStartOfSection->IsSectionNode() )
            {
                const SwSection& rSect = static_cast<SwSectionNode*>(pNd->
                                pStartOfSection)->GetSection();
                if( (bSkipHidden && rSect.IsHiddenFlag()) ||
                    (bSkipProtect && rSect.IsProtectFlag()) )
                    // than skip the section
                    aTmp = *pNd->EndOfSectionNode();
            }
        }
        else if( ND_CONTENTNODE & pNd->GetNodeType() )
        {
            const SwSectionNode* pSectNd;
            if( ( bSkipHidden || bSkipProtect ) &&
                0 != (pSectNd = pNd->FindSectionNode() ) &&
                ( ( bSkipHidden && pSectNd->GetSection().IsHiddenFlag() ) ||
                  ( bSkipProtect && pSectNd->GetSection().IsProtectFlag() )) )
            {
                aTmp = *pSectNd->EndOfSectionNode();
            }
            else
            {
                (*pIdx) = aTmp;
                return const_cast<SwContentNode*>(static_cast<const SwContentNode*>(pNd));
            }
        }
        ++aTmp;
        bFirst = false;
    }
    return 0;
}

///@see SwNodes::GoNextSection (TODO: seems to be C&P programming here)
SwContentNode* SwNodes::GoPrevSection( SwNodeIndex * pIdx,
                            bool bSkipHidden, bool bSkipProtect )
{
    bool bFirst = true;
    SwNodeIndex aTmp( *pIdx );
    const SwNode* pNd;
    while( aTmp > 0 )
    {
        pNd = & aTmp.GetNode();
        if (ND_ENDNODE == pNd->GetNodeType())
        {
            if( pNd->pStartOfSection->IsSectionNode() )
            {
                const SwSection& rSect = static_cast<SwSectionNode*>(pNd->
                                            pStartOfSection)->GetSection();
                if( (bSkipHidden && rSect.IsHiddenFlag()) ||
                    (bSkipProtect && rSect.IsProtectFlag()) )
                    // than skip section
                    aTmp = *pNd->StartOfSectionNode();
            }
            bFirst = false;
        }
        else if( bFirst )
        {
            bFirst = false;
            if( pNd->pStartOfSection->IsSectionNode() )
            {
                const SwSection& rSect = static_cast<SwSectionNode*>(pNd->
                                pStartOfSection)->GetSection();
                if( (bSkipHidden && rSect.IsHiddenFlag()) ||
                    (bSkipProtect && rSect.IsProtectFlag()) )
                    // than skip section
                    aTmp = *pNd->StartOfSectionNode();
            }
        }
        else if( ND_CONTENTNODE & pNd->GetNodeType() )
        {
            const SwSectionNode* pSectNd;
            if( ( bSkipHidden || bSkipProtect ) &&
                0 != (pSectNd = pNd->FindSectionNode() ) &&
                ( ( bSkipHidden && pSectNd->GetSection().IsHiddenFlag() ) ||
                  ( bSkipProtect && pSectNd->GetSection().IsProtectFlag() )) )
            {
                aTmp = *pSectNd;
            }
            else
            {
                (*pIdx) = aTmp;
                return const_cast<SwContentNode*>(static_cast<const SwContentNode*>(pNd));
            }
        }
        --aTmp;
    }
    return 0;
}

//TODO: improve documentation
//TODO: The inventor of the "single responsibility principle" will be crying if you ever show this code to him!
/** find the next/previous ContentNode or a table node with frames
 *
 * If no pEnd is given, search is started with FrameIndex; otherwise
 * search is started with the one before rFrmIdx and after pEnd.
 *
 * @param rFrmIdx node with frames to search in
 * @param pEnd ???
 * @return result node; 0 (!!!) if not found
 */
SwNode* SwNodes::FindPrvNxtFrmNode( SwNodeIndex& rFrmIdx,
                                    const SwNode* pEnd ) const
{
    SwNode* pFrmNd = 0;

    // no layout -> skip
    if( GetDoc()->getIDocumentLayoutAccess().GetCurrentViewShell() )
    {
        SwNode* pSttNd = &rFrmIdx.GetNode();

        // move of a hidden section?
        SwSectionNode* pSectNd = pSttNd->IsSectionNode()
                    ? pSttNd->StartOfSectionNode()->FindSectionNode()
                    : pSttNd->FindSectionNode();
        if( !( pSectNd && pSectNd->GetSection().CalcHiddenFlag() ) )
        {
            // in a table in table situation we have to assure that we don't leave the
            // outer table cell when the inner table is looking for a PrvNxt...
            SwTableNode* pTableNd = pSttNd->IsTableNode()
                    ? pSttNd->StartOfSectionNode()->FindTableNode()
                    : pSttNd->FindTableNode();
            SwNodeIndex aIdx( rFrmIdx );
            SwNode* pNd;
            if( pEnd )
            {
                --aIdx;
                pNd = &aIdx.GetNode();
            }
            else
                pNd = pSttNd;

            if( ( pFrmNd = pNd )->IsContentNode() )
                rFrmIdx = aIdx;

            // search forward or backward for a content node
            else if( 0 != ( pFrmNd = GoPrevSection( &aIdx, true, false )) &&
                    ::CheckNodesRange( aIdx, rFrmIdx, true ) &&
                    // Never out of the table at the start
                    pFrmNd->FindTableNode() == pTableNd &&
                    // Bug 37652: Never out of the table at the end
                    (!pFrmNd->FindTableNode() || pFrmNd->FindTableBoxStartNode()
                        == pSttNd->FindTableBoxStartNode() ) &&
                     (!pSectNd || pSttNd->IsSectionNode() ||
                      pSectNd->GetIndex() < pFrmNd->GetIndex())
                    )
            {
                rFrmIdx = aIdx;
            }
            else
            {
                if( pEnd )
                    aIdx = pEnd->GetIndex() + 1;
                else
                    aIdx = rFrmIdx;

                // NEVER leave the section when doing this!
                if( ( pEnd && ( pFrmNd = &aIdx.GetNode())->IsContentNode() ) ||
                    ( 0 != ( pFrmNd = GoNextSection( &aIdx, true, false )) &&
                    ::CheckNodesRange( aIdx, rFrmIdx, true ) &&
                    ( pFrmNd->FindTableNode() == pTableNd &&
                        // NEVER go out of the table cell at the end
                        (!pFrmNd->FindTableNode() || pFrmNd->FindTableBoxStartNode()
                        == pSttNd->FindTableBoxStartNode() ) ) &&
                     (!pSectNd || pSttNd->IsSectionNode() ||
                      pSectNd->EndOfSectionIndex() > pFrmNd->GetIndex())
                    ))
                {
                    // Undo when merging a table with one before, if there is also one after it.
                    // However, if the node is in a table, it needs to be returned if the
                    // SttNode is a section or a table!
                    SwTableNode* pTableNode;
                    if (pSttNd->IsTableNode() &&
                        0 != (pTableNode = pFrmNd->FindTableNode()) &&
                        // TABLE IN TABLE:
                        pTableNode != pSttNd->StartOfSectionNode()->FindTableNode())
                    {
                        pFrmNd = pTableNode;
                        rFrmIdx = *pFrmNd;
                    }
                    else
                        rFrmIdx = aIdx;
                }
                else if( pNd->IsEndNode() && pNd->StartOfSectionNode()->IsTableNode() )
                {
                    pFrmNd = pNd->StartOfSectionNode();
                    rFrmIdx = *pFrmNd;
                }
                else
                {
                    if( pEnd )
                        aIdx = pEnd->GetIndex() + 1;
                    else
                        aIdx = rFrmIdx.GetIndex() + 1;

                    if( (pFrmNd = &aIdx.GetNode())->IsTableNode() )
                        rFrmIdx = aIdx;
                    else
                    {
                        pFrmNd = 0;

                        // is there some sectionnodes before a tablenode?
                        while( aIdx.GetNode().IsSectionNode() )
                        {
                            const SwSection& rSect = aIdx.GetNode().
                                GetSectionNode()->GetSection();
                            if( rSect.IsHiddenFlag() )
                                aIdx = aIdx.GetNode().EndOfSectionIndex()+1;
                            else
                                ++aIdx;
                        }
                        if( aIdx.GetNode().IsTableNode() )
                        {
                            rFrmIdx = aIdx;
                            pFrmNd = &aIdx.GetNode();
                        }
                    }
                }
            }
        }
    }
    return pFrmNd;
}

void SwNodes::ForEach( sal_uLong nStart, sal_uLong nEnd,
                       FnForEach_SwNodes fn, void* pArgs )
{
    if( nEnd > nSize )
        nEnd = nSize;

    if( nStart < nEnd )
    {
        sal_uInt16 cur = Index2Block( nStart );
        BlockInfo** pp = ppInf + cur;
        BlockInfo* p = *pp;
        sal_uInt16 nElem = sal_uInt16( nStart - p->nStart );
        ElementPtr* pElem = p->pData + nElem;
        nElem = p->nElem - nElem;
        for(;;)
        {
            if( !(*fn)( static_cast<SwNode *>(*pElem++), pArgs ) || ++nStart >= nEnd )
                break;

            // next element
            if( !--nElem )
            {
                // new block
                p = *++pp;
                pElem = p->pData;
                nElem = p->nElem;
            }
        }
    }
}

void SwNodes::ForEach( const SwNodeIndex& rStart, const SwNodeIndex& rEnd,
                    FnForEach_SwNodes fnForEach, void* pArgs )
{
    ForEach( rStart.GetIndex(), rEnd.GetIndex(), fnForEach, pArgs );
}

void SwNodes::RemoveNode( sal_uLong nDelPos, sal_uLong nSz, bool bDel )
{
    {
#ifndef NDEBUG
        SwNode *const pFirst((*this)[nDelPos]);
#endif
        for (sal_uLong nCnt = 0; nCnt < nSz; nCnt++)
        {
            SwNode* pNode = ((*this)[ nDelPos + nCnt ]);
            SwTextNode * pTextNd = pNode->GetTextNode();

            if (pTextNd)
            {
                pTextNd->RemoveFromList();
                // remove FLY_AS_CHAR *before* adjusting SwNodeIndex
                // so their anchor still points to correct node when deleted!
                // NOTE: this will call RemoveNode() recursively!
                // so adjust our indexes to account for removed nodes
                sal_uLong const nPos = pTextNd->GetIndex();
                SwpHints *const pHints(pTextNd->GetpSwpHints());
                if (pHints)
                {
                    std::vector<SwTextAttr*> flys;
                    for (size_t i = 0; i < pHints->Count(); ++i)
                    {
                        SwTextAttr *const pHint(pHints->GetTextHint(i));
                        if (RES_TXTATR_FLYCNT == pHint->Which())
                        {
                            flys.push_back(pHint);
                        }
                    }
                    for (SwTextAttr * pHint : flys)
                    {
                        pTextNd->DeleteAttribute(pHint);
                    }   // pHints may be dead now
                    sal_uLong const nDiff = nPos - pTextNd->GetIndex();
                    if (nDiff)
                    {
                        nDelPos -= nDiff;
                    }
                    assert(pTextNd == (*this)[nDelPos + nCnt]);
                    assert(pFirst == (*this)[nDelPos]);
                }
            }
            SwTableNode* pTableNode = pNode->GetTableNode();
            if (pTableNode)
            {
                // The node that is deleted is a table node.
                // Need to make sure that all the redlines that are
                // related to this table are removed from the
                // 'Extra Redlines' array
                pTableNode->RemoveRedlines();
            }
        }
    }

    sal_uLong nEnd = nDelPos + nSz;
    SwNode* pNew = (*this)[ nEnd ];

    for (SwNodeIndex& rIndex : vIndices->GetRingContainer())
    {
        sal_uLong const nIdx = rIndex.GetIndex();
        if (nDelPos <= nIdx && nIdx < nEnd)
            rIndex = *pNew;
    }

    std::vector<BigPtrEntry> aTempEntries;
    if( bDel )
    {
        sal_uLong nCnt = nSz;
        BigPtrEntry *pDel = (*this)[ nDelPos+nCnt-1 ], *pPrev = (*this)[ nDelPos+nCnt-2 ];

        // set temporary object
        // JP 24.08.98: this should actually be removed because one could
        // call Remove recursively, e.g. for character bound frames. However,
        // since there happens way too much here, this temporary object was
        // inserted that will be deleted in Remove again (see Bug 55406)
        aTempEntries.resize(nCnt);

        while( nCnt-- )
        {
            delete pDel;
            pDel = pPrev;
            sal_uLong nPrevNdIdx = pPrev->GetPos();
            BigPtrEntry* pTempEntry = &aTempEntries[nCnt];
            BigPtrArray::Replace( nPrevNdIdx+1, pTempEntry );
            if( nCnt )
                pPrev = BigPtrArray::operator []( nPrevNdIdx  - 1 );
                    // the accessed element can be a naked BigPtrEntry from
                    // aTempEntries, so the downcast to SwNode* in
                    // SwNodes::operator[] would be illegal (and unnecessary)
        }
        nDelPos = pDel->GetPos() + 1;
    }

    BigPtrArray::Remove( nDelPos, nSz );
}

void SwNodes::InsertNode( const SwNodePtr pNode,
                          const SwNodeIndex& rPos )
{
    const ElementPtr pIns = pNode;
    BigPtrArray::Insert( pIns, rPos.GetIndex() );
}

void SwNodes::InsertNode( const SwNodePtr pNode,
                          sal_uLong nPos )
{
    const ElementPtr pIns = pNode;
    BigPtrArray::Insert( pIns, nPos );
}

// ->#112139#
SwNode * SwNodes::DocumentSectionStartNode(SwNode * pNode) const
{
    if (NULL != pNode)
    {
        SwNodeIndex aIdx(*pNode);

        if (aIdx <= (*this)[0]->EndOfSectionIndex())
            pNode = (*this)[0];
        else
        {
            while ((*this)[0] != pNode->StartOfSectionNode())
                pNode = pNode->StartOfSectionNode();
        }
    }

    return pNode;
}

SwNode * SwNodes::DocumentSectionEndNode(SwNode * pNode) const
{
    return DocumentSectionStartNode(pNode)->EndOfSectionNode();
}

bool SwNodes::IsDocNodes() const
{
    return this == &pMyDoc->GetNodes();
}

void SwNodes::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    xmlTextWriterStartElement(pWriter, BAD_CAST("swNodes"));
    for (sal_uLong i = 0; i < Count(); ++i)
        (*this)[i]->dumpAsXml(pWriter);
    xmlTextWriterEndElement(pWriter);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
