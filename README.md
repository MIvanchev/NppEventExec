# NppEventExec

A handy [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) plugin which allows you to automatically execute [NppExec](https://sourceforge.net/projects/npp-plugins/files/NppExec) scripts on Notepad++ events. Compatible with recent Notepad++ versions and all NppExec versions starting from v0.2.5. See the [Compatibility section](#compatibility) for details.

* _Version: 0.1.0_
* _Binary Verison_: [Download](#installation)
* _License_: GNU GPLv3

## Introduction

NppEventExec supports the creation, management and automatic execution of rules. A rule embodies an NppExec command to be executed for the currently opened file in Notepad++ on an event. Whether the rule is executed for the current document is governed by a regular expression which the absolute file path is checked against whenever the desired event occurs. An intuitive example of a rule is: "execute the NppExec command 'Format C/C++ source' when the current file is about to be saved and it has the extension .c, .h, .cpp or .hpp."

The supported Notepad++ events are listed under [Supported Events](#supported-events).

## Installation

As of yet, NppEventExec cannot be deployed through the editor's plugin manager. To install it, download the latest binaries from this repository and extract the contents to Notepad++'s plugins directory, e.g. `C:\Program Files\Notepad++\plugins`. For the plugin to be usable, NppExec needs to be installed as well.

## Usage

The current early version of NppEventExec features no GUI editor for the creation of rules. Instead, they have to be defined in the file `NppEventExec_rules.csv` in Notepad++'s plugin configuration directory, e.g. `C:\Program Files\Notepad++\plugins\Config`. The file contains records with comma-separated fields according to [RFC 4180](https://tools.ietf.org/html/rfc4180). The initial file provided with the plugin includes an example rule to demonstrate the data format and help you create additional rules:

```
Event,Enabled?,Name,Regex,Command,Background?
NPPN_FILEBEFORESAVE,false,Format C/C++ source,.*[^.]\.c,Format C/C++ source,false
```

Most of the fields are self-explanatory; the last field controls whether the rule can be executed in the background, i.e. without blocking all input to Notepad++. It makes sense to execute source formatting rules in the foreground, because editing the code during the execution can wreck havoc. The rule is also initially disabled.

Assuming you've already added a script called 'Compile C/C++ source' to NppExec and you wish to run it every time the file is saved, extend the file by the following line:

```
Event,Enabled?,Name,Regex,Command,Background?
NPPN_FILEBEFORESAVE,false,Format C/C++ source,.*[^.]\.(c|cpp|h|hpp),Format C/C++ source,false
NPPN_FILESAVED,true,Compile C/C++ source,*[.]\.(c|cpp|h|hpp),Compile C/C++ source,true
```

You'll also notice that the new rule will execute in the background, thus not interrupting your work through a blocking dialog.

The executed NppExec script receives 2 arguments, in `$(ARGV[1])` and `$(ARGV[2])` respectively: the buffer ID and the absolute path of the file for which the rule execution was scheduled. Bear in mind that this might not be the active document at the time the rule is actually executed.

## Releases

The latest version of NppEventExec is `0.1.0`. You can grab the binaries from this repository; see [Installation](#installation) for installation instructions. This is quite an early release still missing a lot of features so be vigilant and please report any issues you encounter. Any feedback is much appreciated &ndash; share your thoughts, wishes, ideas and problems.

## Compatibility

NppEventExec has been tested on 32-bit Notepad++ v6.9.2 with NppExec v0.5.3. Earlier Notepad++ versions should be compatible; the same holds for NppExec starting with v0.2.5. Every test report with other versions is greatly appreciated! Just like Notepad++, the plugin is Unicode only. ANSI versions will not be provided due to the huge effort to ensure correct operation. The only tested OS so far is Windows 7 64-bit, though the plugin also works in [Wine](https://www.winehq.org/).

## Supported Events
Event | Meaning
------|--------
NPPN_READY | Notepad++ was fully initialized
NPPN_FILEBEFORECLOSE | A file is about to be closed
NPPN_FILECLOSED | A file was just closed
NPPN_FILEBEFOREOPEN | A file is about to be opened
NPPN_FILEOPENED | A file was just opened
NPPN_FILEBEFORESAVE | A file is about to be saved
NPPN_FILESAVED | A file was just saved
NPPN_FILEBEFORELOAD | A file is about to be loaded
NPPN_FILELOADFAILED | A file failed to load
NPPN_FILEBEFOREDELETE | A file is about to be deleted
NPPN_FILEDELETEFAILED | The deletion of a file failed
NPPN_FILEDELETED | A file is about to be deleted
NPPN_FILEBEFORERENAME | A file is about to be renamed
NPPN_FILERENAMECANCEL | The renaming of a file was canceled
NPPN_FILERENAMED | A file was renamed
NPPN_BEFORESHUTDOWN | Notepad++ is about to shutdown
NPPN_CANCELSHUTDOWN | Notepad++'s shutdown was canceled
NPPN_SHUTDOWN | Notepad++ is shutting down
NPPN_BUFFERACTIVATED | A buffer was just activated
NPPN_LANGCHANGED | A document's language was changed
NPPN_WORDSTYLESUPDATED | A change was initiated using the word style dialog
NPPN_SHORTCUTREMAPPED | A command shortcut was remapped
NPPN_READONLYCHANGED | A read-only status was changed
NPPN_DOCORDERCHANGED | The document order was changed
NPPN_SNAPSHOTDIRTYFILELOADED | An unsaved file was restored on startup

## Compilation

NppEventExec can be compiled with Visual Studio or Mingw-w64. MinGW would also work, the only problem being the missing `strsafe.h` header, which is essential for NppEventExec. There is no VS project (perhaps a helpful contributor could PR one), use the provided makefiles instead: Makefile.msvc for VS and Makefile.mingw for MinGW and Mingw-w64. MinGW and Mingw-w64 both include GNU make. You will also need [Boost.Regex](http://www.boost.org/doc/libs/1_61_0/libs/regex/doc/html/index.html), preferably the same version as Notepad++. Edit the makefiles to set the correct path and library name for Boost.Regex. An executable for quick testing is generated in the debug configuration.

## License

Copyright © 2016 Mihail Ivanchev.

Distributed under the GNU General Public License, Version 3 (GNU GPLv3)

Notepad++ and NppExec are distributed under their respective licenses.
