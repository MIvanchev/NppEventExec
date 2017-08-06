/*
This file is part of NppEventExec
Copyright (C) 2016-2017 Mihail Ivanchev

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "base.h"
#include "plugin.h"
#include "csv.h"
#include "mem.h"
#include "match.h"
#include "rule.h"
#include "edit_dlg.h"
#include "rules_dlg.h"
#include "util.h"
#include "Scintilla.h"
#include "exec.h"
#include "resource.h"
#include "about_dlg.h"
#include "queue_dlg.h"
#include "PluginInterface.h"
#include "nppexec_msgs.h"

#ifdef DEBUG
#include <time.h>
#endif

static void initPlugin(NppData data);
static void deinitPlugin(void);
static bool validateModuleName(wchar_t **dir);
static wchar_t* queryConfigDir(void);
static void onEditRules(void);
static void onExecQueue(void);
static void onAbout(void);

static wchar_t *pluginDir;
static wchar_t *configDir;
static HINSTANCE pluginInst;
static HWND nppWnd;

static const TCHAR NPP_PLUGIN_NAME[] = PLUGIN_NAME;
static FuncItem menuItems[] = {
    { L"Edit rules...", onEditRules, 0, false, NULL },
    { L"Execution queue...", onExecQueue, 0, false, NULL },
    { L"", NULL, 0, false, NULL },
    { L"About...", onAbout, 0, false, NULL }
};
static bool nppExecLoaded;
static bool initFailed;
static Rule *rules;

void initPlugin(NppData data)
{
    nppWnd = data._nppHandle;

    if (!validateModuleName(&pluginDir))
    {
        /* TODO error */
        errorMsgBox(NULL,
                    L"Failed to validate the plugin's module path. The plugin will not function until the issues are resolved.");
        goto fail_mod;
    }
    if (!(configDir = queryConfigDir()))
    {
        /* TODO error */
        errorMsgBox(NULL,
                    L"Failed to query Notepad++'s configuration directory. The plugin will not function until the issues are resolved.");
        goto fail_config;
    }
    if (readRules(&rules))
    {
        /* TODO error */
        errorMsgBox(NULL,
                    L"Failed to read the rules list. The plugin will not function until the issues are resolved.");
        goto fail_rules;
    }

    initFailed = false;
    return;

fail_rules:
    freeStr(configDir);
fail_config:
    freeStr(pluginDir);
fail_mod:
    initFailed = true;
}

void deinitPlugin(void)
{
    freeRules(rules);
    freeStr(configDir);
    freeStr(pluginDir);
}

bool validateModuleName(wchar_t **dir)
{
    wchar_t *path;
    wchar_t *tmp;
    DWORD len;
    DWORD maxLen;
    wchar_t *fname;

    assert(MAX_PATH < SIZE_MAX);
    assert(MAX_PATH < (DWORD) -1);
    assert(dir);

    len = MAX_PATH + 1;
    maxLen = MIN(SIZE_MAX, (DWORD) -1);

    if (!(path = allocStr(len)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    while (!GetModuleFileNameW(pluginInst, path, len))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            if (len == maxLen)
            {
                /* TODO error */
                goto fail_too_long;
            }
            else if (len > maxLen - MAX_PATH)
                len = maxLen;
            else
                len += MAX_PATH;

            if (!(tmp = reallocStr(path, len)))
            {
                /* TODO error */
                goto fail_realloc;
            }

            path = tmp;
        }
        else
        {
            /* TODO error */
            goto fail_read;
        }
    }

    fname = getFilename(path);

    if (wcscmp(fname, PLUGIN_MODULE))
    {
        /* TODO error */
        goto fail_invalid;
    }

    *(fname - 1) = L'\0';

    if (!(*dir = reallocStr(path, wcslen(path) + 1)))
    {
        /* TODO error */
        goto fail_resize;
    }

    return true;

fail_resize:
fail_invalid:
fail_read:
fail_realloc:
fail_too_long:
    freeStr(path);
fail_alloc:
    return false;
}

wchar_t* queryConfigDir(void)
{
    wchar_t *dir;
    wchar_t *tmp;
    WPARAM len;
    WPARAM maxLen;

    assert(MAX_PATH < SIZE_MAX);
    assert(MAX_PATH < (WPARAM) -1);
    assert(MAX_PATH >= 4);

    len = MAX_PATH + 1;
    maxLen = (WPARAM) -1;

    if (!(dir = allocStr(len)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    StringCchCopyW(dir, len, L"\\\\?\\");

    while (!sendNppMsg(NPPM_GETPLUGINSCONFIGDIR, len - 5,
                       reinterpret_cast<LPARAM>(&dir[4])))
    {

        /* Just assume the problem is the insufficient length although it could
        ** be something else. No way to know.
        */

        if (len == maxLen)
        {
            /* TODO error */
            goto fail_too_long;
        }
        else if (len > maxLen - MAX_PATH)
            len = maxLen;
        else
            len += MAX_PATH;

        if (!(tmp = reallocStr(dir, len)))
        {
            /* TODO error */
            goto fail_realloc;
        }

        dir = tmp;
    }

    if (!(tmp = reallocStr(dir, wcslen(dir) + 1)))
    {
        /* TODO error */
        goto fail_resize;
    }

    return tmp;

fail_resize:
fail_realloc:
fail_too_long:
    freeStr(dir);
fail_alloc:
    return NULL;
}

bool queryNppExecLoaded(void)
{
    DWORD ver;
    return sendNppExecMsg(NPEM_GETVERDWORD,
                          reinterpret_cast<void*>(&ver)) == TRUE;
}

void execRules(uptr_t bufId, unsigned int code)
{
    LRESULT unitCnt;
    size_t unitCntSizeT;
    wchar_t *path;

    unitCnt = sendNppMsg(NPPM_GETFULLPATHFROMBUFFERID,
                         static_cast<WPARAM>(bufId),
                         static_cast<LPARAM>(NULL));

    /* Let's not assume that Notepad++ keeps the path string in a contiguous
    ** region of memory in a zero-terminated fashion. The conversion to size_t
    ** for the validation is based on the fact that conversion to an unsigned
    ** integer is always defined behaviour.
    */

    unitCntSizeT = static_cast<size_t>(unitCnt);

    if (static_cast<LRESULT>(unitCntSizeT) != unitCnt
        || unitCntSizeT == SIZE_MAX)
    {
        /* TODO error */
        goto fail_path_too_long;
    }
    if (!(path = allocStr(unitCntSizeT + 1)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    sendNppMsg(NPPM_GETFULLPATHFROMBUFFERID,
               static_cast<WPARAM>(bufId),
               reinterpret_cast<LPARAM>(path));

    for (Rule *rule = rules; rule; rule = rule->next)
    {
        if (rule->event == code
            && rule->enabled
            && isRegexMatch(rule->regex, path))
        {
            if (execRule(bufId, path, rule))
            {
                /* TODO error */
            }
        }
    }

    freeStr(path);
    return;

fail_alloc:
fail_path_too_long:
    return;
}

void onEditRules(void)
{
    if (isPluginInit())
    {
        if (openRulesDlg(&rules))
        {
            /* TODO error */
            errorMsgBox(nppWnd, L"Failed to open the rule management dialog.");
        }
    }
    else
        errorMsgBox(nppWnd, L"The plugin was not fully initialized.");
}

void onExecQueue(void)
{
    if (isPluginInit())
    {
        if (openQueueDlg(getNppWnd(), QDLR_PLUGIN_MENU) == -1)
        {
            /* TODO error */
            errorMsgBox(nppWnd, L"Failed to open the execution queue dialog.");
        }
    }
    else
        errorMsgBox(nppWnd, L"The plugin was not fully initialized.");
}

void onAbout(void)
{
    if (isPluginInit())
        openAboutDlg();
    else
        errorMsgBox(nppWnd, L"The plugin was not fully initialized.");
}

int isPluginInit(void)
{
    return !initFailed;
}

LRESULT sendNppMsg(UINT msg, WPARAM wp, LPARAM lp)
{
    return SendMessage(nppWnd, msg, wp, lp);
}

LRESULT sendNppExecMsg(long msg, void *info)
{
    CommunicationInfo ci = { msg, PLUGIN_MODULE, info };
    return sendNppMsg(NPPM_MSGTOPLUGIN,
                      reinterpret_cast<WPARAM>(NPPEXEC_PLUGIN_MODULE),
                      reinterpret_cast<LPARAM>(&ci));
}

wchar_t* getPluginDir(void)
{
    return pluginDir;
}

wchar_t* getPluginConfigDir(void)
{
    return configDir;
}

HINSTANCE getPluginInstance(void)
{
    return pluginInst;
}

HWND getNppWnd(void)
{
    return nppWnd;
}

BOOL APIENTRY DllMain(HINSTANCE inst,
                      DWORD reason,
                      LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        pluginInst = inst;

        /* The initialization occurs once Notepad++ informs us about its
        ** environment through the setInfo method.
        */
        break;

    case DLL_PROCESS_DETACH:
        /* The deinitialization occurs in the shutdown event handler because of
        ** complications when synchronizing threads here.
        **
        ** https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971(v=vs.85).aspx
        */
        break;
    }

    return TRUE;
}

#ifdef DEBUG

LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    wchar_t *path;
    size_t len;
    CommunicationInfo *ci;
    DWORD *state;
    Rule *rule;

    switch(msg)
    {
    case WM_CLOSE:
        DestroyWindow(wnd);
        break;
    case WM_DESTROY:
        if (!isQueueEmpty())
            emptyQueue();

        deinitPlugin();

        if (allocatedBytes)
            printf("Unallocated memory: %lu bytes\n", allocatedBytes);

        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE)
            DestroyWindow(wnd);
        else
        {
            for (rule = rules; rule; rule = rule->next)
                execRule(0, L"C:\\foo.txt", rule);

            // openQueueDlg(getNppWnd(), QDLR_PLUGIN_MENU);
            // openAboutDlg();
            // openRulesDlg(&rules);
        }

        break;
    case NPPM_MSGTOPLUGIN:
        ci = (CommunicationInfo*) lp;

        if (ci->internalMsg == NPEM_NPPEXEC)
            ((NpeNppExecParam*) ci->info)->dwResult = NPE_EXECUTE_FAILED;
        else if (ci->internalMsg == NPEM_GETSTATE)
        {
            state = static_cast<DWORD*>(ci->info);

            // if (rand() < RAND_MAX / 5)
            // *state = NPE_EXECUTE_OK;
            // else
            *state = NPE_EXECUTE_FAILED;
        }

        break;
    case NPPM_GETFULLPATHFROMBUFFERID:
        switch (wp)
        {
        case 0:
            path = copyStr(L"C:\\Users\\BAREMETAL\\Desktop\\recipes");
            break;
        case 1:
            path = copyStr(L"E:\\Develop");
            break;
        case 2:
            path = copyStr(L"E:\\Software");
            break;
        case 3:
            path = copyStr(L"E:\\Servers\\xampp");
            break;
        default:
            path = copyStr(L"D:\\notes\\on-rare-plants.docx");
        }

        len = wcslen(path);
        if ((wchar_t*) lp)
        {
            StringCchCopyW(reinterpret_cast<wchar_t*>(lp), len + 1, path);
        }

        freeStr(path);

        return static_cast<LRESULT>(len);
    default:
        return DefWindowProc(wnd, msg, wp, lp);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    MSG msg;

    srand(time(NULL));

    pluginInst = GetModuleHandle(NULL);
    configDir = copyStr(L"config");
    pluginDir = copyStr(L".");
    readRules(&rules);
    printRules(rules);

    WNDCLASSEXW wcex;
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = 0;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = pluginInst;
    wcex.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = L"myWindowClass";
    wcex.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassExW(&wcex);

    nppWnd = CreateWindowExW(WS_EX_CLIENTEDGE,
                             L"myWindowClass",
                             L"The title of my window",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             240,
                             120,
                             NULL,
                             NULL,
                             pluginInst,
                             NULL);

    ShowWindow(nppWnd, SW_SHOW);
    UpdateWindow(nppWnd);
    SetWindowPos(nppWnd, NULL, 400, 400, 0, 0, SWP_NOSIZE);

    while(GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

#endif /* DEBUG */

/******************************************************************************\
*                                                                             *
* Notepad++ plugin interface                                                  *
*                                                                             *
\******************************************************************************/

void setInfo(NppData data)
{
    initPlugin(data);
}

const TCHAR* getName()
{
    return NPP_PLUGIN_NAME;
}

FuncItem* getFuncsArray(int *nbF)
{
    *nbF = sizeof(menuItems) / sizeof(menuItems[0]);
    return menuItems;
}

void beNotified(SCNotification *notification)
{
    Sci_NotifyHeader *hdr;

    hdr = &notification->nmhdr;

    if (!isPluginInit() || reinterpret_cast<HWND>(hdr->hwndFrom) != nppWnd)
        return;

    if (hdr->code == NPPN_READY && !(nppExecLoaded = queryNppExecLoaded()))
    {
        errorMsgBox(nppWnd,
                    L"NppExec was not loaded. No scripts will be executed until the issues are resolved.");
        return;
    }

    if (nppExecLoaded)
        execRules(hdr->idFrom, hdr->code);

    if (hdr->code == NPPN_SHUTDOWN)
    {
        if (!isQueueEmpty()
            && openQueueDlg(getNppWnd(), QDLR_NPP_CLOSING) == -1)
        {
            /* TODO error */
            emptyQueue();
            errorMsgBox(nppWnd,
                        L"The remaining rules were forcefully terminated, because Notepad++ is shutting down and the wait dialog failed to launch.");
        }

        deinitPlugin();

#ifdef DEBUG
        if (allocatedBytes)
        {
            errorMsgBox(nppWnd,
                        L"Unallocated memory: %lu bytes",
                        allocatedBytes);
        }
#endif
    }
}

LRESULT messageProc(UINT msg, WPARAM wp, LPARAM lp)
{
    return TRUE;
}

BOOL isUnicode()
{
    return TRUE;
}
