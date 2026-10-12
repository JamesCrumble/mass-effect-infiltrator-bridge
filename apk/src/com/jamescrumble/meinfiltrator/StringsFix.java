package com.jamescrumble.meinfiltrator;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;

/**
 * The Russian community patch leaves the table entries NO and YES without text, so every Yes/No button (upgrade and shop
 * confirmations, the mission failed screen) stays blank. This gives them "Нет" and "Да".
 *
 * String table layout (masseffect.bin, SBIN v3), as far as it matters here. Chunks are tag(4) size(4) hash(4) payload.
 * The hash is FNV-1 (offset basis 0x811c9dc5, prime 0x01000193) of the payload; the engine rejects the whole table when
 * a hash does not match - which is why a plain byte edit once made every string show up as XXXXX[KEY]XXXXX.
 *   DATA  pair records, u16 fields: string id item, value item, ... (NO and YES have value item 0, the empty string)
 *   CHDR  array of (offset, length) items
 *   CDAT  the NUL-terminated strings; item offsets are relative to CDAT + 12
 * Two items are appended to CHDR, two strings to CDAT, the value index of the NO and YES records is pointed at them,
 * the chunk sizes are updated and the hashes of DATA, CHDR and CDAT are recomputed.
 * scratch/fixstr.py is the same algorithm in Python (kept in sync by hand; outputs are compared byte for byte).
 */
final class StringsFix {
    private static final Charset UTF8 = Charset.forName("UTF-8");
    private static final String[] KEYS = { "NO", "YES" };
    private static final String[] TEXT = { "Нет", "Да" };

    private StringsFix() { }

    /** Patches one masseffect.bin in place; returns true when the file was changed. */
    static boolean fixFile(File f) throws IOException {
        byte[] d = new byte[(int) f.length()];
        try (FileInputStream in = new FileInputStream(f)) {
            int n = 0;
            while (n < d.length) {
                int r = in.read(d, n, d.length - n);
                if (r < 0) throw new IOException("short read " + f);
                n += r;
            }
        }
        byte[] r = fix(d);
        if (r == null) return false;
        File tmp = new File(f.getPath() + ".tmp");
        try (FileOutputStream out = new FileOutputStream(tmp)) { out.write(r); }
        if (!tmp.renameTo(f)) throw new IOException("cannot replace " + f);
        return true;
    }

    private static int find(byte[] d, byte[] pat, int from, int to) {
        outer:
        for (int i = Math.max(0, from); i + pat.length <= Math.min(d.length, to); i++) {
            for (int k = 0; k < pat.length; k++) if (d[i + k] != pat[k]) continue outer;
            return i;
        }
        return -1;
    }

    private static int u32(byte[] d, int p) { return ByteBuffer.wrap(d, p, 4).order(ByteOrder.LITTLE_ENDIAN).getInt(); }

    private static void put32(byte[] d, int p, int v) { ByteBuffer.wrap(d, p, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(v); }

    private static void put16(byte[] d, int p, int v) { d[p] = (byte) v; d[p + 1] = (byte) (v >> 8); }

    /** FNV-1 as used by the chunk headers. */
    static int fnv1(byte[] d, int from, int len) {
        int h = 0x811c9dc5;
        for (int i = from; i < from + len; i++) h = (h * 0x01000193) ^ (d[i] & 0xff);
        return h;
    }

    /** Recomputes the hash of the chunk whose tag starts at pos (payload length from its size field). */
    private static void rehash(byte[] d, int pos) {
        put32(d, pos + 8, fnv1(d, pos + 12, u32(d, pos + 4)));
    }

    /** Returns the patched table, or null when this is not a table that needs (or allows) the fix. */
    static byte[] fix(byte[] d) {
        int ch = find(d, "CHDR".getBytes(UTF8), 0, d.length);
        int cd = find(d, "CDAT".getBytes(UTF8), 0, d.length);
        int data = find(d, "DATA".getBytes(UTF8), 0, d.length);
        if (ch < 0 || cd < 0 || data < 0) return null;
        int size = u32(d, ch + 4), start = ch + 12;
        if (size <= 0 || start + size != cd) return null;
        int base = cd + 12, cdsize = u32(d, cd + 4);
        if (base + cdsize != d.length) return null;
        // never touch a table whose own hashes do not check out
        if (u32(d, data + 8) != fnv1(d, data + 12, u32(d, data + 4)) || u32(d, ch + 8) != fnv1(d, start, size)
                || u32(d, cd + 8) != fnv1(d, base, cdsize)) return null;
        int n = size / 8;
        int[] idx = { -1, -1 };
        for (int i = 0; i < n; i++) {
            int o = u32(d, start + 8 * i), l = u32(d, start + 8 * i + 4);
            if (l != 2 && l != 3) continue;
            if (base + o + l > d.length) return null;
            String s = new String(d, base + o, l, UTF8);
            for (int k = 0; k < 2; k++) if (idx[k] < 0 && s.equals(KEYS[k])) idx[k] = i;
        }
        if (idx[0] < 0 || idx[1] < 0) return null;
        // the two records read [id, 0, 1, 0] one after the other (the numbers also occur in the table header)
        ByteBuffer pat = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN);
        pat.putShort((short) idx[0]).putShort((short) 0).putShort((short) 1).putShort((short) 0);
        pat.putShort((short) idx[1]).putShort((short) 0).putShort((short) 1).putShort((short) 0);
        int rec = find(d, pat.array(), data, ch);
        if (rec < 0) return null;

        ByteArrayOutputStream blob = new ByteArrayOutputStream();
        ByteBuffer items = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN);
        byte[] out = d.clone();
        int blobEnd = d.length - base;
        for (int k = 0; k < 2; k++) {
            byte[] tb = TEXT[k].getBytes(UTF8);
            items.putInt(blobEnd + blob.size()).putInt(tb.length);
            blob.write(tb, 0, tb.length);
            blob.write(0);
            put16(out, rec + 2 + 8 * k, n + k);
        }
        put32(out, ch + 4, size + 16);
        put32(out, cd + 4, cdsize + blob.size());
        ByteArrayOutputStream res = new ByteArrayOutputStream(out.length + 64);
        res.write(out, 0, cd);
        res.write(items.array(), 0, 16);
        res.write(out, cd, out.length - cd);
        byte[] b = blob.toByteArray();
        res.write(b, 0, b.length);
        byte[] r = res.toByteArray();
        rehash(r, data);
        rehash(r, ch);
        rehash(r, cd + 16);
        return r;
    }

    /** Applies the fix to every language table below game/assets/published/strings. */
    static int fixAll(File stringsDir) {
        int changed = 0;
        File[] langs = stringsDir.listFiles();
        if (langs == null) return 0;
        for (File lang : langs) {
            File bin = new File(lang, "masseffect.bin");
            try {
                if (bin.isFile() && fixFile(bin)) changed++;
            } catch (IOException ignored) {
            }
        }
        return changed;
    }
}
