package org.dunelegacy.tablethd;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;

/** Installs app-managed assets while preserving files supplied by the user. */
final class AssetInstaller {
    static final String ASSET_VERSION_FILE = "asset-version.txt";
    static final String ASSET_MANIFEST_FILE = "asset-manifest.txt";

    interface AssetSource {
        InputStream open(String relativePath) throws IOException;
    }

    private AssetInstaller() {
    }

    static Set<String> readManifest(InputStream input) throws IOException {
        LinkedHashSet<String> paths = new LinkedHashSet<>();
        try (InputStream source = input;
             BufferedReader reader = new BufferedReader(
                     new InputStreamReader(source, StandardCharsets.UTF_8))) {
            String path;
            while ((path = reader.readLine()) != null) {
                if (path.isEmpty()) {
                    continue;
                }
                validateRelativePath(path);
                if (!paths.add(path)) {
                    throw new IOException("Duplicate path in asset manifest: " + path);
                }
            }
        }
        if (!paths.contains(ASSET_VERSION_FILE) || !paths.contains(ASSET_MANIFEST_FILE)) {
            throw new IOException("Asset manifest must include its version and manifest files");
        }
        return Collections.unmodifiableSet(paths);
    }

    static void install(
            File engineData,
            File staging,
            File backup,
            String bundledVersion,
            Set<String> bundledManifest,
            AssetSource assets) throws IOException {
        if (bundledVersion.isEmpty() || bundledVersion.indexOf('\n') >= 0
                || bundledVersion.indexOf('\r') >= 0) {
            throw new IOException("Invalid bundled asset version");
        }
        validateManifest(bundledManifest);
        recoverOrCleanBackup(engineData, backup);
        deleteRecursively(staging);
        if (!staging.mkdirs()) {
            throw new IOException("Unable to create asset staging directory: " + staging);
        }

        try {
            for (String path : bundledManifest) {
                if (ASSET_VERSION_FILE.equals(path)) {
                    continue;
                }
                copy(assets.open(path), resolve(staging, path));
            }

            Set<String> previouslyManaged = readInstalledManifestOrBootstrap(
                    engineData, bundledManifest);
            preserveUserFiles(
                    engineData, engineData, staging, previouslyManaged, bundledManifest);

            // The marker is deliberately the final file written to the complete staging tree.
            writeVersionMarker(new File(staging, ASSET_VERSION_FILE), bundledVersion);
            validateStagingTree(staging, bundledManifest, bundledVersion);
            activate(engineData, staging, backup);
        } catch (IOException exception) {
            try {
                deleteRecursively(staging);
            } catch (IOException cleanupFailure) {
                exception.addSuppressed(cleanupFailure);
            }
            throw exception;
        }
    }

    private static Set<String> readInstalledManifestOrBootstrap(
            File engineData, Set<String> bundledManifest) throws IOException {
        File installedManifest = new File(engineData, ASSET_MANIFEST_FILE);
        if (!installedManifest.isFile()) {
            // Pre-android9 installations have no manifest. Current bundled paths are the only
            // paths known to be app-managed; every other path is preserved conservatively.
            return bundledManifest;
        }
        return readManifest(new FileInputStream(installedManifest));
    }

    private static void preserveUserFiles(
            File root,
            File current,
            File staging,
            Set<String> previouslyManaged,
            Set<String> newlyManaged) throws IOException {
        if (!current.exists()) {
            return;
        }
        if (current.isDirectory()) {
            File[] children = current.listFiles();
            if (children == null) {
                throw new IOException("Unable to list installed asset directory: " + current);
            }
            for (File child : children) {
                preserveUserFiles(root, child, staging, previouslyManaged, newlyManaged);
            }
            return;
        }

        String relativePath = root.toPath().relativize(current.toPath())
                .toString().replace(File.separatorChar, '/');
        validateRelativePath(relativePath);
        if (previouslyManaged.contains(relativePath) || newlyManaged.contains(relativePath)) {
            return;
        }
        copy(new FileInputStream(current), resolve(staging, relativePath));
    }

    private static void activate(File engineData, File staging, File backup) throws IOException {
        boolean hadInstalledData = engineData.exists();
        if (hadInstalledData && !engineData.renameTo(backup)) {
            throw new IOException("Unable to move installed engine data to rollback directory");
        }
        if (!staging.renameTo(engineData)) {
            boolean rollbackSucceeded = !hadInstalledData || backup.renameTo(engineData);
            if (!rollbackSucceeded) {
                throw new IOException(
                        "Unable to activate new engine data and rollback also failed; data remains in "
                                + backup);
            }
            throw new IOException("Unable to activate new engine data; previous data restored");
        }
        if (backup.exists()) {
            try {
                deleteRecursively(backup);
            } catch (IOException cleanupFailure) {
                System.err.println("Unable to remove obsolete engine-data backup: "
                        + cleanupFailure.getMessage());
            }
        }
    }

    private static void recoverOrCleanBackup(File engineData, File backup) throws IOException {
        if (!backup.exists()) {
            return;
        }
        if (!engineData.exists()) {
            if (!backup.renameTo(engineData)) {
                throw new IOException("Unable to restore interrupted engine-data update from " + backup);
            }
            return;
        }
        deleteRecursively(backup);
    }

    private static void validateStagingTree(
            File staging, Set<String> bundledManifest, String bundledVersion) throws IOException {
        for (String path : bundledManifest) {
            if (!resolve(staging, path).isFile()) {
                throw new IOException("Missing staged app-managed asset: " + path);
            }
        }
        Set<String> stagedManifest = readManifest(new FileInputStream(
                new File(staging, ASSET_MANIFEST_FILE)));
        if (!bundledManifest.equals(stagedManifest)) {
            throw new IOException("Staged asset manifest does not match bundled manifest");
        }
        String installedVersion = MainActivityText.read(new FileInputStream(
                new File(staging, ASSET_VERSION_FILE))).trim();
        if (!bundledVersion.equals(installedVersion)) {
            throw new IOException("Staged asset version marker does not match bundled version");
        }
    }

    private static void validateManifest(Set<String> manifest) throws IOException {
        if (manifest == null || !manifest.contains(ASSET_VERSION_FILE)
                || !manifest.contains(ASSET_MANIFEST_FILE)) {
            throw new IOException("Invalid bundled asset manifest");
        }
        for (String path : manifest) {
            validateRelativePath(path);
        }
    }

    private static void validateRelativePath(String path) throws IOException {
        if (path.isEmpty() || path.startsWith("/") || path.indexOf('\\') >= 0
                || path.indexOf(':') >= 0) {
            throw new IOException("Unsafe asset manifest path: " + path);
        }
        for (String segment : path.split("/", -1)) {
            if (segment.isEmpty() || ".".equals(segment) || "..".equals(segment)) {
                throw new IOException("Unsafe asset manifest path: " + path);
            }
        }
    }

    private static File resolve(File root, String relativePath) throws IOException {
        validateRelativePath(relativePath);
        File result = new File(root, relativePath.replace('/', File.separatorChar));
        String rootPath = root.getCanonicalPath();
        String resultPath = result.getCanonicalPath();
        if (!resultPath.startsWith(rootPath + File.separator)) {
            throw new IOException("Asset path escapes destination: " + relativePath);
        }
        return result;
    }

    private static void copy(InputStream input, File destination) throws IOException {
        File parent = destination.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Unable to create asset directory: " + parent);
        }
        try (InputStream source = input; OutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[16384];
            int count;
            while ((count = source.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
    }

    private static void writeVersionMarker(File marker, String version) throws IOException {
        try (OutputStream output = new FileOutputStream(marker)) {
            output.write((version + "\n").getBytes(StandardCharsets.UTF_8));
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

    /** Pure-Java text helper kept here so AssetInstaller remains Android-independent in tests. */
    private static final class MainActivityText {
        static String read(InputStream input) throws IOException {
            StringBuilder text = new StringBuilder();
            try (InputStream source = input;
                 InputStreamReader reader = new InputStreamReader(source, StandardCharsets.UTF_8)) {
                char[] buffer = new char[256];
                int count;
                while ((count = reader.read(buffer)) != -1) {
                    text.append(buffer, 0, count);
                }
            }
            return text.toString();
        }
    }
}
