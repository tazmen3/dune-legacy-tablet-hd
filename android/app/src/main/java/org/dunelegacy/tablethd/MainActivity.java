package org.dunelegacy.tablethd;

import android.content.res.AssetManager;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

/** Minimal SDL2 host. Tablet-specific input remains intentionally unimplemented. */
public final class MainActivity extends SDLActivity {
    private static final String BUNDLED_ASSET_ROOT = "dunelegacy-data";
    private static final String ENGINE_DATA_DIRECTORY = "engine-data";
    private static final String ASSET_VERSION_FILE = "asset-version.txt";

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
        String bundledVersion = readText(assets.open(BUNDLED_ASSET_ROOT + "/" + ASSET_VERSION_FILE));
        File engineData = new File(getFilesDir(), ENGINE_DATA_DIRECTORY);
        File installedVersion = new File(engineData, ASSET_VERSION_FILE);

        if (installedVersion.isFile()
                && bundledVersion.equals(readText(new FileInputStream(installedVersion)))) {
            return;
        }

        File staging = new File(getFilesDir(), ENGINE_DATA_DIRECTORY + ".new");
        File backup = new File(getFilesDir(), ENGINE_DATA_DIRECTORY + ".old");
        deleteRecursively(staging);
        deleteRecursively(backup);
        copyAssetTree(assets, BUNDLED_ASSET_ROOT, staging);

        if (engineData.exists() && !engineData.renameTo(backup)) {
            deleteRecursively(staging);
            throw new IOException("Unable to replace existing engine data directory");
        }
        if (!staging.renameTo(engineData)) {
            if (backup.exists()) {
                backup.renameTo(engineData);
            }
            throw new IOException("Unable to activate extracted engine data directory");
        }
        deleteRecursively(backup);
    }

    private static void copyAssetTree(AssetManager assets, String assetPath, File destination)
            throws IOException {
        String[] children = assets.list(assetPath);
        if (children == null) {
            throw new IOException("Unable to list APK asset: " + assetPath);
        }

        if (children.length == 0) {
            File parent = destination.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                throw new IOException("Unable to create directory: " + parent);
            }
            try (InputStream input = assets.open(assetPath);
                 OutputStream output = new FileOutputStream(destination)) {
                byte[] buffer = new byte[16384];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    output.write(buffer, 0, count);
                }
            }
            return;
        }

        if (!destination.isDirectory() && !destination.mkdirs()) {
            throw new IOException("Unable to create directory: " + destination);
        }
        for (String child : children) {
            copyAssetTree(assets, assetPath + "/" + child, new File(destination, child));
        }
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

    private static void deleteRecursively(File file) throws IOException {
        if (!file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) {
                throw new IOException("Unable to list directory: " + file);
            }
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        if (!file.delete()) {
            throw new IOException("Unable to delete: " + file);
        }
    }
}
