/** @file
 * IPRT - C++ Utilities (useful templates, defines and such).
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_cpputils_h
#define ___iprt_cpputils_h

#include <iprt/types.h>

/** @defgroup grp_rt_cpp        IPRT C++ APIs */

/** @defgroup grp_rt_cpp_util   C++ Utilities
 * @ingroup grp_rt_cpp
 * @{
 */

#define DPTR(CLASS) CLASS##Private *d = static_cast<CLASS##Private *>(d_ptr)
#define QPTR(CLASS) CLASS *q = static_cast<CLASS *>(q_ptr)

/**
 * A simple class used to prevent copying and assignment.
 *
 * Inherit from this class in order to prevent automatic generation
 * of the copy constructor and assignment operator in your class.
 */
class RTCNonCopyable
{
protected:
    RTCNonCopyable() {}
    ~RTCNonCopyable() {}
private:
    RTCNonCopyable(RTCNonCopyable const &);
    RTCNonCopyable &operator=(RTCNonCopyable const &);
};


/**
 * Shortcut to |const_cast<C &>()| that automatically derives the correct
 * type (class) for the const_cast template's argument from its own argument.
 *
 * Can be used to temporarily cancel the |const| modifier on the left-hand side
 * of assignment expressions, like this:
 * @code
 *      const Class That;
 *      ...
 *      unconst(That) = SomeValue;
 * @endcode
 *
 * @todo What to do about the prefix here?
 */
template <class C>
inline C &unconst(const C &that)
{
    return const_cast<C &>(that);
}


/**
 * Shortcut to |const_cast<C *>()| that automatically derives the correct
 * type (class) for the const_cast template's argument from its own argument.
 *
 * Can be used to temporarily cancel the |const| modifier on the left-hand side
 * of assignment expressions, like this:
 * @code
 *      const Class *pThat;
 *      ...
 *      unconst(pThat) = SomeValue;
 * @endcode
 *
 * @todo What to do about the prefix here?
 */
template <class C>
inline C *unconst(const C *that)
{
    return const_cast<C *>(that);
}


/**
 * Macro for generating a non-const getter version from a const getter.
 *
 * @param   a_RetType       The getter return type.
 * @param   a_Class         The class name.
 * @param   a_Getter        The getter name.
 * @param   a_ArgDecls      The argument declaration for the getter method.
 * @param   a_ArgList       The argument list for the call.
 */
#define RT_CPP_GETTER_UNCONST(a_RetType, a_Class, a_Getter, a_ArgDecls, a_ArgList) \
    a_RetType a_Getter a_ArgDecls \
    {  \
        return static_cast< a_Class const *>(this)-> a_Getter a_ArgList; \
    }


/**
 * Macro for generating a non-const getter version from a const getter,
 * unconsting the return value as well.
 *
 * @param   a_RetType       The getter return type.
 * @param   a_Class         The class name.
 * @param   a_Getter        The getter name.
 * @param   a_ArgDecls      The argument declaration for the getter method.
 * @param   a_ArgList       The argument list for the call.
 */
#define RT_CPP_GETTER_UNCONST_RET(a_RetType, a_Class, a_Getter, a_ArgDecls, a_ArgList) \
    a_RetType a_Getter a_ArgDecls \
    {  \
        return const_cast<a_RetType>(static_cast< a_Class const *>(this)-> a_Getter a_ArgList); \
    }


/** @} */

#endif

