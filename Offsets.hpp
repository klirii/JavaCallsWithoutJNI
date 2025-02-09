#include "AOBScanner.hpp"
#include <jni.h>

extern HMODULE jvm;

extern bool* AssumeMP;
extern bool* UseMembar;

class JavaValue;
enum JNICallType;
class JNI_ArgumentPusher;
class JavaThread;

class AllocFailStrategy {
public:
	enum AllocFailEnum { EXIT_OOM, RETURN_NULL };
};

typedef AllocFailStrategy::AllocFailEnum AllocFailType;

typedef void* (__fastcall* jni_invoke_static_t)(
	JNIEnv* env,
	JavaValue* result,
	jmethodID method_id,
	JNI_ArgumentPusher* args,
	JavaThread* thread
);
jni_invoke_static_t jni_invoke_static = nullptr;

typedef char* (__fastcall* resource_allocate_bytes_t)(size_t size, AllocFailType alloc_failmode);
resource_allocate_bytes_t resource_allocate_bytes = nullptr;

#define JNI_ARGUMENT_PUSHER_VA_ARG 0x60B668
#define JNI_ARGUMENT_PUSHER_VA_ARG_ITERATE_VFTABLE_OFFSET 0xA0

#define JNI_INVOKE_STATIC 0x149920
#define RESOURCE_ALLOCATE_BYTES 0xEDCA0

#define OS_PROCESSOR_COUNT 0x815828
#define SAFEPOINT_SYNCHRONIZE_STATE 0x815A98

#define ASSUME_MP 0x814AA3
#define USE_MEMBAR 0x814AA4

#define ORDER_ACCESS_FENCE 0x21E7D0
#define INTERFACE_SUPPORT_SERIALIZE_MEMORY 0x8670
#define JAVA_THREAD_CHECK_SAFEPOINT_AND_SUSPEND_FOR_NATIVE_TRANS 0x243C50
#define ARENA_SET_SIZE_IN_BYTES 0xCEB20
#define CHUNK_NEXT_CHOP 0xCE820
#define SAFEPOINT_SYNCHRONIZE_BLOCK 0x22AAE0
#define METHOD_HANDLE_REMOVE 0x4330
#define JAVA_CALLS_CALL 0x215F60
#define JNI_HANDLES_MAKE_LOCAL 0x217940

#define FINGERPRINTER 0x5FBB88
#define FINGERPRINTER_FINGERPRINT 0xB7A20

#define WEAK_PRESERVE_EXCEPTION_MARK_PRESERVE 0x27DDE0
#define WEAK_PRESERVE_EXCEPTION_MARK_RESTORE  0x27DE30

// jni_invoke_static

#define GROWABLE_ARRAY_GROW 0x455DB0

#define FIND_AOB(pattern, matches_data_type, reg_attrs, from, to)	\
	std::vector<matches_data_type> matches;							\
	AOBScanner::Scan(												\
		current_process,											\
		pattern,													\
		matches,													\
		reg_attrs,													\
		from,														\
		to															\
	)																\

#define FIND_AOB_IN_MODULE(pattern, matches_data_type, reg_attrs, module_info)	\
	FIND_AOB(																	\
		pattern,																\
		matches_data_type,														\
		reg_attrs,																\
		(BYTE*)module_info.lpBaseOfDll,											\
		(BYTE*)module_info.lpBaseOfDll + module_info.SizeOfImage				\
	)																			\

#define FIND_STRUCT_OFFSET(pattern, result_var, offset_data_type, offset_from_pattern) {							\
	FIND_AOB_IN_MODULE(																								\
		pattern,																									\
		BYTE*,																										\
		AOBScanner::RegionAttributes(PAGE_EXECUTE_READ, MEM_COMMIT, PAGE_EXECUTE_READ, MEM_MAPPED),					\
		jvm_info																									\
	);																												\
	if (matches.size() == 1) result_var = *reinterpret_cast<offset_data_type*>(matches[0] + offset_from_pattern);	\
}																													\

#define FIND_VA_FROM_RVA(pattern, result_var, rva_offset_from_pattern, inst_offset_from_pattern, inst_length) {	\
	FIND_AOB_IN_MODULE(																							\
		pattern,																								\
		BYTE*,																									\
		AOBScanner::RegionAttributes(PAGE_EXECUTE_READ, MEM_COMMIT, PAGE_EXECUTE_READ, MEM_MAPPED),				\
		jvm_info																								\
	);																											\
																												\
	if (matches.size() == 1) {																					\
		signed int rva = *reinterpret_cast<signed int*>(matches[0] + rva_offset_from_pattern);					\
		result_var = (matches[0] + inst_offset_from_pattern + inst_length + rva);								\
	}																											\
}																												\

void FindAllOffsets() {
	jni_invoke_static = (jni_invoke_static_t)((BYTE*)jvm + JNI_INVOKE_STATIC);
	resource_allocate_bytes = (resource_allocate_bytes_t)((BYTE*)jvm + RESOURCE_ALLOCATE_BYTES);

	AssumeMP = (bool*)((BYTE*)jvm + ASSUME_MP);
	UseMembar = (bool*)((BYTE*)jvm + USE_MEMBAR);
}