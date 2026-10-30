/*
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 */

package java.lang.invoke;

import java.lang.reflect.Constructor;
import java.lang.reflect.AccessibleObject;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Lambda instances for LambdaMetafactory: a Proxy of the functional interface whose handler calls
 * the implementation method with the captured arguments followed by the call arguments.
 *
 * The implementation method is called through reflection where possible: invokeWithArguments
 * fails on handles whose MethodType the runtime created (they have no MethodTypeForm).
 */
final class LambdaProxy implements InvocationHandler {

    /** One per call site; create() is what the call site's factory handle invokes. */
    public static final class Factory {
        private final Constructor<?> proxyConstructor;
        private final String samName;
        private final MethodHandle implMethod;
        private final Member implMember;
        private final Class<?> iface;

        Factory(Constructor<?> proxyConstructor, String samName, MethodHandle implMethod, Class<?> iface) {
            this.proxyConstructor = proxyConstructor;
            this.samName = samName;
            this.implMethod = implMethod;
            this.iface = iface;
            Member member = null;
            if (implMethod instanceof MethodHandleImpl) {
                int kind = implMethod.getHandleKind();
                if (kind == MethodHandle.INVOKE_STATIC || kind == MethodHandle.INVOKE_VIRTUAL
                        || kind == MethodHandle.INVOKE_INTERFACE || kind == MethodHandle.INVOKE_DIRECT) {
                    member = ((MethodHandleImpl) implMethod).getMemberInternal();
                    ((AccessibleObject) member).setAccessible(true);
                }
            }
            this.implMember = member;
        }

        public Object create(Object[] captured) throws Throwable {
            return proxyConstructor.newInstance(new LambdaProxy(this, captured));
        }
    }

    private static final ConcurrentHashMap<Method, MethodHandle> defaultMethodHandles = new ConcurrentHashMap<>();

    private final Factory factory;
    private final Object[] captured;

    private LambdaProxy(Factory factory, Object[] captured) {
        this.factory = factory;
        this.captured = captured;
    }

    static CallSite makeCallSite(MethodHandles.Lookup caller, String samName, MethodType invokedType,
                                 MethodHandle implMethod, Class<?>[] markers)
            throws LambdaConversionException {
        Class<?> iface = invokedType.returnType();
        Set<Class<?>> interfaces = new LinkedHashSet<>();
        interfaces.add(iface);
        for (Class<?> marker : markers) {
            if (marker.isInterface())
                interfaces.add(marker);
        }
        try {
            ClassLoader loader = caller.lookupClass().getClassLoader();
            Class<?> proxyClass = Proxy.getProxyClass(loader, interfaces.toArray(new Class<?>[0]));
            Constructor<?> proxyConstructor = proxyClass.getConstructor(InvocationHandler.class);
            Factory factory = new Factory(proxyConstructor, samName, implMethod, iface);
            MethodHandle create = MethodHandles.Lookup.IMPL_LOOKUP.findVirtual(Factory.class, "create",
                    MethodType.methodType(Object.class, Object[].class));
            // explicitCastArguments rather than asType: ART's asType only changes the nominal
            // type, but invokedynamic requires the handle's real type to match
            MethodHandle target = MethodHandles.explicitCastArguments(
                    create.bindTo(factory).asCollector(Object[].class, invokedType.parameterCount()),
                    invokedType);
            return new ConstantCallSite(target);
        } catch (ReflectiveOperationException | IllegalArgumentException e) {
            LambdaConversionException lce = new LambdaConversionException(
                    "cannot create lambda for " + iface.getName() + "." + samName);
            lce.initCause(e);
            throw lce;
        }
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        if (args == null)
            args = new Object[0];
        if (method.getDeclaringClass() == Object.class) {
            switch (method.getName()) {
                case "equals":
                    return proxy == args[0];
                case "hashCode":
                    return System.identityHashCode(proxy);
                case "toString":
                    return factory.iface.getName() + "$$Lambda@" + Integer.toHexString(System.identityHashCode(proxy));
            }
        }
        if (method.isDefault())
            return defaultMethodHandle(method).bindTo(proxy).invokeWithArguments(args);
        if (!method.getName().equals(factory.samName))
            throw new AbstractMethodError(method.toString());

        Object[] all = new Object[captured.length + args.length];
        System.arraycopy(captured, 0, all, 0, captured.length);
        System.arraycopy(args, 0, all, captured.length, args.length);
        Object result = callImpl(all);
        return method.getReturnType() == void.class ? null : convert(result, method.getReturnType());
    }

    private Object callImpl(Object[] all) throws Throwable {
        Member member = factory.implMember;
        if (member == null)
            return factory.implMethod.asFixedArity().invokeWithArguments(all);
        try {
            if (member instanceof Constructor)
                return ((Constructor<?>) member).newInstance(all);
            Method m = (Method) member;
            if (Modifier.isStatic(m.getModifiers()))
                return m.invoke(null, all);
            Object[] rest = new Object[all.length - 1];
            System.arraycopy(all, 1, rest, 0, rest.length);
            return m.invoke(all[0], rest);
        } catch (InvocationTargetException e) {
            throw e.getCause();
        }
    }

    /** A handle invoking the interface's own default implementation (invoke-super semantics). */
    private static MethodHandle defaultMethodHandle(Method method) {
        MethodHandle handle = defaultMethodHandles.get(method);
        if (handle == null) {
            MethodType type = MethodType.methodType(method.getReturnType(), method.getParameterTypes())
                    .insertParameterTypes(0, method.getDeclaringClass());
            handle = new MethodHandleImpl(method.getArtMethod(), MethodHandle.INVOKE_SUPER, type);
            if (method.isVarArgs())
                handle = handle.asFixedArity();
            defaultMethodHandles.putIfAbsent(method, handle);
        }
        return handle;
    }

    /** Applies the primitive widening and boxing conversions LambdaMetafactory permits. */
    private static Object convert(Object value, Class<?> type) {
        if (value == null || !(value instanceof Number || value instanceof Character))
            return value;
        Class<?> primitive = unboxedType(type);
        if (primitive == null || primitive == unboxedType(value.getClass()))
            return value;
        if (value instanceof Character) {
            char c = (Character) value;
            if (primitive == int.class) return (int) c;
            if (primitive == long.class) return (long) c;
            if (primitive == float.class) return (float) c;
            if (primitive == double.class) return (double) c;
            return value;
        }
        Number n = (Number) value;
        if (primitive == long.class) return n.longValue();
        if (primitive == int.class) return n.intValue();
        if (primitive == short.class) return n.shortValue();
        if (primitive == float.class) return n.floatValue();
        if (primitive == double.class) return n.doubleValue();
        if (primitive == byte.class) return n.byteValue();
        return value;
    }

    private static Class<?> unboxedType(Class<?> type) {
        if (type.isPrimitive()) return type;
        if (type == Integer.class) return int.class;
        if (type == Long.class) return long.class;
        if (type == Short.class) return short.class;
        if (type == Byte.class) return byte.class;
        if (type == Character.class) return char.class;
        if (type == Float.class) return float.class;
        if (type == Double.class) return double.class;
        if (type == Boolean.class) return boolean.class;
        return null;
    }
}
