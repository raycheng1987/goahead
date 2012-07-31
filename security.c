/*
    security.c -- Security handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "wsIntrn.h"
#include    "um.h"
#ifdef DIGEST_ACCESS_SUPPORT
#include    "websda.h"
#endif

/********************************** Defines ***********************************/
/*
    The following #defines change the behaviour of security in the absence of User Management.
    Note that use of User management functions require prior calling of umInit() to behave correctly
 */
#ifndef USER_MANAGEMENT_SUPPORT
#define umGetAccessMethodForURL(url) AM_FULL
#define umUserExists(userid) 0
#define umUserCanAccessURL(userid, url) 1
#define umGetUserPassword(userid) websGetPassword()
#define umGetAccessLimitSecure(accessLimit) 0
#define umGetAccessLimit(url) NULL
#endif

/******************************** Local Data **********************************/

static char_t   websPassword[WEBS_MAX_PASS];    /* Access password (decoded) */
//  MOB
#ifdef _DEBUG
static int      debugSecurity = 1;
#else
static int      debugSecurity = 0;
#endif

/*********************************** Code *************************************/
/*
    Determine if this request should be honored
 */
int websSecurityHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
    char_t          *type, *userid, *password, *accessLimit;
    accessMeth_t    am;
    int             flags, nRet;

    a_assert(websValid(wp));
    a_assert(url && *url);
    a_assert(path && *path);

    /*
        Get the critical request details
     */
    type = websGetRequestType(wp);
    password = websGetRequestPassword(wp);
    userid = websGetRequestUserName(wp);
    flags = websGetRequestFlags(wp);
    /*
        Get the access limit for the URL.  Exit if none found.
     */
    accessLimit = umGetAccessLimit(path);
    if (accessLimit == NULL) {
        return 0;
    }
         
    /*
        Check to see if URL must be encrypted
     */
#ifdef WEBS_SSL_SUPPORT
    nRet = umGetAccessLimitSecure(accessLimit);
    if (nRet && ((flags & WEBS_SECURE) == 0)) {
        websStats.access++;
        websError(wp, 405, T("Access Denied\nSecure access is required."));
        trace(3, T("SEC: Non-secure access attempted on <%s>\n"), path);
      /* bugfix 5/24/02 -- we were leaking the memory pointed to by
       * 'accessLimit'. Thanks to Simon Byholm.
       */
      bfree(accessLimit);
        return 1;
    }
#endif

    /*
        Get the access limit for the URL
     */
    am = umGetAccessMethodForURL(accessLimit);

    nRet = 0;
    if ((flags & WEBS_LOCAL_REQUEST) && (debugSecurity == 0)) {
        /*
            Local access is always allowed (defeat when debugging)
         */
    } else if (am == AM_NONE) {
        /*
            URL is supposed to be hidden!  Make like it wasn't found.
         */
        websStats.access++;
        websError(wp, 404, T("Page Not Found"));
        nRet = 1;
    } else  if (userid && *userid) {
        if (!umUserExists(userid)) {
            websStats.access++;
            websError(wp, 401, T("Access Denied\nUnknown User"));
            trace(3, T("SEC: Unknown user <%s> attempted to access <%s>\n"), 
                userid, path);
            nRet = 1;
        } else if (!umUserCanAccessURL(userid, accessLimit)) {
            websStats.access++;
            websError(wp, 403, T("Access Denied\nProhibited User"));
            nRet = 1;
        } else if (password && * password) {
            char_t * userpass = umGetUserPassword(userid);
            if (userpass) {
                if (gstrcmp(password, userpass) != 0) {
                    websStats.access++;
                    websError(wp, 401, T("Access Denied\nWrong Password"));
                    trace(3, T("SEC: Password fail for user <%s>")
                                T("attempt to access <%s>\n"), userid, path);
                    nRet = 1;
                } else {
                    /*
                        User and password check out.
                     */
                }
                bfree (userpass);
            }
#ifdef DIGEST_ACCESS_SUPPORT
        } else if (flags & WEBS_AUTH_DIGEST) {
            char_t *digestCalc;

            /*
                Check digest for equivalence
             */
            wp->password = umGetUserPassword(userid);

            a_assert(wp->digest);
            a_assert(wp->nonce);
            a_assert(wp->password);
                             
            digestCalc = websCalcDigest(wp);
            a_assert(digestCalc);

            if (gstrcmp(wp->digest, digestCalc) != 0) {
                bfree (digestCalc);
                digestCalc = websCalcUrlDigest(wp);
                a_assert(digestCalc);
                if (gstrcmp(wp->digest, digestCalc) != 0) {
                    websStats.access++;

                    websError(wp, 401, T("Access Denied\nWrong Password"));
                    nRet = 1;
                }
            }

            bfree (digestCalc);
#endif
        } else {
            /*
                No password has been specified
             */
#ifdef DIGEST_ACCESS_SUPPORT
            if (am == AM_DIGEST) {
                wp->flags |= WEBS_AUTH_DIGEST;
            }
#endif
            websStats.errors++;
            websError(wp, 401, 
                T("Access to this document requires a password"));
            nRet = 1;
        }
    } else if (am != AM_FULL) {
        /*
                This will cause the browser to display a password / username dialog
         */
#ifdef DIGEST_ACCESS_SUPPORT
        if (am == AM_DIGEST) {
            wp->flags |= WEBS_AUTH_DIGEST;
        }
#endif
        websStats.errors++;
        websError(wp, 401, T("Access to this document requires a User ID"));
        nRet = 1;
    }
    bfree(accessLimit);
    return nRet;
}


void websSecurityDelete()
{
    websUrlHandlerDelete(websSecurityHandler);
}


/*
    Store the new password, expect a decoded password. Store in websPassword in the decoded form.
 */
void websSetPassword(char_t *password)
{
    a_assert(password);

    gstrncpy(websPassword, password, TSZ(websPassword));
}


/*
    Get password, return the decoded form
 */
char_t *websGetPassword()
{
    return bstrdup(websPassword);
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
