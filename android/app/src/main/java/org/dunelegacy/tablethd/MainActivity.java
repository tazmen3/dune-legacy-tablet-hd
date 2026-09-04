package org.dunelegacy.tablethd;

import org.libsdl.app.SDLActivity;

/** Minimal SDL2 host. Tablet-specific input remains intentionally unimplemented. */
public final class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        // SDL2 and its JNI bridge are linked statically into this application library.
        return new String[] { "dunelegacy" };
    }
}
