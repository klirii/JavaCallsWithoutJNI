#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <jni.h>

#include "JvmStructures.hpp"

#pragma warning(disable:6031)

#define DEBUG(format, ...) printf("[!] " format "\n", __VA_ARGS__);

HMODULE jvm = nullptr;
JavaVM* vm  = nullptr;
JNIEnv* env = nullptr;

void jni_invoke_static_internal(JNIEnv* env, JavaValue* result, jmethodID method_id, JNI_ArgumentPusherVaArg* args, JavaThread* thread) {
	methodHandle method(thread, Method::resolve_jmethod_id(method_id));

	ResourceMark rm(thread);
	int number_of_parameters = method->size_of_parameters();
	JavaCallArguments java_args(number_of_parameters);
	args->set_java_argument_object(&java_args);

	args->iterate(Fingerprinter(method).fingerprint());
	result->set_type(args->get_ret_type());

	JavaCalls::call(result, method, &java_args, thread);

	if (!thread->has_pending_exception()) {
		if (result->get_type() == T_OBJECT || result->get_type() == T_ARRAY) {
			result->set_jobject(JNIHandles::make_local(env, result->get_jobject()));
		}
	}
}

jobject JNICALL CallStaticObjectMethod(JNIEnv* env, jclass clazz, jmethodID methodID, ...) {
	va_list args;
	va_start(args, methodID);

	JavaThread* thread = JavaThread::thread_from_jni_environment(env);
	
	ThreadInVMfromNative transition(thread);
	WeakPreserveExceptionMark mark(thread);
	HandleMarkCleaner cleaner(thread);

	JavaValue jvalue(T_OBJECT);
	JNI_ArgumentPusherVaArg ap(methodID, args);
	jni_invoke_static_internal(env, &jvalue, methodID, &ap, thread);

	va_end(args);

	if (thread->has_pending_exception())
		return nullptr;

	return jvalue.get_jobject();
}

void Main() {
	jvm = GetModuleHandleA("jvm.dll");
	DEBUG("jvm: %p", jvm);

	JNI_GetCreatedJavaVMs(&vm, 1, nullptr);	
	DEBUG("vm: %p", vm);

	vm->AttachCurrentThread((void**)&env, nullptr);
	DEBUG("env: %p", env);
	DEBUG("CallStaticObjectMethodV offset: %llu", (size_t)((BYTE*)env->functions->CallStaticObjectMethodV - (BYTE*)jvm));

	FindAllOffsets();
	
	jclass Minecraft = env->FindClass("ave");
	DEBUG("Minecraft class: %p", Minecraft);

	jmethodID get_minecraft_mid = env->GetStaticMethodID(Minecraft, "A", "()Lave;");
	DEBUG("getMinecraft method id: %p", get_minecraft_mid);

	jobject mc = CallStaticObjectMethod(env, Minecraft, get_minecraft_mid);
	DEBUG("mc: %p", mc);

	jfieldID display_width_fid = env->GetFieldID(Minecraft, "d", "I");
	DEBUG("displayWidth field id: %p", display_width_fid);

	jfieldID display_height_fid = env->GetFieldID(Minecraft, "e", "I");
	DEBUG("displayHeight field id: %p", display_height_fid);

	DEBUG("displayWidth: %d", env->GetIntField(mc, display_width_fid));
	DEBUG("displayHeight: %d", env->GetIntField(mc, display_height_fid));
	
	DEBUG("thread state -> %d", JavaThread::thread_from_jni_environment(env)->get_thread_state());
	DEBUG("TlsGetValue: %p", TlsGetValue);
	vm->DetachCurrentThread();
	DEBUG("test!");
}

BOOL APIENTRY DllMain(HINSTANCE handle, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_ATTACH) {
		AllocConsole();
		freopen("CONOUT$", "w", stdout);

		CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)Main, nullptr, NULL, nullptr);
		return TRUE;
	}

	return FALSE;
}