package com.rinnejay.kopiide;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

/**
 * Jembatan JS ↔ Android.
 *
 * Tata letak berkas:
 *   files/lib/libkopicc.so, libkopivm.so   ← salinan biner compiler & VM
 *   files/tmp/                             ← scratch eksekusi
 *   /storage/emulated/0/KopiProject/       ← proyek pengguna
 *        <nama>/src/*.kop, <nama>/build/, <nama>/out/*.kbc
 *
 * Eksekusi: app ini sengaja targetSdk 28 (seperti Termux) supaya Android ≥10
 * mengizinkan exec dari data dir. Biner disalin ke files/lib dan, bila
 * PackageManager tidak mengekstrak jniLibs, diambil langsung dari dalam APK.
 * nativeLibraryDir tetap dipakai sebagai kandidat kedua.
 */
public class IdeBridge {

    private static final String VERSION = "1.0.3";
    private static final String DEFAULT_PROJECTS = "/storage/emulated/0/KopiProject";

    private final Activity act;
    private final WebView web;
    private final File filesDir, libDir, tmpDir, projectsRoot;
    private final ExecutorService pool = Executors.newSingleThreadExecutor();

    private volatile Process running;
    private volatile boolean stopping;

    public IdeBridge(Activity a, WebView w) {
        act = a;
        web = w;
        filesDir = a.getFilesDir();
        libDir = new File(filesDir, "lib");
        tmpDir = new File(filesDir, "tmp");
        projectsRoot = new File(DEFAULT_PROJECTS);
        libDir.mkdirs();
        tmpDir.mkdirs();
        projectsRoot.mkdirs();
        syncLib("kopicc");
        syncLib("kopivm");
        syncKopLib();
    }

    /* ───────────────────────── util ───────────────────────── */

    private static String json(String s) {
        try {
            return JSONObject.quote(s);
        } catch (Exception e) {
            return "\"\"";
        }
    }

    private static String ok(String msg) {
        try {
            JSONObject o = new JSONObject();
            o.put("ok", true);
            o.put("msg", msg);
            return o.toString();
        } catch (Exception e) {
            return "{\"ok\":true,\"msg\":\"ok\"}";
        }
    }

    private static String err(String msg) {
        try {
            JSONObject o = new JSONObject();
            o.put("ok", false);
            o.put("err", true);
            o.put("msg", msg);
            return o.toString();
        } catch (Exception e) {
            return "{\"ok\":false,\"msg\":\"error\"}";
        }
    }

    private String nativeLib(String name) {
        return act.getApplicationInfo().nativeLibraryDir + "/lib" + name + ".so";
    }

    private void makeExec(File f) {
        f.setExecutable(true, false);
        f.setReadable(true, false);
    }

    private static void copyStream(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[65536];
        int n;
        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        in.close();
        out.close();
    }

    /**
     * Salin biner ke files/lib. Sumber pertama nativeLibraryDir (hasil
     * ekstraksi PackageManager); bila kosong, baca langsung dari APK.
     */
    private void syncLib(String name) {
        File dst = new File(libDir, "lib" + name + ".so");
        try {
            File src = new File(nativeLib(name));
            if (src.isFile()) {
                if (!dst.isFile() || dst.length() != src.length()) {
                    copyStream(new FileInputStream(src), new FileOutputStream(dst));
                }
            } else if (!dst.isFile()) {
                extractFromApk(name, dst);
            }
        } catch (Exception ignored) {
        }
        if (dst.isFile()) makeExec(dst);
    }

    /** Ambil lib/<abi>/lib<name>.so dari dalam APK (tanpa butuh ekstraksi). */
    private boolean extractFromApk(String name, File dst) {
        ZipFile z = null;
        try {
            File apk = new File(act.getApplicationInfo().sourceDir);
            z = new ZipFile(apk);
            Enumeration<? extends ZipEntry> en = z.entries();
            String want = "/lib" + name + ".so";
            while (en.hasMoreElements()) {
                ZipEntry e = en.nextElement();
                String n = e.getName();
                if (n.startsWith("lib/") && n.endsWith(want)) {
                    copyStream(z.getInputStream(e), new FileOutputStream(dst));
                    makeExec(dst);
                    return true;
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (z != null) try { z.close(); } catch (Exception ignored) { }
        }
        return false;
    }

    /**
     * Ekstrak modul bahasa (lib/core/*.kop, lib/net/*.kop, ...) dari
     * assets/kopilib.zip ke files/lib/lib. kopicc mencari import di
     * <dir biner>/lib; dengan biner di files/lib, hasilnya files/lib/lib.
     */
    private void syncKopLib() {
        File dest = new File(libDir, "lib");
        File marker = new File(dest, ".kopilib");
        try {
            String have = "";
            if (marker.isFile()) {
                byte[] b = new byte[(int) marker.length()];
                FileInputStream in = new FileInputStream(marker);
                int off = 0, n;
                while (off < b.length && (n = in.read(b, off, b.length - off)) > 0) off += n;
                in.close();
                have = new String(b, 0, off, StandardCharsets.UTF_8).trim();
            }
            if (have.equals(VERSION)) return;

            rmrf(dest);
            dest.mkdirs();
            String base = dest.getCanonicalPath() + File.separator;

            ZipInputStream zin = new ZipInputStream(act.getAssets().open("kopilib.zip"));
            ZipEntry e;
            byte[] buf = new byte[65536];
            while ((e = zin.getNextEntry()) != null) {
                if (e.isDirectory()) continue;
                File out = new File(dest, e.getName());
                if (!out.getCanonicalPath().startsWith(base)) continue;
                out.getParentFile().mkdirs();
                FileOutputStream fo = new FileOutputStream(out);
                int n;
                while ((n = zin.read(buf)) > 0) fo.write(buf, 0, n);
                fo.close();
            }
            zin.close();

            FileOutputStream mo = new FileOutputStream(marker);
            mo.write(VERSION.getBytes(StandardCharsets.UTF_8));
            mo.close();
        } catch (Exception ignored) {
        }
    }

    /** jumlah modul .kop yang berhasil diekstrak (untuk info/diagnostik) */
    private int countKop(File dir) {
        int n = 0;
        File kids[] = dir.listFiles();
        if (kids == null) return 0;
        for (File f : kids) {
            if (f.isDirectory()) n += countKop(f);
            else if (f.getName().endsWith(".kop")) n++;
        }
        return n;
    }

    /** salinan biner di files/lib */
    private String fileLib(String name) {
        return new File(libDir, "lib" + name + ".so").getAbsolutePath();
    }

    /** path nativeLibraryDir (hasil ekstraksi jniLibs) */
    private String execPath(String name) {
        File f = new File(fileLib(name));
        if (f.isFile()) return f.getAbsolutePath();
        return nativeLib(name);
    }

    private int targetSdk() {
        try {
            return act.getPackageManager().getPackageInfo(
                    act.getPackageName(), 0).applicationInfo.targetSdkVersion;
        } catch (Exception e) {
            return -1;
        }
    }

    private File resolve(String path) throws IOException {
        File f = new File(path);
        String real = f.getCanonicalPath();
        String roots[] = {projectsRoot.getCanonicalPath(), filesDir.getCanonicalPath()};
        for (String r : roots) {
            if (real.equals(r) || real.startsWith(r + File.separator)) return new File(real);
        }
        throw new IOException("di luar folder yang diizinkan: " + path);
    }

    private void emit(String js) {
        web.post(new Runnable() {
            @Override
            public void run() {
                try {
                    web.evaluateJavascript(js, null);
                } catch (Exception ignored) {
                }
            }
        });
    }

    /* ───────────────────── info & izin ───────────────────── */

    @JavascriptInterface
    public String info() {
        try {
            JSONObject o = new JSONObject();
            o.put("version", VERSION);
            o.put("projects", projectsRoot.getAbsolutePath());
            o.put("libDir", libDir.getAbsolutePath());
            o.put("tmpDir", tmpDir.getAbsolutePath());
            o.put("storage", hasStorage());
            o.put("sdk", Build.VERSION.SDK_INT);
            o.put("targetSdk", targetSdk());
            o.put("nativeDir", act.getApplicationInfo().nativeLibraryDir);
            o.put("kopLib", new File(libDir, "lib").getAbsolutePath());
            o.put("kopModules", countKop(new File(libDir, "lib")));
            JSONArray libs = new JSONArray();
            String names[] = {"kopicc", "kopivm"};
            for (String n : names) {
                JSONObject l = new JSONObject();
                File f = new File(fileLib(n));
                File nat = new File(nativeLib(n));
                l.put("name", n);
                l.put("exists", f.isFile());
                l.put("size", f.length());
                l.put("exec", execPath(n));
                l.put("nativeExists", nat.isFile());
                l.put("nativePath", nat.getAbsolutePath());
                l.put("canExec", f.isFile() && f.canExecute());
                libs.put(l);
            }
            o.put("libs", libs);
            return o.toString();
        } catch (Exception e) {
            return err("info gagal: " + e.getMessage());
        }
    }

    /** Uji eksekusi kedua biner; dipakai tombol "Tes" di IDE. */
    @JavascriptInterface
    public String selfTest() {
        try {
            JSONObject o = new JSONObject();
            o.put("targetSdk", targetSdk());
            o.put("sdk", Build.VERSION.SDK_INT);
            o.put("nativeDir", act.getApplicationInfo().nativeLibraryDir);
            JSONArray arr = new JSONArray();
            String names[] = {"kopicc", "kopivm"};
            for (String n : names) {
                JSONObject l = new JSONObject();
                l.put("name", n);
                List<String> args = n.equals("kopicc")
                        ? Arrays.asList("--version") : new ArrayList<String>();
                try {
                    Process p = startIn(filesDir, n, args);
                    String out = slurp(p).trim();
                    int code = p.waitFor();
                    l.put("ok", out.length() > 0);
                    l.put("code", code);
                    l.put("out", out.length() > 400 ? out.substring(0, 400) : out);
                } catch (Exception e) {
                    l.put("ok", false);
                    l.put("out", String.valueOf(e.getMessage()));
                }
                arr.put(l);
            }
            o.put("libs", arr);
            o.put("kopLib", new File(libDir, "lib").getAbsolutePath());
            o.put("kopModules", countKop(new File(libDir, "lib")));

            /* uji end-to-end: import Str; (butuh lib/core/Str.kop) lalu run */
            JSONObject e2e = new JSONObject();
            try {
                File src = new File(tmpDir, "selftest.kop");
                File kbc = new File(tmpDir, "selftest.kbc");
                String code = "import Str;\n\nclass Main {\n"
                        + "    public static void main(String[] args) {\n"
                        + "        System.out.println(`selftest {2 + 3}`);\n"
                        + "    }\n}\n";
                FileOutputStream fo = new FileOutputStream(src);
                fo.write(code.getBytes(StandardCharsets.UTF_8));
                fo.close();
                Process p = startIn(tmpDir, "kopicc", Arrays.asList(
                        src.getAbsolutePath(), "-t", "vm", "-o", kbc.getAbsolutePath()));
                String blog = slurp(p).trim();
                int bcode = p.waitFor();
                e2e.put("buildCode", bcode);
                e2e.put("buildLog", blog.length() > 800 ? blog.substring(0, 800) : blog);
                boolean ok = bcode == 0 && kbc.isFile();
                if (ok) {
                    Process q = startIn(tmpDir, "kopivm", Arrays.asList(kbc.getAbsolutePath()));
                    String rlog = slurp(q).trim();
                    int rcode = q.waitFor();
                    e2e.put("runCode", rcode);
                    e2e.put("runLog", rlog.length() > 400 ? rlog.substring(0, 400) : rlog);
                    ok = rcode == 0 && rlog.contains("selftest 5");
                }
                e2e.put("ok", ok);
            } catch (Exception e) {
                e2e.put("ok", false);
                e2e.put("runLog", String.valueOf(e.getMessage()));
            }
            o.put("e2e", e2e);
            return o.toString();
        } catch (Exception e) {
            return err("self-test gagal: " + e.getMessage());
        }
    }

    @JavascriptInterface
    public boolean hasStorage() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                return Environment.isExternalStorageManager();
            } catch (Throwable t) {
                return false;
            }
        }
        return act.checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    @JavascriptInterface
    public void requestStorage() {
        try {
            if (Build.VERSION.SDK_INT >= 30) {
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                i.setData(Uri.parse("package:" + act.getPackageName()));
                try {
                    act.startActivity(i);
                } catch (Exception e) {
                    act.startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                }
            } else {
                act.requestPermissions(new String[]{
                        Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1001);
            }
        } catch (Exception ignored) {
        }
    }

    @JavascriptInterface
    public void exitApp() {
        act.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                act.finish();
            }
        });
    }

    /* ───────────────────── berkas ───────────────────── */

    @JavascriptInterface
    public String listDir(String path) {
        try {
            File dir = resolve(path);
            if (!dir.isDirectory()) return err("bukan folder: " + path);
            File kids[] = dir.listFiles();
            if (kids == null) kids = new File[0];
            Arrays.sort(kids, new java.util.Comparator<File>() {
                @Override
                public int compare(File a, File b) {
                    if (a.isDirectory() != b.isDirectory()) return a.isDirectory() ? -1 : 1;
                    return a.getName().compareToIgnoreCase(b.getName());
                }
            });
            JSONArray arr = new JSONArray();
            for (File f : kids) {
                if (f.getName().startsWith(".")) continue;
                JSONObject o = new JSONObject();
                o.put("n", f.getName());
                o.put("d", f.isDirectory() ? 1 : 0);
                o.put("s", f.length());
                o.put("p", f.getAbsolutePath());
                arr.put(o);
            }
            return arr.toString();
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    @JavascriptInterface
    public String readFile(String path) {
        try {
            File f = resolve(path);
            if (!f.isFile()) return err("bukan berkas: " + path);
            if (f.length() > 4 * 1024 * 1024) return err("berkas terlalu besar (>4 MB)");
            FileInputStream in = new FileInputStream(f);
            byte[] b = new byte[(int) f.length()];
            int off = 0, n;
            while (off < b.length && (n = in.read(b, off, b.length - off)) > 0) off += n;
            in.close();
            return new String(b, 0, off, StandardCharsets.UTF_8);
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    @JavascriptInterface
    public String writeFile(String path, String content) {
        try {
            File f = resolve(path);
            File p = f.getParentFile();
            if (p != null) p.mkdirs();
            FileOutputStream out = new FileOutputStream(f);
            out.write(content.getBytes(StandardCharsets.UTF_8));
            out.close();
            return ok("tersimpan");
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    @JavascriptInterface
    public String mkdirs(String path) {
        try {
            File f = resolve(path);
            return f.mkdirs() || f.isDirectory() ? ok("ok") : err("gagal membuat folder");
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    @JavascriptInterface
    public String delete(String path) {
        try {
            File f = resolve(path);
            if (f.getAbsolutePath().equals(projectsRoot.getAbsolutePath()) ||
                f.getAbsolutePath().equals(libDir.getAbsolutePath()))
                return err("folder ini tidak boleh dihapus");
            return rmrf(f) ? ok("terhapus") : err("gagal menghapus");
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    private boolean rmrf(File f) {
        if (f.isDirectory()) {
            File kids[] = f.listFiles();
            if (kids != null) for (File k : kids) rmrf(k);
        }
        return f.delete();
    }

    @JavascriptInterface
    public String rename(String from, String to) {
        try {
            File a = resolve(from), b = resolve(to);
            b.getParentFile().mkdirs();
            return a.renameTo(b) ? ok("diubah nama") : err("gagal mengubah nama");
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    @JavascriptInterface
    public String createProject(String name) {
        try {
            String clean = name.trim().replaceAll("[^A-Za-z0-9._-]", "_");
            if (clean.isEmpty()) return err("nama proyek kosong");
            File proj = new File(projectsRoot, clean);
            File src = new File(proj, "src");
            File build = new File(proj, "build");
            File out = new File(proj, "out");
            src.mkdirs();
            build.mkdirs();
            out.mkdirs();
            File main = new File(src, "Main.kop");
            if (!main.exists()) {
                String tpl = "// " + clean + " — proyek Kopi\n"
                        + "// Build: tombol Build (kopicc -t vm), lalu Run.\n"
                        + "import Str;\n"
                        + "\n"
                        + "class Main {\n"
                        + "    public static void main(String[] args) {\n"
                        + "        System.out.println(\"halo dari " + clean + "\");\n"
                        + "        malloc {\n"
                        + "            int[] buf = new int[8];\n"
                        + "            for (int i = 0; i < 8; i++) buf[i] = i * i;\n"
                        + "            int s = 0;\n"
                        + "            for (int i = 0; i < 8; i++) s = s + buf[i];\n"
                        + "            System.out.println(`jumlah kuadrat 0..7 = {s}`);\n"
                        + "        }\n"
                        + "    }\n"
                        + "}\n";
                FileOutputStream o = new FileOutputStream(main);
                o.write(tpl.getBytes(StandardCharsets.UTF_8));
                o.close();
            }
            JSONObject r = new JSONObject();
            r.put("ok", true);
            r.put("project", proj.getAbsolutePath());
            r.put("main", main.getAbsolutePath());
            return r.toString();
        } catch (Exception e) {
            return err(e.getMessage());
        }
    }

    /* ───────────────────── build & run ───────────────────── */

    private static String baseName(String path) {
        int i = path.lastIndexOf('/');
        String n = i >= 0 ? path.substring(i + 1) : path;
        int d = n.lastIndexOf('.');
        return d > 0 ? n.substring(0, d) : n;
    }

    /**
     * Jalankan biner. Kandidat dicoba berurutan: files/lib (lokasi utama,
     * legal karena targetSdk 28) lalu nativeLibraryDir. Bila semua gagal,
     * pesan error memuat path, status berkas, dan targetSdk agar mudah
     * didiagnosis di perangkat.
     */
    private Process startIn(File dir, String name, List<String> args) throws IOException {
        String[] cands = {fileLib(name), nativeLib(name)};
        StringBuilder why = new StringBuilder();
        for (String path : cands) {
            File f = new File(path);
            if (!f.isFile()) {
                if (why.length() > 0) why.append(" | ");
                why.append(path).append(": tidak ada");
                continue;
            }
            List<String> cmd = new ArrayList<>();
            cmd.add(path);
            cmd.addAll(args);
            try {
                return build(cmd, dir).start();
            } catch (IOException e) {
                if (why.length() > 0) why.append(" | ");
                why.append(path).append(": ").append(e.getMessage());
            }
        }
        throw new IOException("tidak bisa menjalankan " + name
                + " (targetSdk=" + targetSdk() + ", sdk=" + Build.VERSION.SDK_INT + "): " + why);
    }

    private ProcessBuilder build(List<String> cmd, File dir) {
        ProcessBuilder pb = new ProcessBuilder(cmd);
        pb.directory(dir);
        pb.redirectErrorStream(true);
        pb.environment().put("TMPDIR", tmpDir.getAbsolutePath());
        pb.environment().put("HOME", filesDir.getAbsolutePath());
        // kopicc memakai KOPPATH lebih dulu untuk mencari import
        pb.environment().put("KOPPATH", new File(libDir, "lib").getAbsolutePath());
        return pb;
    }

    private String slurp(Process p) {
        StringBuilder sb = new StringBuilder();
        try {
            BufferedReader r = new BufferedReader(new InputStreamReader(
                    p.getInputStream(), StandardCharsets.UTF_8));
            String line;
            while ((line = r.readLine()) != null) {
                sb.append(line).append('\n');
                if (sb.length() > 200000) break;
            }
            r.close();
        } catch (Exception ignored) {
        }
        return sb.toString();
    }

    /** Build berkas .kop aktif → out/<nama>.kbc (VM). */
    @JavascriptInterface
    public String compile(String projDir, String srcRel) {
        try {
            File proj = resolve(projDir);
            File src = srcRel.startsWith("/") ? resolve(srcRel) : resolve(
                    new File(proj, srcRel).getAbsolutePath());
            File outDir = new File(proj, "out");
            outDir.mkdirs();
            File kbc = new File(outDir, baseName(src.getPath()) + ".kbc");

            long t0 = System.currentTimeMillis();
            Process p = startIn(proj, "kopicc", Arrays.asList(
                    src.getAbsolutePath(), "-t", "vm", "-o", kbc.getAbsolutePath()));
            String log = slurp(p);
            int code = p.waitFor();
            long ms = System.currentTimeMillis() - t0;

            JSONObject r = new JSONObject();
            r.put("ok", code == 0);
            r.put("code", code);
            r.put("ms", ms);
            r.put("log", log);
            r.put("kbc", kbc.getAbsolutePath());
            r.put("src", src.getAbsolutePath());
            return r.toString();
        } catch (Exception e) {
            return err("build gagal: " + e.getMessage());
        }
    }

    /** Jalankan .kbc di VM; output dialirkan ke IDE.onOutput / onExit. */
    @JavascriptInterface
    public String run(String projDir, String kbcRel) {
        try {
            File proj = resolve(projDir);
            File kbc = kbcRel.startsWith("/") ? resolve(kbcRel) : resolve(
                    new File(proj, kbcRel).getAbsolutePath());
            if (!kbc.isFile()) return err("belum ada hasil build: " + kbc.getPath());

            stop();

            final File projDirF = proj;
            final File kbcF = kbc;
            stopping = false;
            final String label = kbc.getName();
            pool.execute(new Runnable() {
                @Override
                public void run() {
                    try {
                        Process p = startIn(projDirF, "kopivm",
                                Arrays.asList(kbcF.getAbsolutePath()));
                        running = p;
                        emit("IDE.onRunStart(" + json(label) + ")");
                        BufferedReader r = new BufferedReader(new InputStreamReader(
                                p.getInputStream(), StandardCharsets.UTF_8));
                        String line;
                        while ((line = r.readLine()) != null) {
                            emit("IDE.onOutput(" + json(line + "\n") + ")");
                        }
                        int code = p.waitFor();
                        running = null;
                        emit("IDE.onExit(" + code + "," + (stopping ? "true" : "false") + ")");
                    } catch (Exception e) {
                        running = null;
                        emit("IDE.onOutput(" + json("gagal menjalankan VM: " + e.getMessage() + "\n") + ")");
                        emit("IDE.onExit(-1,false)");
                    }
                }
            });
            return ok("berjalan");
        } catch (Exception e) {
            return err("run gagal: " + e.getMessage());
        }
    }

    @JavascriptInterface
    public void stop() {
        stopping = true;
        Process p = running;
        if (p != null) {
            try {
                p.destroy();
            } catch (Exception ignored) {
            }
            running = null;
        }
    }

    public void shutdown() {
        stop();
        pool.shutdownNow();
    }
}
