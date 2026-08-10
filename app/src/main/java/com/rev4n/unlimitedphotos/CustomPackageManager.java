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
     * "Pixel 2016 - Pixel (original quality)" from PPR1.180610.009 Factory Image
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
            Object result = invokeOriginal(method, args);
            Boolean desired = desiredValue(feature);
            if (desired != null) {
                boolean real = Boolean.TRUE.equals(result);
                if (real != desired) {
                    EntryPoint.LOG(String.format("[%s]: %b -> %b", feature, real, desired));
                    return desired;
                }
            }
            if (EntryPoint.getVerboseLogs() > 1) {
                EntryPoint.LOG(String.format("hasSystemFeature('%s'): passthrough -> %s", feature, result));
            }
            return result;
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
     * True for the original Pixel's features, false for every other Pixel device marker: a device
     * answering yes to both the 2016 preload features and a newer one gets that newer device's more
     * limited backup tier instead of the original's. Every com.google.android.feature.*_EXPERIENCE is
     * denied instead of just the PIXEL_-prefixed ones, which also covers any future codename with no
     * code change. Returns null when the feature is none of our business.
     */
    private static Boolean desiredValue(String feature) {
        if (OG_PIXEL_FEATURES.contains(feature)) return Boolean.TRUE;
        if ((feature.startsWith("com.google.android.feature.") && feature.endsWith("_EXPERIENCE"))
                || feature.startsWith("com.google.android.apps.photos.PIXEL_")
                || feature.startsWith("com.google.android.apps.photos.NEXUS_")) {
            return Boolean.FALSE;
        }
        return null;
    }
}
