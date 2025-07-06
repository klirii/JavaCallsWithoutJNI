⚠️ IMPORTANT!
This works only with the official JVM 8u191 b12.
For other builds, you may need to modify offsets, class structures, or even the behavior of JVM functions copied into Main.cpp (for example, if you want to use this code with newer versions of Java).

This is an example of invoking Java methods without using JNI directly (e.g. when JNI is restricted or obfuscated in a custom JVM).

P.S.: This code is provided for educational purposes only. I do not encourage any illegal activity, and any responsibility for using this code lies solely with you.

🧠 What does this code do?
This is a DLL intended to be injected into a clean Minecraft 1.8.8 instance running on Java 8u191.
The code obtains the theMinecraft instance via a call to the getMinecraft method in the net.minecraft.client.Minecraft class without using JNI.

P.S.: JNI is used in this example, but only to retrieve jclass, jmethodID, and jfieldID of the Minecraft class in order to locate the method and later access instance fields using the obtained object, thereby proving its validity.

🔧 How to use?
Download JDK 8 update 191 from the official website
// where you'll add the official download link //

Link JNI from the JDK you just downloaded into your project

Build the project in Visual Studio

Launch a clean Minecraft 1.8.8

Inject the compiled DLL using any injector

🛠️ How does it work?
Based on the JDK source code here:
// where you'll insert the GitHub repository link for JDK 8 //
and reverse engineering jvm.dll, I implemented exactly the same behavior that JNI and the JVM perform when invoking a method — but bypassed JNI entirely, and copied some of the JVM's method invocation functions. These functions are often the target of various checks or obfuscations, which is why standard JNI may stop working in custom JVMs.

Some of the JVM dependencies used in these method invocation routines were copied directly from the JVM source (certain classes, methods, and values).
For the rest (e.g. larger methods or dynamically generated JVM internals), I found offsets that are guaranteed to be valid only for the official JVM 8u191 b12.
// text between asterisks must be bold //

These offsets are essential when interacting with JVM internals, as they allow direct memory access without relying on JNI.

Due to complexity and lack of necessity, I didn't fully replicate the large JVM classes. Therefore, for accessing certain fields inside JVM classes, I also determined offsets relative to the class's starting address. These offsets are likewise guaranteed to be valid only for the official JVM 8u191 b12.
For other JVM builds, class layouts may differ, and therefore the offsets may break. You can find them inside JvmStructures.hpp.
