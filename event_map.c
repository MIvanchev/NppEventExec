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
#include "event_map.h"
#include "Notepad_plus_msgs.h"

#define ENTRY(c, m) { NPPN_ ## c, L"NPPN_" STRW(c) }

const EventMapEntry eventMap[] =
{
    ENTRY(READY, L"Notepad++ was fully initialized"),
    ENTRY(FILEBEFORECLOSE, L"A file is about to be closed"),
    ENTRY(FILECLOSED, L"A file was just closed"),
    ENTRY(FILEBEFOREOPEN, L"A file is about to be opened"),
    ENTRY(FILEOPENED, L"A file was just opened"),
    ENTRY(FILEBEFORESAVE, L"A file is about to be saved"),
    ENTRY(FILESAVED, L"A file was just saved"),
    ENTRY(FILEBEFORELOAD, L"A file is about to be loaded"),
    ENTRY(FILELOADFAILED, L"A file failed to load"),
    ENTRY(FILEBEFOREDELETE, L"A file is about to be deleted"),
    ENTRY(FILEDELETEFAILED, L"The deletion of a file failed"),
    ENTRY(FILEDELETED, L"A file is about to be deleted"),
    ENTRY(FILEBEFORERENAME, L"A file is about to be renamed"),
    ENTRY(FILERENAMECANCEL, L"The renaming of a file was canceled"),
    ENTRY(FILERENAMED, L"A file was renamed"),
    ENTRY(BEFORESHUTDOWN, L"Notepad++ is about to shutdown"),
    ENTRY(CANCELSHUTDOWN, L"Notepad++'s shutdown was canceled"),
    ENTRY(SHUTDOWN, L"Notepad++ is shutting down"),
    ENTRY(BUFFERACTIVATED, L"A buffer was just activated"),
    ENTRY(LANGCHANGED, L"A document's language was changed"),
    ENTRY(WORDSTYLESUPDATED,
          L"A change was initiated using the word style dialog"),
    ENTRY(SHORTCUTREMAPPED, L"A command shortcut was remapped"),
    ENTRY(READONLYCHANGED, L"A read-only status was changed"),
    ENTRY(DOCORDERCHANGED, L"The document order was changed"),
    ENTRY(SNAPSHOTDIRTYFILELOADED, L"An unsaved file was restored on startup"),
};

const size_t eventMapSize = BUFLEN(eventMap);

int getEventMapEntryIndex(unsigned int event, size_t *index)
{
    size_t ii;

    for (ii = 0; ii < eventMapSize; ii++)
    {
        if (eventMap[ii].event == event)
        {
            *index = ii;
            return 1;
        }
    }

    return 0;
}

const EventMapEntry* getEventMapEntry(unsigned int event)
{
    size_t ii;

    for (ii = 0; ii < eventMapSize; ii++)
    {
        if (eventMap[ii].event == event)
            return &eventMap[ii];
    }

    return NULL;
}
