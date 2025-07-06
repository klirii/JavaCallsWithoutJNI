# ⚠️ IMPORTANT!

**This works only with the official [JVM 8u191 b12](https://www.oracle.com/java/technologies/javase/javase8-archive-downloads.html).**  
For other builds, you may need to change offsets, class structures, or in some cases even modify the behavior of JVM functions copied into `Main.cpp` (for example, if you want to use this code with newer versions of Java).

This is an example of invoking Java methods **without directly using JNI** – useful in cases where JNI is restricted or obfuscated in a custom JVM.

**P.S: This code is provided for educational purposes only. I do not promote or encourage any illegal activity. All responsibility for the use of this code lies solely with you.**

---

## 🧠 What does this code do?

This is a DLL intended to be injected into a clean Minecraft 1.8.8 instance running on **Java 8u191**.  
The code obtains the `theMinecraft` instance by calling the `getMinecraft` method from the `net.minecraft.client.Minecraft` class – **without using JNI directly**.

**Note:** JNI *is* technically used here, but only to retrieve the `jclass`, `jmethodID`, and `jfieldID` of the `Minecraft` class. This is necessary to locate the method and demonstrate that the resulting object instance is valid for accessing non-static fields.

---

## 🧰 How to use?

1. Download **[JDK 8 Update 191](https://www.oracle.com/java/technologies/javase/javase8-archive-downloads.html)** from the official Oracle website  
2. Link the JNI headers from the downloaded JDK in your project
3. Build the project using Visual Studio
4. Launch a clean Minecraft 1.8.8 instance
5. Inject the compiled DLL using any DLL injector

---

## ⚙️ How does it work?

Based on the **[JDK 8 source code](https://github.com/openjdk/jdk8u/tree/jdk8u191-b12)** and reverse engineering of `jvm.dll`, this project replicates the behavior performed by JNI and the JVM when invoking methods – but **bypasses JNI entirely**. Some of the internal JVM functions used for method invocation were copied into this project.

These functions are often where custom JVMs implement additional checks or obfuscation, causing standard JNI to fail. By replicating the logic and avoiding JNI's direct usage, this code bypasses such restrictions.

Parts of the JVM logic used in method calls were directly copied from the official JVM source code (specific classes, methods, and constants).  
For the remaining parts – especially large methods or dynamically generated internals – **offsets** were used that are  
**guaranteed to be valid only for the official JVM 8u191 b12**.

These offsets are used to interact directly with the JVM's internal structures and memory.

Due to the complexity and size of full JVM classes, not all of them were fully replicated. To access certain fields inside these classes, additional offsets (relative to the start of class) were identified. These are also **guaranteed to be valid only for the official JVM 8u191 b12**.

For other JVM builds or vendors, internal layouts may differ – so offsets and structure definitions may need to be adjusted. You can find the relevant values in `JvmStructures.hpp`.

---

Feel free to open issues or PRs if you're adapting this for other JVM versions or Java games.
