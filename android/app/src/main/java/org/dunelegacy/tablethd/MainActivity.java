package org.dunelegacy.tablethd;

import android.content.res.AssetManager;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.Set;

/** Minimal SDL2 host. Tablet-specific input remains intentionally unimplemented. */
public final class MainActivity extends SDLActivity {
    private static final String BUNDLED_ASSET_ROOT = "dunelegacy-data";
    private static final String ENGINE_DATA_DIRECTORY = "engine-data";

    @Override
    protected String[] getLibraries() {
        // SDL2 and its JNI bridge are linked statically into this application library.
        return new String[] { "dunelegacy" };
    }

    @Override
    public void loadLibraries() {
        super.loadLibraries();

        try {
            installBundledEngineData();
        } catch (IOException exception) {
            throw new RuntimeException("Unable to install bundled Dune Legacy resources", exception);
        }
    }

    private void installBundledEngineData() throws IOException {
        AssetManager assets = getAssets();
        String bundledVersion = readText(assets.open(
                BUNDLED_ASSET_ROOT + "/" + AssetInstaller.ASSET_VERSION_FILE));
        File engineData = new File(getFilesDir(), ENGINE_DATA_DIRECTORY);
        File installedVersion = new File(engineData, AssetInstaller.ASSET_VERSION_FILE);

        if (installedVersion.isFile()
                && bundledVersion.equals(readText(new FileInputStream(installedVersion)))) {
            return;
        }

        Set<String> bundledManifest = AssetInstaller.readManifest(assets.open(
                BUNDLED_ASSET_ROOT + "/" + AssetInstaller.ASSET_MANIFEST_FILE));
        AssetInstaller.install(
                engineData,
                new File(getFilesDir(), ENGINE_DATA_DIRECTORY + ".new"),
                new File(getFilesDir(), ENGINE_DATA_DIRECTORY + ".old"),
                bundledVersion,
                bundledManifest,
                relativePath -> assets.open(BUNDLED_ASSET_ROOT + "/" + relativePath));
    }

    private static String readText(InputStream input) throws IOException {
        try (InputStream source = input; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[256];
            int count;
            while ((count = source.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
            return output.toString(StandardCharsets.UTF_8.name()).trim();
        }
    }

}
