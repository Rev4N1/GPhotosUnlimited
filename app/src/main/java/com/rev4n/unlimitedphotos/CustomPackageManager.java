package com.rev4n.unlimitedphotos;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Answers Google Photos' Pixel feature queries the way the original 2016 Pixel/Pixel XL would,
 * matching the Build fields fgp.prop spoofs. Installed as a {@link java.lang.reflect.Proxy} over
 * the app process' IPackageManager, so it only ever affects the process it is installed in.
 */
final class CustomPackageManager implements InvocationHandler {

    /**
     * Features the original Pixel/Pixel XL declares - the "Pixel Core" and
     * "Pixel 2016 - Pixel XL (original quality)" buckets of Pixelify-Google-Photos' DeviceProps.kt.
     */
    private static final Set<String> OG_PIXEL_FEATURES = new HashSet<>(Arrays.asList(
            "com.google.android.feature.PIXEL_EXPERIENCE",
            "com.google.android.feature.GOOGLE_BUILD",
            "com.google.android.feature.GOOGLE_EXPERIENCE",
            "com.google.android.apps.photos.NEXUS_PRELOAD",
            "com.google.android.apps.photos.nexus_preload",
            "com.google.android.apps.photos.PIXEL_PRELOAD",
            "com.google.android.apps.photos.PIXEL_2016_PRELOAD"));

    private final Object original;

    CustomPackageManager(Object original) {
        this.original = original;
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        if (args != null && args.length > 0 && args[0] instanceof String
                && "hasSystemFeature".equals(method.getName())) {
            String feature = (String) args[0];
            Boolean spoofed = spoofFeature(feature);
            // Anything that isn't a Pixel marker falls through to the real answer
            if (spoofed != null) return spoofed;
            if (EntryPoint.getVerboseLogs() > 1) {
                Object result = invokeOriginal(method, args);
                // Passthrough, unlike spoofFeature()'s two branches above - logged separately so a
                // capture at this level sees every feature Photos actually asks about, not just the
                // ones already known to matter, which is what makes the list above worth trusting
                EntryPoint.LOG(String.format("hasSystemFeature('%s'): passthrough -> %s", feature, result));
                return result;
            }
        }
        return invokeOriginal(method, args);
    }

    private Object invokeOriginal(Method method, Object[] args) throws Throwable {
        try {
            return method.invoke(original, args);
        } catch (InvocationTargetException e) {
            // Unwrap, or the RemoteException the real PackageManager declares would reach the
            // caller as an UndeclaredThrowableException instead
            throw e.getCause() != null ? e.getCause() : e;
        }
    }

    /**
     * True for the original Pixel's features, false for every other Pixel model-year marker: a
     * device answering yes to both the 2016 preload features and a newer model year gets that newer
     * device's more limited backup tier instead of the original's. Matching the newer ones by prefix
     * rather than by a list of years keeps mid-year, tablet and future markers covered as well.
     * Returns null when the feature is none of our business.
     */
    private static Boolean spoofFeature(String feature) {
        boolean spoofed;
        if (OG_PIXEL_FEATURES.contains(feature)) {
            spoofed = true;
        } else if (feature.startsWith("com.google.android.feature.PIXEL_")
                || feature.startsWith("com.google.android.apps.photos.PIXEL_")
                || feature.startsWith("com.google.android.apps.photos.NEXUS_")) {
            spoofed = false;
        } else {
            return null;
        }
        if (EntryPoint.getVerboseLogs() > 0) {
            EntryPoint.LOG(String.format("hasSystemFeature('%s'): -> %b", feature, spoofed));
        }
        return spoofed;
    }
}
