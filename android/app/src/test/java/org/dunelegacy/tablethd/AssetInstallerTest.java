package org.dunelegacy.tablethd;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public final class AssetInstallerTest {
    @Rule
    public final TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Test
    public void firstInstallationExtractsManagedAssetsAndWritesVersionLast() throws Exception {
        Fixture fixture = fixture();
        fixture.assets.put("GFXHD.PAK", bytes("new gfx"));
        fixture.install("android9");

        assertArrayEquals(bytes("new gfx"), fixture.read("GFXHD.PAK"));
        assertEquals("android9\n", fixture.readText(AssetInstaller.ASSET_VERSION_FILE));
        assertTrue(fixture.file(AssetInstaller.ASSET_MANIFEST_FILE).isFile());
    }

    @Test
    public void updateWithoutUserFilesReplacesManagedAssets() throws Exception {
        Fixture fixture = fixture();
        fixture.createInstalled(
                "android8",
                mapOf("GFXHD.PAK", bytes("old gfx")),
                Collections.emptyMap());
        fixture.assets.put("GFXHD.PAK", bytes("new gfx"));
        fixture.install("android9");

        assertArrayEquals(bytes("new gfx"), fixture.read("GFXHD.PAK"));
    }

    @Test
    public void updatePreservesUnknownUserFileByteForByte() throws Exception {
        Fixture fixture = fixture();
        byte[] userPak = new byte[] { 0, 1, 2, 3, (byte) 255, 10, 42 };
        fixture.createInstalled(
                "android8",
                mapOf("GFXHD.PAK", bytes("old gfx")),
                mapOf("ATRE.PAK", userPak));
        String hashBefore = sha256(userPak);
        fixture.assets.put("GFXHD.PAK", bytes("new gfx"));
        fixture.install("android9");

        byte[] preserved = fixture.read("ATRE.PAK");
        assertArrayEquals(userPak, preserved);
        assertEquals(hashBefore, sha256(preserved));
    }

    @Test
    public void preManifestUpdateConservativelyPreservesUnknownFiles() throws Exception {
        Fixture fixture = fixture();
        Files.createDirectories(fixture.engineData.toPath());
        fixture.writeInstalled(AssetInstaller.ASSET_VERSION_FILE, bytes("android8\n"));
        fixture.writeInstalled("GFXHD.PAK", bytes("old gfx"));
        fixture.writeInstalled("DUNE.PAK", bytes("user dune data"));
        fixture.assets.put("GFXHD.PAK", bytes("new gfx"));
        fixture.install("android9");

        assertArrayEquals(bytes("user dune data"), fixture.read("DUNE.PAK"));
        assertArrayEquals(bytes("new gfx"), fixture.read("GFXHD.PAK"));
    }

    @Test
    public void removedPreviouslyManagedAssetIsNotRetained() throws Exception {
        Fixture fixture = fixture();
        fixture.createInstalled(
                "android8",
                mapOf("obsolete.dat", bytes("obsolete")),
                mapOf("notes/user.dat", bytes("keep me")));
        fixture.assets.put("current.dat", bytes("current"));
        fixture.install("android9");

        assertFalse(fixture.file("obsolete.dat").exists());
        assertArrayEquals(bytes("keep me"), fixture.read("notes/user.dat"));
    }

    @Test
    public void localizationCataloguesAreManagedAndReplaced() throws Exception {
        Fixture fixture = fixture();
        fixture.createInstalled(
                "android8",
                mapOf(
                        "locale/French.fr.po", bytes("ancienne traduction"),
                        "locale/Spanish.es.po", bytes("traduccion anterior"),
                        "locale/German.de.po", bytes("alte Uebersetzung")),
                Collections.emptyMap());
        fixture.assets.put("locale/French.fr.po", bytes("Tout annuler"));
        fixture.assets.put("locale/Spanish.es.po", bytes("Cancelar todo"));
        fixture.assets.put("locale/German.de.po", bytes("Alles abbrechen"));
        fixture.install("android9");

        assertEquals("Tout annuler", fixture.readText("locale/French.fr.po"));
        assertEquals("Cancelar todo", fixture.readText("locale/Spanish.es.po"));
        assertEquals("Alles abbrechen", fixture.readText("locale/German.de.po"));
    }

    @Test
    public void failedExtractionLeavesInstalledVersionAndUserBytesUntouched() throws Exception {
        Fixture fixture = fixture();
        byte[] userPak = bytes("irreplaceable user bytes");
        fixture.createInstalled(
                "android8",
                mapOf("GFXHD.PAK", bytes("old gfx")),
                mapOf("ATRE.PAK", userPak));
        fixture.assets.put("GFXHD.PAK", bytes("new gfx"));
        fixture.assets.put("broken.dat", bytes("never returned"));
        fixture.failOn = "broken.dat";

        try {
            fixture.install("android9");
            fail("Expected extraction failure");
        } catch (IOException expected) {
            assertTrue(expected.getMessage().contains("injected"));
        }

        assertEquals("android8\n", fixture.readText(AssetInstaller.ASSET_VERSION_FILE));
        assertArrayEquals(bytes("old gfx"), fixture.read("GFXHD.PAK"));
        assertArrayEquals(userPak, fixture.read("ATRE.PAK"));
        assertFalse(fixture.staging.exists());
    }

    @Test
    public void interruptedSwapRecoversBackupBeforeUpdating() throws Exception {
        Fixture fixture = fixture();
        Files.createDirectories(fixture.backup.toPath());
        Files.write(new File(fixture.backup, AssetInstaller.ASSET_VERSION_FILE).toPath(),
                bytes("android8\n"));
        Files.write(new File(fixture.backup, AssetInstaller.ASSET_MANIFEST_FILE).toPath(),
                Fixture.manifestBytes(Collections.singleton("GFXHD.PAK")));
        Files.write(new File(fixture.backup, "GFXHD.PAK").toPath(), bytes("old gfx"));
        Files.write(new File(fixture.backup, "ATRE.PAK").toPath(), bytes("user bytes"));
        fixture.assets.put("GFXHD.PAK", bytes("new gfx"));

        fixture.install("android9");

        assertArrayEquals(bytes("new gfx"), fixture.read("GFXHD.PAK"));
        assertArrayEquals(bytes("user bytes"), fixture.read("ATRE.PAK"));
        assertFalse(fixture.backup.exists());
    }

    @Test
    public void manifestParserRejectsTraversalAndAcceptsCanonicalPaths() throws Exception {
        Set<String> parsed = AssetInstaller.readManifest(new ByteArrayInputStream(bytes(
                "asset-version.txt\nasset-manifest.txt\nlocale/French.fr.po\n")));
        assertTrue(parsed.contains("locale/French.fr.po"));

        try {
            AssetInstaller.readManifest(new ByteArrayInputStream(bytes(
                    "asset-version.txt\nasset-manifest.txt\n../ATRE.PAK\n")));
            fail("Expected unsafe manifest path rejection");
        } catch (IOException expected) {
            assertTrue(expected.getMessage().contains("Unsafe"));
        }
    }

    private Fixture fixture() throws IOException {
        return new Fixture(temporaryFolder.newFolder("files"));
    }

    private static byte[] bytes(String value) {
        return value.getBytes(StandardCharsets.UTF_8);
    }

    private static String sha256(byte[] content) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(content);
        StringBuilder hex = new StringBuilder();
        for (byte value : digest) {
            hex.append(String.format("%02x", value & 0xff));
        }
        return hex.toString();
    }

    private static Map<String, byte[]> mapOf(Object... entries) {
        LinkedHashMap<String, byte[]> result = new LinkedHashMap<>();
        for (int index = 0; index < entries.length; index += 2) {
            result.put((String) entries[index], (byte[]) entries[index + 1]);
        }
        return result;
    }

    private static final class Fixture {
        final File engineData;
        final File staging;
        final File backup;
        final LinkedHashMap<String, byte[]> assets = new LinkedHashMap<>();
        String failOn;

        Fixture(File filesDir) {
            engineData = new File(filesDir, "engine-data");
            staging = new File(filesDir, "engine-data.new");
            backup = new File(filesDir, "engine-data.old");
        }

        void createInstalled(
                String version,
                Map<String, byte[]> managed,
                Map<String, byte[]> userOwned) throws Exception {
            Files.createDirectories(engineData.toPath());
            for (Map.Entry<String, byte[]> entry : managed.entrySet()) {
                writeInstalled(entry.getKey(), entry.getValue());
            }
            for (Map.Entry<String, byte[]> entry : userOwned.entrySet()) {
                writeInstalled(entry.getKey(), entry.getValue());
            }
            writeInstalled(AssetInstaller.ASSET_VERSION_FILE, bytes(version + "\n"));
            writeInstalled(AssetInstaller.ASSET_MANIFEST_FILE,
                    manifestBytes(managed.keySet()));
        }

        void install(String version) throws Exception {
            LinkedHashMap<String, byte[]> packaged = new LinkedHashMap<>(assets);
            packaged.put(AssetInstaller.ASSET_VERSION_FILE, bytes(version + "\n"));
            packaged.put(AssetInstaller.ASSET_MANIFEST_FILE, manifestBytes(assets.keySet()));
            Set<String> manifest = AssetInstaller.readManifest(new ByteArrayInputStream(
                    packaged.get(AssetInstaller.ASSET_MANIFEST_FILE)));
            AssetInstaller.install(engineData, staging, backup, version, manifest, path -> {
                if (path.equals(failOn)) {
                    throw new IOException("injected copy failure for " + path);
                }
                byte[] content = packaged.get(path);
                if (content == null) {
                    throw new IOException("missing fixture asset " + path);
                }
                return new ByteArrayInputStream(content);
            });
        }

        void writeInstalled(String path, byte[] content) throws IOException {
            File destination = file(path);
            Files.createDirectories(destination.getParentFile().toPath());
            Files.write(destination.toPath(), content);
        }

        byte[] read(String path) throws IOException {
            return Files.readAllBytes(file(path).toPath());
        }

        String readText(String path) throws IOException {
            return new String(read(path), StandardCharsets.UTF_8);
        }

        File file(String path) {
            return new File(engineData, path.replace('/', File.separatorChar));
        }

        private static byte[] manifestBytes(Iterable<String> paths) {
            StringBuilder manifest = new StringBuilder();
            manifest.append(AssetInstaller.ASSET_VERSION_FILE).append('\n');
            manifest.append(AssetInstaller.ASSET_MANIFEST_FILE).append('\n');
            for (String path : paths) {
                if (!AssetInstaller.ASSET_VERSION_FILE.equals(path)
                        && !AssetInstaller.ASSET_MANIFEST_FILE.equals(path)) {
                    manifest.append(path).append('\n');
                }
            }
            return bytes(manifest.toString());
        }
    }
}
