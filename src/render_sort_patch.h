#pragma once

struct so_module;

/*
 * Replace m3g::SortedRenderNode::operator< with a transitive version of the
 * same order. Returns true when the hook is installed.
 */
bool patch_render_sort(so_module *mod);
