/** @file
 * IPRT - Process Environment Strings.
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

#ifndef ___iprt_env_h
#define ___iprt_env_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_env    RTEnv - Process Environment Strings
 * @ingroup grp_rt
 * @{
 */

#ifdef IN_RING3

/** Special handle that indicates the default process environment. */
#define RTENV_DEFAULT   ((RTENV)~(uintptr_t)0)

/**
 * Creates an empty environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pEnv        Where to store the handle of the new environment block.
 */
RTDECL(int) RTEnvCreate(PRTENV pEnv);

/**
 * Creates an environment block and fill it with variables from the given
 * environment array.
 *
 * @returns IPRT status code.
 * @retval  VWRN_ENV_NOT_FULLY_TRANSLATED may be returned when passing
 *          RTENV_DEFAULT and one or more of the environment variables have
 *          codeset incompatibilities.  The problematic variables will be
 *          ignored and not included in the clone, thus the clone will have
 *          fewer variables.
 * @retval  VERR_NO_MEMORY
 * @retval  VERR_NO_STR_MEMORY
 * @retval  VERR_INVALID_HANDLE
 *
 * @param   pEnv        Where to store the handle of the new environment block.
 * @param   EnvToClone  The environment to clone.
 */
RTDECL(int) RTEnvClone(PRTENV pEnv, RTENV EnvToClone);

/**
 * Creates an environment block from an UTF-16 environment raw block.
 *
 * This is the reverse of RTEnvQueryUtf16Block.
 *
 * @returns IPRT status code.
 * @retval  VERR_NO_MEMORY
 * @retval  VERR_NO_STR_MEMORY
 *
 * @param   phEnv       Where to store the handle of the new environment block.
 * @param   pwszzBlock  List of zero terminated string end with a zero length
 *                      string (or two zero terminators if you prefer).  The
 *                      strings are on the RTPutEnv format (VAR=VALUE), except
 *                      they are all expected to include an equal sign.
 * @param   fFlags      Flags served for the future.
 */
RTDECL(int) RTEnvCloneUtf16Block(PRTENV phEnv, PCRTUTF16 pwszzBlock, uint32_t fFlags);

/**
 * Destroys an environment block.
 *
 * @returns IPRT status code.
 *
 * @param   Env     Environment block handle.
 *                  Both RTENV_DEFAULT and NIL_RTENV are silently ignored.
 */
RTDECL(int) RTEnvDestroy(RTENV Env);

/**
 * Resets the environment block to contain zero variables.
 *
 * @returns IPRT status code.
 *
 * @param   hEnv    Environment block handle.  RTENV_DEFAULT is not supported.
 */
RTDECL(int) RTEnvReset(RTENV hEnv);

/**
 * Get the execve/spawnve/main envp.
 *
 * All returned strings are in the current process' codepage.
 * This array is only valid until the next RTEnv call.
 *
 * @returns Pointer to the raw array of environment variables.
 * @returns NULL if Env is NULL or invalid.
 *
 * @param   Env     Environment block handle.
 * @todo    This needs to change to return a copy of the env vars like
 *          RTEnvQueryUtf16Block does!
 */
RTDECL(char const * const *) RTEnvGetExecEnvP(RTENV Env);

/**
 * Get a sorted, UTF-16 environment block for CreateProcess.
 *
 * @returns IPRT status code.
 *
 * @param   hEnv            Environment block handle.
 * @param   ppwszzBlock     Where to return the environment block.  This must be
 *                          freed by calling RTEnvFreeUtf16Block.
 */
RTDECL(int) RTEnvQueryUtf16Block(RTENV hEnv, PRTUTF16 *ppwszzBlock);

/**
 * Frees an environment block returned by RTEnvGetUtf16Block().
 *
 * @param   pwszzBlock      What RTEnvGetUtf16Block returned.  NULL is ignored.
 */
RTDECL(void) RTEnvFreeUtf16Block(PRTUTF16 pwszzBlock);

/**
 * Get a sorted, UTF-8 environment block.
 *
 * The environment block is a sequence of putenv formatted ("NAME=VALUE" or
 * "NAME") zero terminated strings ending with an empty string (i.e. last string
 * has two zeros).
 *
 * @returns IPRT status code.
 *
 * @param   hEnv            Environment block handle.
 * @param   fSorted         Whether to sort it, this will affect @a hEnv.
 * @param   ppszzBlock      Where to return the environment block.  This must be
 *                          freed by calling RTEnvFreeUtf8Block.
 * @param   pcbBlock        Where to return the size of the block. Optional.
 */
RTDECL(int) RTEnvQueryUtf8Block(RTENV hEnv, bool fSorted, char **ppszzBlock, size_t *pcbBlock);

/**
 * Frees an environment block returned by RTEnvGetUtf8Block().
 *
 * @param   pszzBlock       What RTEnvGetUtf8Block returned.  NULL is ignored.
 */
RTDECL(void) RTEnvFreeUtf8Block(char *pszzBlock);

/**
 * Checks if an environment variable exists in the default environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pszVar      The environment variable name.
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(bool) RTEnvExist(const char *pszVar);
RTDECL(bool) RTEnvExistsBad(const char *pszVar);
RTDECL(bool) RTEnvExistsUtf8(const char *pszVar);

/**
 * Checks if an environment variable exists in a specific environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 */
RTDECL(bool) RTEnvExistEx(RTENV Env, const char *pszVar);

/**
 * Gets an environment variable from the default environment block. (getenv).
 *
 * The caller is responsible for ensuring that nobody changes the environment
 * while it's using the returned string pointer!
 *
 * @returns Pointer to read only string on success, NULL if the variable wasn't found.
 *
 * @param   pszVar      The environment variable name.
 *
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(const char *) RTEnvGet(const char *pszVar);
RTDECL(const char *) RTEnvGetBad(const char *pszVar);
RTDECL(int) RTEnvGetUtf8(const char *pszVar, char *pszValue, size_t cbValue, size_t *pcchActual);

/**
 * Gets an environment variable in a specific environment block.
 *
 * @returns IPRT status code.
 * @retval  VERR_ENV_VAR_NOT_FOUND if the variable was not found.
 * @retval  VERR_ENV_VAR_UNSET if @a hEnv is an environment change record and
 *          the variable has been recorded as unset.
 *
 * @param   hEnv        The environment handle.
 * @param   pszVar      The environment variable name.
 * @param   pszValue    Where to put the buffer.
 * @param   cbValue     The size of the value buffer.
 * @param   pcchActual  Returns the actual value string length. Optional.
 */
RTDECL(int) RTEnvGetEx(RTENV hEnv, const char *pszVar, char *pszValue, size_t cbValue, size_t *pcchActual);

/**
 * Puts an variable=value string into the environment (putenv).
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is
 *                              omitted, the variable is removed from the environment.
 *
 * @remark Don't assume the value is copied.
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(int) RTEnvPut(const char *pszVarEqualValue);
RTDECL(int) RTEnvPutBad(const char *pszVarEqualValue);
RTDECL(int) RTEnvPutUtf8(const char *pszVarEqualValue);

/**
 * Puts a copy of the passed in 'variable=value' string into the environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   Env                 Handle of the environment block.
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is
 *                              omitted, the variable is removed from the environment.
 */
RTDECL(int) RTEnvPutEx(RTENV Env, const char *pszVarEqualValue);

/**
 * Sets an environment variable (setenv(,,1)).
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pszVar      The environment variable name.
 * @param   pszValue    The environment variable value.
 *
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(int) RTEnvSet(const char *pszVar, const char *pszValue);
RTDECL(int) RTEnvSetBad(const char *pszVar, const char *pszValue);
RTDECL(int) RTEnvSetUtf8(const char *pszVar, const char *pszValue);

/**
 * Sets an environment variable (setenv(,,1)).
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 * @param   pszValue    The environment variable value.
 */
RTDECL(int) RTEnvSetEx(RTENV Env, const char *pszVar, const char *pszValue);

/**
 * Removes an environment variable from the default environment block.
 *
 * @returns IPRT status code.
 * @returns VINF_ENV_VAR_NOT_FOUND if the variable was not found.
 *
 * @param   pszVar      The environment variable name.
 *
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(int) RTEnvUnset(const char *pszVar);
RTDECL(int) RTEnvUnsetBad(const char *pszVar);
RTDECL(int) RTEnvUnsetUtf8(const char *pszVar);

/**
 * Removes an environment variable from the specified environment block.
 *
 * @returns IPRT status code.
 * @returns VINF_ENV_VAR_NOT_FOUND if the variable was not found.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 */
RTDECL(int) RTEnvUnsetEx(RTENV Env, const char *pszVar);

/**
 * Duplicates the value of a environment variable if it exists.
 *
 * @returns Pointer to a string containing the value, free it using RTStrFree.
 *          NULL if the variable was not found or we're out of memory.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 */
RTDECL(char *) RTEnvDupEx(RTENV Env, const char *pszVar);

/**
 * Counts the variables in the environment.
 *
 * @returns Number of variables in the environment. UINT32_MAX on error.
 * @param   hEnv        The environment handle.
 *                      RTENV_DEFAULT is currently not accepted.
 */
RTDECL(uint32_t) RTEnvCountEx(RTENV hEnv);

/**
 * Queries an environment variable by it's index.
 *
 * This can be used together with RTEnvCount to enumerate the environment block.
 *
 * @returns IPRT status code.
 * @retval  VERR_ENV_VAR_NOT_FOUND if the index is out of bounds, output buffers
 *          untouched.
 * @retval  VERR_BUFFER_OVERFLOW if one of the buffers are too small.  We'll
 *          fill it with as much we can in RTStrCopy fashion.
 * @retval  VINF_ENV_VAR_UNSET if @a hEnv is an environment change record and
 *          the variable at @a iVar is recorded as being unset.
 *
 * @param   hEnv        The environment handle.
 *                      RTENV_DEFAULT is currently not accepted.
 * @param   iVar        The variable index.
 * @param   pszVar      Variable name buffer.
 * @param   cbVar       The size of the variable name buffer.
 * @param   pszValue    Value buffer.
 * @param   cbValue     The size of the value buffer.
 */
RTDECL(int) RTEnvGetByIndexEx(RTENV hEnv, uint32_t iVar, char *pszVar, size_t cbVar, char *pszValue, size_t cbValue);

/**
 * Leaner and meaner version of RTEnvGetByIndexEx.
 *
 * This can be used together with RTEnvCount to enumerate the environment block.
 *
 * Use with caution as the returned pointer may change by the next call using
 * the environment handle.  Please only use this API in cases where there is no
 * chance of races.
 *
 * @returns Pointer to the internal environment variable=value string on
 *          success.  If @a hEnv is an environment change recordthe string may
 *          also be on the "variable" form, representing an unset operation. Do
 *          NOT change this string, it is read only!
 *
 *          If the index is out of range on the environment handle is invalid,
 *          NULL is returned.
 *
 * @param   hEnv        The environment handle.
 *                      RTENV_DEFAULT is currently not accepted.
 * @param   iVar        The variable index.
 */
RTDECL(const char *) RTEnvGetByIndexRawEx(RTENV hEnv, uint32_t iVar);


/**
 * Creates an empty environment change record.
 *
 * This is a special environment for use with RTEnvApplyChanges and similar
 * purposes.  The
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   phEnv       Where to store the handle of the new environment block.
 */
RTDECL(int) RTEnvCreateChangeRecord(PRTENV phEnv);

/**
 * Checks if @a hEnv is an environment change record.
 *
 * @returns true if it is, false if it's not or if the handle is invalid.
 * @param   hEnv         The environment handle.
 * @sa      RTEnvCreateChangeRecord.
 */
RTDECL(bool) RTEnvIsChangeRecord(RTENV hEnv);

/**
 * Applies changes from one environment onto another.
 *
 * If @a hEnvChanges is a normal environment, its content is just added to @a
 * hEnvDst, where variables in the destination can only be overwritten. However
 * if @a hEnvChanges is a change record environment, variables in the
 * destination can also be removed.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * @param   hEnvDst     The destination environment.
 * @param   hEnvChanges Handle to the environment containig the changes to
 *                      apply.  As said, especially useful if it's a environment
 *                      change record.  RTENV_DEFAULT is not supported here.
 */
RTDECL(int) RTEnvApplyChanges(RTENV hEnvDst, RTENV hEnvChanges);

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif

