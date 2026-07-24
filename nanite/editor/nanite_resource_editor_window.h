/**************************************************************************/
/*  nanite_resource_editor_window.h                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#ifdef TOOLS_ENABLED

#include "scene/gui/dialogs.h"

class NaniteMeshEditor;
class NaniteMeshResource;

// NaniteMeshResourceEditorWindow is a standalone popup window that hosts a
// NaniteMeshEditor for interactive 3D preview of a NaniteMeshResource. It is
// shown by NaniteEditorPlugin::edit() when the user double-clicks a
// .nanite.tres file in the FileSystem dock. The window is reused across
// edits (hide() on close, never queue_free()) so subsequent edits are cheap.
class NaniteMeshResourceEditorWindow : public AcceptDialog {
	GDCLASS(NaniteMeshResourceEditorWindow, AcceptDialog);

private:
	NaniteMeshEditor *viewer = nullptr;

protected:
	static void _bind_methods();

public:
	// Load p_resource into the embedded viewer, update the window title,
	// and popup centered. Safe to call repeatedly; the window instance is
	// reused across calls.
	void edit(const Ref<NaniteMeshResource> &p_resource);

	NaniteMeshResourceEditorWindow();
	~NaniteMeshResourceEditorWindow();
};

#endif // TOOLS_ENABLED
