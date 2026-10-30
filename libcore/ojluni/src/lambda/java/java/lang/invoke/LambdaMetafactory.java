/*
 * Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package java.lang.invoke;

/*
 * Android desugars lambdas at build time, so upstream this class is a stub. art_standalone dexes
 * libcore (and apps may ship code) with invoke-custom left in, so implement it at runtime on top of
 * java.lang.reflect.Proxy; see LambdaProxy.
 */
public class LambdaMetafactory {

    public static final int FLAG_SERIALIZABLE = 1 << 0;

    public static final int FLAG_MARKERS = 1 << 1;

    public static final int FLAG_BRIDGES = 1 << 2;

    public static CallSite metafactory(MethodHandles.Lookup caller,
                                       String invokedName,
                                       MethodType invokedType,
                                       MethodType samMethodType,
                                       MethodHandle implMethod,
                                       MethodType instantiatedMethodType)
            throws LambdaConversionException {
        return LambdaProxy.makeCallSite(caller, invokedName, invokedType, implMethod, new Class<?>[0]);
    }

    public static CallSite altMetafactory(MethodHandles.Lookup caller,
                                          String invokedName,
                                          MethodType invokedType,
                                          Object... args)
            throws LambdaConversionException {
        // args: samMethodType, implMethod, instantiatedMethodType, flags, [markerCount, markers...],
        // [bridgeCount, bridges...]; bridges need no handling as dispatch is by method name
        MethodHandle implMethod = (MethodHandle) args[1];
        int flags = (Integer) args[3];
        int argIndex = 4;
        Class<?>[] markers = new Class<?>[0];
        if ((flags & FLAG_MARKERS) != 0) {
            int markerCount = (Integer) args[argIndex++];
            markers = new Class<?>[markerCount];
            for (int i = 0; i < markerCount; i++)
                markers[i] = (Class<?>) args[argIndex++];
        }
        if ((flags & FLAG_SERIALIZABLE) != 0) {
            Class<?>[] withSerializable = java.util.Arrays.copyOf(markers, markers.length + 1);
            withSerializable[markers.length] = java.io.Serializable.class;
            markers = withSerializable;
        }
        return LambdaProxy.makeCallSite(caller, invokedName, invokedType, implMethod, markers);
    }
}
