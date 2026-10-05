# Project goals

## G1 — A bug is reported from inside the running application

A player presses one key, the application freezes, and an in-window form collects a summary and a
description beside the pictures the application captured. Success: one keypress to an editable
form, one keypress to save or cancel, no terminal, no second window.

## G2 — A report is enough to act on without the player

Every report carries what the player saw, a reference picture where the application has one, the
facts the application knows, and a reproduction a person or agent can run. Success: an agent given
only the report folder can replay the moment.

## G3 — One implementation for every RmlUi C++ application

The PSX ports and other RmlUi C++ projects use this library rather than their own. Success: a
consumer adds the target, supplies its context, fonts and captures, and writes no report or form
code of its own.

## Non-goals

- Uploading reports anywhere. Reports are local folders.
- Capturing anything. The application decides what to attach.
- Owning RmlUi, a window, a renderer or input routing.
