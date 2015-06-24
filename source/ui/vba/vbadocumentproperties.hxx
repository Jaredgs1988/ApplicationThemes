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
#ifndef INCLUDED_SW_SOURCE_UI_VBA_VBADOCUMENTPROPERTIES_HXX
#define INCLUDED_SW_SOURCE_UI_VBA_VBADOCUMENTPROPERTIES_HXX

#include <ooo/vba/XDocumentProperties.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <vbahelper/vbacollectionimpl.hxx>

typedef CollTestImplHelper< ov::XDocumentProperties > SwVbaDocumentproperties_BASE;

class SwVbaBuiltinDocumentProperties : public SwVbaDocumentproperties_BASE
{
protected:
    css::uno::Reference< css::frame::XModel > m_xModel;
public:
    SwVbaBuiltinDocumentProperties( const css::uno::Reference< ov::XHelperInterface >& xParent, const css::uno::Reference< css::uno::XComponentContext > & xContext, const css::uno::Reference< css::frame::XModel >& xDocument );

    // XDocumentProperties
    virtual css::uno::Reference< ::ooo::vba::XDocumentProperty > SAL_CALL Add( const OUString& Name, sal_Bool LinkToContent, ::sal_Int8 Type, const css::uno::Any& Value, const css::uno::Any& LinkSource ) throw (css::script::BasicErrorException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    // XEnumerationAccess
    virtual css::uno::Type SAL_CALL getElementType() throw (css::uno::RuntimeException) SAL_OVERRIDE;
    virtual css::uno::Reference< css::container::XEnumeration > SAL_CALL createEnumeration() throw (css::uno::RuntimeException) SAL_OVERRIDE;
    // ScVbaCollectionBaseImpl
    virtual css::uno::Any createCollectionObject( const css::uno::Any& aSource ) SAL_OVERRIDE;

    // XHelperInterface
    virtual OUString getServiceImplName() SAL_OVERRIDE;
    virtual css::uno::Sequence<OUString> getServiceNames() SAL_OVERRIDE;
};

class SwVbaCustomDocumentProperties : public SwVbaBuiltinDocumentProperties
{
public:
    SwVbaCustomDocumentProperties( const css::uno::Reference< ov::XHelperInterface >& xParent, const css::uno::Reference< css::uno::XComponentContext > & xContext, const css::uno::Reference< css::frame::XModel >& xDocument );
// XDocumentProperties
    virtual css::uno::Reference< ::ooo::vba::XDocumentProperty > SAL_CALL Add( const OUString& Name, sal_Bool LinkToContent, ::sal_Int8 Type, const css::uno::Any& Value, const css::uno::Any& LinkSource ) throw (css::script::BasicErrorException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    // XHelperInterface
    virtual OUString getServiceImplName() SAL_OVERRIDE;
};

#endif /* SW_VBA_DOCUMENTPROPERTY_HXX */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
