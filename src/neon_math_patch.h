#pragma once

struct so_module;

/*
 * Replace the game's scalar-VFP Vectormath::Aos::Matrix4 multiplies with NEON
 * bodies of the same arithmetic. Returns the number of functions replaced.
 */
unsigned int patch_neon_matrix_math(so_module *mod);
