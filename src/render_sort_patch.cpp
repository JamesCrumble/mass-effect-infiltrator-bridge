/*
 * A transitive m3g::SortedRenderNode::operator<.
 *
 * The crash this answers (phone, 18 minutes into a fight):
 *
 *   SIGSEGV, pc = libMassEffect.so+0x00a580a0   (data: typeinfo for AppearanceBase)
 *   lr = +0x003f0e04  m3g::Renderer::RenderRenderNode, after `ldr pc, [vtable, #0x5c]`
 *
 * The render list held, where a node belongs, a pointer to memory that had since
 * become an m3g::Appearance: a stale list entry. Stale entries are not rare -
 * the list is a vector that is cleared and refilled every frame, so everything
 * between end() and capacity is last frame's entries, pointers and all. They
 * are harmless as long as nothing reads past end().
 *
 * The engine's comparator makes eastl::quick_sort do exactly that. As shipped:
 *
 *   1. layer (+12), signed
 *   2. sort key (+16): 68 before everything else, other keys equal to each other
 *   3. if BOTH entries carry the flag at +20 and their vertex buffers (+32)
 *      differ: by vertex buffer
 *   4. otherwise by appearance (+24): texture 0, texture 1, +0x5c, +0x34, +0x30
 *   5. vertex buffer, then +36
 *
 * Step 3 applies to a pair, not to an entry, so the order is not transitive:
 * with A and B flagged and C not, A < B is decided by vertex buffer while B < C
 * and C < A are decided by appearance, and all three can hold at once.
 * eastl's get_partition scans with `while (*first < pivot) ++first` and no
 * bounds check, which is sound only for a strict weak order.
 *
 * Measured rather than argued: the game's own quick_sort and operator<, run
 * from a copy of the library's bytes on lists shaped like the engine's with
 * stale entries after the end, wrote outside the list in 11 of 20000 sorts
 * (up to 90 entries past it) and each time left a stale entry inside the list;
 * with no flagged entries, 0 of 20000. With the comparator below: 0 of 20000,
 * every list sorted.
 *
 * The fix is one added step between 2 and 3: unflagged entries before flagged
 * ones. Every pair with equal flags compares exactly as before (2 million
 * random pairs, no disagreement); only mixed pairs, whose relative order was an
 * accident of texture addresses, are affected.
 *
 * That this defect is what produced that particular crash is an inference from
 * the crash's shape, not something the crash log proves.
 *
 * MASSEFFECT_NO_SORT_FIX=1 leaves the engine's comparator in place.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "so_util.h"
#include "trace.h"
#include "platform.h"
#include "render_sort_patch.h"

struct SortedRenderNode {
    void    *node;
    void    *transform;
    void    *unknown8;
    int32_t  layer;
    int32_t  key;
    uint8_t  flag;
    uint8_t  pad[3];
    const uint32_t *appearance;
    void    *indexBuffer;
    uintptr_t vertexBuffer;
    uint32_t  unknown36;
};

/* The key value the engine gives an entry whose appearance sets none. */
static const int32_t kDefaultKey = 68;

/* Appearance fields the engine's comparator reads, as word indices. */
enum { kTexture0 = 0x40 / 4, kTexture1 = 0x44 / 4, kField5c = 0x5c / 4,
       kField34 = 0x34 / 4, kField30 = 0x30 / 4 };

extern "C" int render_sort_less(const SortedRenderNode *a,
                                const SortedRenderNode *b)
{
    if (a->layer != b->layer)
        return a->layer < b->layer;
    if (a->key != b->key && (a->key == kDefaultKey) != (b->key == kDefaultKey))
        return a->key == kDefaultKey;

    if (a->flag != b->flag)             /* the added step */
        return !a->flag;
    if (a->flag && a->vertexBuffer != b->vertexBuffer)
        return a->vertexBuffer < b->vertexBuffer;

    if (a->appearance != b->appearance) {
        const uint32_t *x = a->appearance, *y = b->appearance;
        static const int kOrder[] = {kTexture0, kTexture1, kField5c, kField34, kField30};
        for (int i : kOrder)
            if (x[i] != y[i])
                return x[i] < y[i];
    }
    if (a->vertexBuffer != b->vertexBuffer)
        return a->vertexBuffer < b->vertexBuffer;
    return a->unknown36 < b->unknown36;
}

bool patch_render_sort(so_module *mod)
{
    static const uint32_t kOffset = 0x003f0f30;
    static const uint32_t kExpect[] = {0xe92d41f0, 0xe590200c, 0xe591300c};

    const char *off = getenv("MASSEFFECT_NO_SORT_FIX");
    if (off && *off && *off != '0') {
        trace("render sort: engine comparator kept (MASSEFFECT_NO_SORT_FIX)");
        return false;
    }
    if (sizeof(SortedRenderNode) != 40) {
        warning("render sort: SortedRenderNode is %u bytes here, not 40 - not patched.\n",
                (unsigned)sizeof(SortedRenderNode));
        return false;
    }
    const uint32_t *at = (const uint32_t *)(mod->text_base + kOffset);
    for (unsigned i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
        if (at[i] != kExpect[i]) {
            warning("render sort: +0x%08x is not SortedRenderNode::operator< "
                    "(found %08x) - not patched.\n", kOffset + i * 4, at[i]);
            return false;
        }
    }
    hook_address(mod, mod->text_base + kOffset, (uintptr_t)&render_sort_less);
    fprintf(stderr, "render sort: SortedRenderNode::operator< replaced with a transitive order\n");
    return true;
}
