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
#ifndef INCLUDED_SW_SOURCE_FILTER_INC_RTF_HXX
#define INCLUDED_SW_SOURCE_FILTER_INC_RTF_HXX

#include <tools/solar.h>

class RTFSurround
{
    union {
        struct {
            sal_uInt8 nGoldCut : 1; // should ideally be bool
            sal_uInt8 nOrder : 4;
            sal_uInt8 nJunk : 3;
        } Flags;
        sal_uInt8 nVal;
    } Value;
public:
    RTFSurround( sal_uInt8 nValue ) { Value.nVal = nValue; }

    RTFSurround( bool bGoldCut, sal_uInt8 nOrder ) {
        Value.Flags.nGoldCut = sal_uInt8(bGoldCut);
        Value.Flags.nOrder = nOrder;
        Value.Flags.nJunk = 0;
    }

    sal_uInt8  GetOrder()   const { return (sal_uInt8)Value.Flags.nOrder; }
    bool       GetGoldCut() const { return Value.Flags.nGoldCut; }
    sal_uInt16 GetValue()   const { return Value.nVal; }
};

#endif // INCLUDED_SW_SOURCE_FILTER_INC_RTF_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
