package com.rev4n.unlimitedphotos;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Answers Google Photos' Pixel feature queries as configured in fgp.prop, purely per-process via
 * a {@link java.lang.reflect.Proxy} over IPackageManager.
 */
final class CustomPackageManager implements InvocationHandler {

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
            String override = EntryPoint.getFeatureOverride(feature);
            if (override != null) {
                boolean real = Boolean.TRUE.equals(result);
                boolean desired = Boolean.parseBoolean(override);
                if (!"true".equalsIgnoreCase(override) && !"false".equalsIgnoreCase(override)) {
                    EntryPoint.LOG(String.format("[%s]: value '%s' isn't 'true' or 'false', treating as false", feature, override));
                }
                if (real != desired) {
                    EntryPoint.LOG(String.format("[%s]: %b -> %b", feature, real, desired));
                } else {
                    EntryPoint.LOG(String.format("[%s]: %b (unchanged)", feature, real));
                }
                return desired;
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
}
