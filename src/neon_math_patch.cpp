/*
 * NEON bodies for the four Vectormath::Aos::Matrix4 multiplies.
 *
 * A guest profile of a fight on the phone (QEMU_GUEST_PROF, 18 minutes) put
 * these at the top of the game's own code:
 *
 *   17.3%  Matrix4::operator*(const Vector4 &) const     +0x000f7bcc
 *    4.4%  Matrix4::operator*(const Matrix4 &) const     +0x000f7c70
 *    3.6%  Matrix4::transform(Vector4 &) const           +0x000b7efc
 *    1.7%  Matrix4::operator*=(const Matrix4 &)          +0x003569dc
 *
 * The library is an ARMv5TE/VFP build, so each of them is scalar: 41
 * instructions for one matrix * vector, and the matrix * matrix forms call that
 * four times and copy the result through the stack. The replacements do one
 * column per instruction: 10 instructions for matrix * vector, 26 for
 * matrix * matrix.
 *
 * Same arithmetic, not a faster approximation. The original computes
 *     out = c1*y;  out += c0*x;  out += c2*z;  out += c3*w
 * with VMUL/VMLA, which on VFPv3 is multiply-then-add with both steps rounded.
 * NEON VMLA is the same two rounded steps, and the bodies keep the same order,
 * so for ordinary values the results are bit-identical: 400000 random rounds of
 * each function, with every aliasing the originals tolerate (out == vector,
 * out == either matrix), against the original bytes run from a copy, 0
 * mismatches. They differ only where NEON's fixed "standard FPSCR" differs from
 * VFP: a denormal flushes to zero and a NaN loses its payload.
 *
 * Registers: the bodies clobber only what the originals did (s8-s15 for the
 * vector forms, s0-s15 for the matrix forms) plus d16-d31 and ip, which no
 * caller can hold across a call. r1/r2 are left alone, r0 is returned as the
 * originals return it.
 *
 * Whether this is a gain is a property of the CPU underneath, so it is
 * measured, not assumed. On real ARM hardware it plainly is. Under our
 * qemu the answer depends on one thing: its fast NEON path runs with the host
 * FPCR switched to FZ+DN and its fast VFP path with the default FPCR, lazily,
 * so NEON code called from VFP code pays two FPCR writes per call - and that
 * write is expensive on Oryon. Stock qemu, with softfloat NEON, is slower with
 * these bodies. So the default is to time original against NEON at startup,
 * called the way the game calls them (from scalar VFP code), separately for the
 * vector forms and the matrix forms, and install each only on a clear win.
 * MASSEFFECT_NEON_MATH=0 never installs, =1 always does.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/auxv.h>
#include <sys/mman.h>

#include "so_util.h"
#include "trace.h"
#include "platform.h"
#include "neon_math_patch.h"

/* vld1.32 {d16-d19},[rM]; add ip,rM,#32; vld1.32 {d20-d23},[ip]; vld1.32 {d4-d5},[rV];
 * vmul.f32 q3,q9,d4[1]; vmla.f32 q3,q8,d4[0]; vmla.f32 q3,q10,d5[0]; vmla.f32 q3,q11,d5[1];
 * vst1.32 {d6-d7},[rOut]; bx lr */
static const uint32_t kMulVector[] = {   /* r0 = out, r1 = M, r2 = v */
    0xf461028f, 0xe281c020, 0xf46c428f, 0xf4224a8f, 0xf3a269e4,
    0xf3a061c4, 0xf3a461c5, 0xf3a661e5, 0xf4006a8f, 0xe12fff1e,
};
static const uint32_t kTransform[] = {   /* r0 = M, r1 = v (in and out) */
    0xf460028f, 0xe280c020, 0xf46c428f, 0xf4214a8f, 0xf3a269e4,
    0xf3a061c4, 0xf3a461c5, 0xf3a661e5, 0xf4016a8f, 0xe12fff1e,
};
/* A's columns in q8-q11, B's in q0-q3, the four result columns in q12-q15 and
 * stored only at the end, so out may be A or B. */
static const uint32_t kMulMatrix[] = {   /* r0 = out, r1 = A, r2 = B */
    0xf461028f, 0xe281c020, 0xf46c428f, 0xf422028f, 0xe282c020, 0xf42c428f,
    0xf3e289e0, 0xf3e081c0, 0xf3e481c1, 0xf3e681e1,
    0xf3e2a9e2, 0xf3e0a1c2, 0xf3e4a1c3, 0xf3e6a1e3,
    0xf3e2c9e4, 0xf3e0c1c4, 0xf3e4c1c5, 0xf3e6c1e5,
    0xf3e2e9e6, 0xf3e0e1c6, 0xf3e4e1c7, 0xf3e6e1e7,
    0xf440828f, 0xe280c020, 0xf44cc28f, 0xe12fff1e,
};
static const uint32_t kMulAssign[] = {   /* r0 = this (A and out), r1 = B */
    0xf460028f, 0xe280c020, 0xf46c428f, 0xf421028f, 0xe281c020, 0xf42c428f,
    0xf3e289e0, 0xf3e081c0, 0xf3e481c1, 0xf3e681e1,
    0xf3e2a9e2, 0xf3e0a1c2, 0xf3e4a1c3, 0xf3e6a1e3,
    0xf3e2c9e4, 0xf3e0c1c4, 0xf3e4c1c5, 0xf3e6c1e5,
    0xf3e2e9e6, 0xf3e0e1c6, 0xf3e4e1c7, 0xf3e6e1e7,
    0xf440828f, 0xe280c020, 0xf44cc28f, 0xe12fff1e,
};

struct MathPatch {
    uint32_t        offset;      /* from text_base */
    uint32_t        size;        /* of the original function */
    uint32_t        expect[2];   /* its first two instructions */
    const uint32_t *body;
    uint32_t        words;
    const char     *name;
};

static const MathPatch kMath[] = {
    {0x000f7bcc, 164, {0xed926a01, 0xedd15a07}, kMulVector, 10, "Matrix4 * Vector4"},
    {0x000b7efc, 164, {0xedd05a06, 0xedd17a01}, kTransform, 10, "Matrix4::transform"},
    {0x000f7c70, 216, {0xe92d4070, 0xe24dd040}, kMulMatrix, 26, "Matrix4 * Matrix4"},
    {0x003569dc, 220, {0xe92d4030, 0xe1a04000}, kMulAssign, 26, "Matrix4 *= Matrix4"},
};

typedef void (*MulFn)(float *out, const float *a, const float *b);

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

/*
 * One timing pass over a three-pointer multiply (b is a vector or a matrix).
 * The scalar add between calls is the point of it: the game calls these from
 * scalar VFP code, and whatever the CPU underneath charges for going from VFP
 * to NEON and back has to be in the number.
 */
static double time_calls(MulFn fn, int rounds)
{
    static const float m[16] = {
        0.5f, 0.1f, -0.2f, 0.f, 0.3f, 0.9f, 0.1f, 0.f,
        -0.1f, 0.2f, 0.8f, 0.f, 4.f, -2.f, 7.f, 1.f,
    };
    float b[16], out[16];
    memcpy(b, m, sizeof b);
    volatile float acc = 0.f;

    const double t0 = now_ms();
    for (int i = 0; i < rounds; i++) {
        fn(out, m, b);
        acc = acc + out[0] * 0.001f;
        b[0] = acc;
    }
    return now_ms() - t0;
}

static bool faster(MulFn orig, MulFn neon, const char *what)
{
    const int kRounds = 20000;
    double t_orig = 1e30, t_neon = 1e30;
    time_calls(orig, 2000);    /* translate both before timing */
    time_calls(neon, 2000);
    for (int pass = 0; pass < 3; pass++) {
        const double a = time_calls(orig, kRounds);
        const double b = time_calls(neon, kRounds);
        if (a < t_orig) t_orig = a;
        if (b < t_neon) t_neon = b;
    }
    const bool yes = t_neon < t_orig * 0.85;
    fprintf(stderr, "neon math: %s x%d: original %.2f ms, NEON %.2f ms -> %s\n",
            what, kRounds, t_orig, t_neon, yes ? "NEON" : "original kept");
    return yes;
}

/*
 * Times the two shapes separately, because they answer differently. A lone
 * matrix * vector trades 41 scalar instructions for 10 NEON ones plus the mode
 * switch; a matrix * matrix trades four such calls and a copy through the
 * stack for 26 instructions and one switch.
 *
 * The originals are run from a copy. Matrix * vector and matrix * matrix are
 * adjacent in the library and the second reaches the first by a relative
 * branch, so copying the two as one block keeps both runnable.
 */
static void measure(const so_module *mod, bool *vector_wins, bool *matrix_wins)
{
    *vector_wins = *matrix_wins = false;
    const MathPatch &v = kMath[0], &m = kMath[2];
    const size_t block = m.offset + m.size - v.offset;
    void *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED || block > 1024) {
        trace("neon math: no page for the timing run, left off");
        return;
    }

    uint8_t *orig = (uint8_t *)page, *neon_v = orig + 1024, *neon_m = orig + 2048;
    memcpy(orig, (const void *)(mod->text_base + v.offset), block);
    memcpy(neon_v, v.body, v.words * 4);
    memcpy(neon_m, m.body, m.words * 4);
    __builtin___clear_cache((char *)page, (char *)page + 4096);

    *vector_wins = faster((MulFn)orig, (MulFn)neon_v, "matrix*vector");
    *matrix_wins = faster((MulFn)(orig + (m.offset - v.offset)), (MulFn)neon_m, "matrix*matrix");
    munmap(page, 4096);
}

unsigned int patch_neon_matrix_math(so_module *mod)
{
    const char *mode = getenv("MASSEFFECT_NEON_MATH");
    if (mode && !strcmp(mode, "0")) {
        trace("neon math: off (MASSEFFECT_NEON_MATH=0)");
        return 0;
    }
#ifdef __arm__
    if (!(getauxval(AT_HWCAP) & (1UL << 12))) {    /* HWCAP_NEON */
        trace("neon math: this CPU has no NEON, left off");
        return 0;
    }
#else
    return 0;
#endif

    for (const MathPatch &p : kMath) {
        const uint32_t *at = (const uint32_t *)(mod->text_base + p.offset);
        if (at[0] != p.expect[0] || at[1] != p.expect[1]) {
            warning("neon math: %s at +0x%08x is not the expected code "
                    "(%08x %08x) - nothing replaced.\n",
                    p.name, p.offset, at[0], at[1]);
            return 0;
        }
    }

    /* kMath order: the two vector forms, then the two matrix forms. */
    bool vector_wins = true, matrix_wins = true;
    const bool forced = mode && !strcmp(mode, "1");
    if (forced)
        fprintf(stderr, "neon math: installed without timing (MASSEFFECT_NEON_MATH=1)\n");
    else
        measure(mod, &vector_wins, &matrix_wins);

    unsigned int done = 0;
    for (unsigned i = 0; i < sizeof(kMath) / sizeof(kMath[0]); i++) {
        const MathPatch &p = kMath[i];
        if (!(i < 2 ? vector_wins : matrix_wins))
            continue;
        memcpy((void *)(mod->text_base + p.offset), p.body, p.words * 4);
        trace("neon math: replaced %s at +0x%08x (%u -> %u instructions)",
              p.name, p.offset, p.size / 4, p.words);
        done++;
    }
    return done;
}
