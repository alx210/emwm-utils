/*
 * Copyright (C) 2026 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included COPYING file for further information.
 */

/* Workspace switcher widget public header */

#ifndef WSWITCH_H 
#define WSWITCH_H

extern WidgetClass switcherWidgetClass;

#define CreateSwitcher(parent, name, args, nargs) \
	XtCreateWidget(name, switcherWidgetClass, parent, args, nargs)
#define CreateManagedSwitcher(parent, name, args, nargs) \
	XtCreateManagedWidget(name, switcherWidgetClass, parent, args, nargs)
#define VaCreateSwitcher(parent, name, ...) \
	XtVaCreateWidget(name, switcherWidgetClass, parent, __VA_ARGS__)
#define VaCreateManagedSwitcher(parent, name, ...) \
	XtVaCreateManagedWidget(name, switcherWidgetClass, parent, __VA_ARGS__)


/* These take an XtRShort argument */
#define NnumberOfWorkspaces "numberOfWorkspaces"
#define NactiveWorkspace "activeWorkspace"
#define CNumberOfWorkspaces "NumberOfWorkspaces"
#define CActiveWorkspace "ActiveWorkspace"
#define NpaddingWidth "paddingWidth"
#define CPaddingWidth "PaddingWidth"
#define NpaddingHeight "paddingHeight"
#define CPaddingHeight "PaddingHeight"

void SwitcherSetActiveWorkspace(Widget, unsigned short);

#endif /* WSWITCH_H */
