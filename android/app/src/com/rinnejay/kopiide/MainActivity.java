package com.rinnejay.kopiide;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.ViewGroup;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/**
 * Kopi IDE — satu Activity berisi WebView.
 * Semua UI ada di assets/ (HTML+CSS+JS); semua akses berkas & eksekusi
 * compiler ada di IdeBridge.
 */
public class MainActivity extends Activity {

    private WebView web;
    private IdeBridge bridge;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);

        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(24, 26, 31));
        web.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(true);
        s.setAllowContentAccess(true);
        s.setSupportZoom(false);
        s.setBuiltInZoomControls(false);
        s.setDisplayZoomControls(false);
        s.setUseWideViewPort(false);
        s.setLoadWithOverviewMode(false);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);

        web.setWebViewClient(new WebViewClient());
        bridge = new IdeBridge(this, web);
        web.addJavascriptInterface(bridge, "Kopi");

        setContentView(web);
        web.loadUrl("file:///android_asset/index.html");
    }

    @Override
    public void onBackPressed() {
        // biarkan JS yang memutuskan: tutup laci/panel dulu, baru keluar
        web.evaluateJavascript("window.IDE && IDE.handleBack() ? 0 : 0", null);
    }

    @Override
    protected void onDestroy() {
        if (bridge != null) bridge.shutdown();
        if (web != null) web.destroy();
        super.onDestroy();
    }
}
