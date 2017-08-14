# NppEventExec
[![SemVer](https://img.shields.io/badge/version-0.9.0-brightgreen.svg)](http://semver.org)
[![Build status](https://ci.appveyor.com/api/projects/status/eav4wt6kbx47d74y?svg=true)](https://ci.appveyor.com/project/MIvanchev/nppeventexec)
[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](http://www.gnu.org/licenses/gpl-3.0)

A handy [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) plugin which allows you to automatically execute [NppExec](https://sourceforge.net/projects/npp-plugins/files/NppExec) scripts on Notepad++ events. Compatible with recent Notepad++ and NppExec versions; see  [Compatibility](#compatibility) for supported older versions.

[<img src="https://raw.githubusercontent.com/MIvanchev/NppEventExec/master/demo/demo.png" width="640" alt="Demo thumbnail">](https://raw.githubusercontent.com/MIvanchev/NppEventExec/master/demo/demo.gif)

## Contents

* [Introduction](#introduction)
* [Installation](#installation)
* [Usage](#usage)
  * [Defining commands](#defining-commands)
  * [Managing rules](#managing-rules)
  * [Execution queue and aborting rules](#execution-queue-and-aborting-rules)
  * [Examples](#examples)
* [Releases](#releases)
* [Compatibility](#compatibility)
* [Supported Events](#supported-events)
* [Compilation](#compilation)
* [Known issues](#known-issues)
* [Help and support](#help-and-support)
* [License](#license)

## Introduction

NppEventExec supports the creation, management and automatic execution of rules. A rule embodies an NppExec command to be executed for the currently active document in Notepad++ on an event. Whether the rule is executed for the current document is governed by a regular expression which the absolute file path is checked against whenever the desired event occurs. An intuitive example of a rule is: "execute the NppExec command 'Format C/C++ source' when the current file is about to be saved and it has the extension .c, .h, .cpp or .hpp."

The supported Notepad++ events are listed under [Supported Events](#supported-events).

## Installation

As of yet, NppEventExec cannot be installed through the editor's plugin manager. Instead, download the latest binaries for your platform &ndash; [x86](https://github.com/MIvanchev/NppEventExec/releases/download/v0.9.0/NppEventExec-plugin-x86-0.9.0.zip) or [x64](https://github.com/MIvanchev/NppEventExec/releases/download/v0.9.0/NppEventExec-plugin-x64-0.9.0.zip) &ndash; from this repository and extract the contents to Notepad++'s plugins directory, e.g. `C:\Program Files\Notepad++\plugins`. For the plugin to be usable, NppExec needs to be installed as well.

## Usage

### Defining commands

This documentation assumes you're acquinted with NppExec. This is because NppEventExec's purpose is the automatic execution of commands which were previously defined in NppExec or the path to a file containing an NppExec script on Notepad++ events, e.g. a document is about to be saved, a new document was created etc. For additional control, you can define a regular expression which the absolute path of the active document is checked against before the execution takes place. To specify what gets executed when, NppEventExec supports to creation of *rules*.

### Managing rules

To create or edit rules, select <i>Plugins->NppEventExec->Edit rules...</i> from Notepad++'s main menu. This opens the rule management dialog. To edit a specific rule either select the rule and then click on the edit button in the toolbar or double-click on the rule in the list. The rule attributes are as follows:

Attribute | Meaning
------|--------
Enabled? | When true, the rule is considered active and will be executed when the conditions are met. Otherwise the plugin will ignore the rule. Useful for example if you want to prevent the rule from executing for a short while, but don't want to delete it permanently.
Event | The Notepad++ event the rule will be executed on.
Name | The name of the rule. It cannot be empty and cannot begin or end with white-space characters. The rules are named solely for convinience.
Regex | The regular expression that the file system path of the currently active document is checked against before executing the rule.
Command | The name of the NppExec command or the absolute path to a file containing an NppExec script to execute when the conditions are met.
Background? | When true, the rule is executed in the background, i.e. it will allow the user to continue working in Notepad++ normally while the rule is executing. Otherwise the user will be prevented from interacting with Notepad++ until the rule finishes which makes sense for example when the document's content should not be changed during the rule's execution.

The modifications are only written to the disk when you click on the Save button. If any rules are executing, the plugin will wait until they finish or you abort them. Clicking on Reset will reset **all** changes you've made to the rules.

### Execution queue and aborting rules
To see the rule which is currently executing as well as all scheduled rules, select <i>Plugins->NppEventExec->Execution queue...</i> from Notepad++'s main menu. This opens the queue dialog which allows you to abort all rules except for the rule that NppExec is currently executing.

The queue dialog opens automatically in the following cases:
* when a non-background rule is executed; the dialog is shown and cannot be closed to prevent you from interacting with Notepad++;
* when rule modifications are being saved, but a rule is still being executed or a number of rules are queued for execution;
* when Notepad++ is closing, but a number of rules are queued for execution.

### Examples
A good usage scenario for NppEventExec is auto-formatting source code files. Assuming you want to use [uncrustify](http://uncrustify.sourceforge.net/) to auto-format for C/C++ source and you've defined your preferences in a config file somewhere. The first step is to create an NppExec command called `Format C/C++ source` with content similar to [this](https://github.com/MIvanchev/snippets/blob/master/NppExec/Format%20source.script). Then, open the rule managent dialog and create a new rule with the name `Format C/C++ source` (or any other name), the command `Format C/C++ source` and the regular expression  `.*[^.]\.(c|cpp|h|hpp)`. It makes sense to execute the rule before the file is saved so select `NPPN_FILEBEFORESAVE` and it **definitely** makes a lot of sense to prevent the user from modifying the contents while the rule is executing so make sure the option to block the UI is checked. Finally, enable the rule and try to save a C/C++ source file.

### Technical
The executed NppExec scripts receive 2 arguments from NppEventExec, in `$(ARGV[1])` and `$(ARGV[2])` respectively: the buffer ID and the absolute path of the document for which the rule was executed. Bear in mind that this might not be the active document at the time the rule is actually executed.

## Releases

The latest version of NppEventExec is `0.9.0`. You can grab the binaries from this repository; see [Installation](#installation) for installation instructions. Although the plugin has grown in features since previous releases, they are yet to be put to a trial by the broad public. Therefore, please be vigilant and report any issues you encounter. Any feedback is much appreciated &ndash; share your thoughts, wishes, ideas and problems.

## Compatibility

NppEventExec has been tested on 32-bit and 64-bit Notepad++. The minimal Notepad++ versions known to work are v6.9.2 (32-bit) and v7.4.2 (64-bit). Earlier versions should be compatible as well. The minimal NppExec versions known to work are v0.5.3 (32-bit) and v0.5.9.9 (64-bit). Theoretically, all versions starting with v0.2.5 should be compatible.

Every test report with other versions is greatly appreciated! Just like Notepad++, the plugin is Unicode only. ANSI versions will not be provided due to the huge effort to ensure correct operation. The only tested OS so far is Windows 7 64-bit, though the plugin also works in [Wine](https://www.winehq.org/).

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

NppEventExec can be compiled with Visual Studio 2013 and Mingw-w64. MinGW would also work, the only problem being the missing `strsafe.h` header, which is essential for NppEventExec. The Mingw-w64/MinGW makefile supports the same configurations and platforms as the VS project:

```
make CONFIGURATION=Debug PLATFORM=Win32
make CONFIGURATION=Debug PLATFORM=x64
make CONFIGURATION=Release PLATFORM=Win32
make CONFIGURATION=Release PLATFORM=x64
```

The default configuration and platform are `Debug` and `Win32` respectively. To be fully compatible with the regular expression format of Notepad++, NppEventExec depends on [Boost.Regex](http://www.boost.org/doc/libs/1_58_0/libs/regex/doc/html/index.html). If you use Mingw-w64 or MinGW, edit the makefile to set the correct path and library name for Boost.Regex. When using Visual Studio, you can quickly obtain precompiled binaries with [NuGet](https://www.nuget.org/). An executable for quick testing is generated in the debug configuration, but only when using the makefile.

## Known issues

Currently, there are issues when foreground rules are triggered from NppExec scripts or from the console, e.g. by saving or opening a file. Depending on the circumstances, a rule might not get executed and will be stuck in the state `Waiting` until it's aborted. In another case, the user may experience that rules are executed out of order. This is partly due to the single threaded nature of NppExec and partly due to unfilled gaps in the scheduling semantics of NppEventExec.

Please report any abnormal or counter-intuitive behaviour you encouter while using the plugin or just share how you are using the plugin. Any information on the patterns in which users are using NppEventExec is valuable.

## Help and support

The best place to discuss the plugin and to receive help is the [Notepad++ Community](https://notepad-plus-plus.org/community/). You can also submit issues through the community's website or use the [issues section](issues) of this repository (you need to sign in). Of course, you can also contact me privately.

## Credits

The plugin is using **modified** icons from the set [16x16 free application icons](http://www.small-icons.com/packs/16x16-free-application-icons.htm) from [Aha-Soft](http://www.aha-soft.com/). The said icon pack is distributed under the license [Creative Commons Attribution 3.0 United States (CC BY 3.0 US)](https://creativecommons.org/licenses/by/3.0/us/). Aha-Soft makes awesome icons.

## License

Copyright © 2016-2017 Mihail Ivanchev.

Distributed under the GNU General Public License, Version 3 (GNU GPLv3)

Notepad++, NppExec and the icon set "16x16 free application icons" from Aha-Soft are distributed under their respective licenses.
