#include "Offsets.hpp"

bool* AssumeMP  = nullptr;
bool* UseMembar = nullptr;

typedef unsigned char  u1;
typedef unsigned short u2;

#define NEW_RESOURCE_ARRAY(type, size)\
    (type*)resource_allocate_bytes(size * sizeof(type), AllocFailType::EXIT_OOM);

// NOTE: replicated in SA in vm/agent/sun/jvm/hotspot/runtime/BasicType.java
enum BasicType {
    T_BOOLEAN = 4,
    T_CHAR = 5,
    T_FLOAT = 6,
    T_DOUBLE = 7,
    T_BYTE = 8,
    T_SHORT = 9,
    T_INT = 10,
    T_LONG = 11,
    T_OBJECT = 12,
    T_ARRAY = 13,
    T_VOID = 14,
    T_ADDRESS = 15,
    T_NARROWOOP = 16,
    T_METADATA = 17,
    T_NARROWKLASS = 18,
    T_CONFLICT = 19, // for stack value type with conflicting contents
    T_ILLEGAL = 99
};

enum JNICallType {
    JNI_STATIC,
    JNI_VIRTUAL,
    JNI_NONVIRTUAL
};

enum JavaThreadState {
    _thread_uninitialized = 0, // should never happen (missing initialization)
    _thread_new = 2, // just starting up, i.e., in process of being initialized
    _thread_new_trans = 3, // corresponding transition state (not used, included for completness)
    _thread_in_native = 4, // running in native code
    _thread_in_native_trans = 5, // corresponding transition state
    _thread_in_vm = 6, // running in VM
    _thread_in_vm_trans = 7, // corresponding transition state
    _thread_in_Java = 8, // running in Java or in stub code
    _thread_in_Java_trans = 9, // corresponding transition state (not used, included for completness)
    _thread_blocked = 10, // blocked in vm
    _thread_blocked_trans = 11, // corresponding transition state
    _thread_max_state = 12  // maximum thread state+1 - used for statistics allocation
};

class Symbol {
public:
    unsigned short _length;
    short _refcount;
    int _identity_hash;
    char _body[1];
};

class JavaValue {
public:
    typedef union JavaCallValue {
        jfloat   f;
        jdouble  d;
        jint     i;
        jlong    l;
        jobject  h;
    } JavaCallValue;

    BasicType _type;
    JavaCallValue _value = { NULL };

    JavaValue(BasicType t = T_ILLEGAL) { _type = t; }

    JavaValue(jfloat value) {
        _type = T_FLOAT;
        _value.f = value;
    }

    JavaValue(jdouble value) {
        _type = T_DOUBLE;
        _value.d = value;
    }

    jfloat get_jfloat() const { return _value.f; }
    jdouble get_jdouble() const { return _value.d; }
    jint get_jint() const { return _value.i; }
    jlong get_jlong() const { return _value.l; }
    jobject get_jobject() const { return _value.h; }
    JavaCallValue* get_value_addr() { return &_value; }
    BasicType get_type() const { return _type; }

    void set_jfloat(jfloat f) { _value.f = f; }
    void set_jdouble(jdouble d) { _value.d = d; }
    void set_jint(jint i) { _value.i = i; }
    void set_jlong(jlong l) { _value.l = l; }
    void set_jobject(jobject h) { _value.h = h; }
    void set_type(BasicType t) { _type = t; }
};

class JavaCallArguments {
public:
    enum Constants {
        _default_size = 8    // Must be at least # of arguments in JavaCalls methods
    };

    intptr_t    _value_buffer[_default_size + 1];
    u_char      _value_state_buffer[_default_size + 1];

    intptr_t* _value;
    u_char* _value_state;
    int         _size;
    int         _max_size;
    bool        _start_at_zero;      // Support late setting of receiver

    JavaCallArguments(int max_size) {
        if (max_size > _default_size) {
            _value = NEW_RESOURCE_ARRAY(intptr_t, max_size + 1);
            _value_state = NEW_RESOURCE_ARRAY(u_char, max_size + 1);

            // Reserve room for potential receiver in value and state
            _value++;
            _value_state++;

            _max_size = max_size;
            _size = 0;
            _start_at_zero = false;
        }
        else {
            initialize();
        }
    }

    void initialize() {
        // Starts at first element to support set_receiver.
        _value = &_value_buffer[1];
        _value_state = &_value_state_buffer[1];

        _max_size = _default_size;
        _size = 0;
        _start_at_zero = false;
    }
};

class ConstantPool {
public:
    void* vtable;
    void* _tags;
    void* _cache;
    void* _pool_holder;
    void* _operands;

    jobject _resolved_references;
    void* _reference_map;

    int _flags;  // old fashioned bit twiddling
    int _length; // number of elements in the array

    union {
        // set for CDS to restore resolved references
        int _resolved_reference_length;
        // keeps version number for redefined classes (used in backtrace)
        int _version;
    } _saved;

    void* _lock;

    intptr_t* base() const { return (intptr_t*)(((char*)this) + sizeof(ConstantPool)); }

    Symbol** symbol_at_addr(int which) const {
        return (Symbol**)&base()[which];
    }

    Symbol* symbol_at(int which) {
        return *symbol_at_addr(which);
    }
};

class ConstMethod {
public:
    uint64_t _fingerprint;
    ConstantPool* _constants;
    
    void* _stackmap_data;

    int _constMethod_size;
    u2 _flags;
    u1 _result_type;

    u2 _code_size;
    u2 _name_index;
    u2 _signature_index;
    u2 _method_idnum;

    u2 _max_stack;
    u2 _max_locals;
    u2 _size_of_parameters;
    u2 _orig_method_idnum;

    ConstantPool* constants() const { return _constants; }

    int signature_index() const { return _signature_index; }
};

class Method {
public:
    void* vtable;
    ConstMethod* _constMethod;

    inline static Method* resolve_jmethod_id(jmethodID mid) {
        return *((Method**)mid);
    }

    ConstMethod* constMethod() const { return _constMethod; }

    // constant pool for Klass* holding this method
    ConstantPool* constants() const { return constMethod()->constants(); }

    // signature
    int signature_index() const { return constMethod()->signature_index(); }
    Symbol* signature() const { return constants()->symbol_at(signature_index()); }

    int size_of_parameters() const { return constMethod()->_size_of_parameters; }
};

class SignatureIterator {
public:
    void* vftable = nullptr;
    Symbol* _signature;
    int _index = NULL;
    int _parameter_index;
    BasicType _return_type = BasicType();

    SignatureIterator(Symbol* signature) {
        _signature = signature;
        _parameter_index = 0;
    }

    BasicType get_ret_type() const { return _return_type; }
};

class JNI_ArgumentPusher : public SignatureIterator {
public:
    JavaCallArguments* _arguments;

    JNI_ArgumentPusher(Symbol* signature) : SignatureIterator(signature) {
        this->_return_type = T_ILLEGAL;
        _arguments = NULL;
    }

    void set_java_argument_object(JavaCallArguments* arguments) { _arguments = arguments; }
};

class JNI_ArgumentPusherVaArg : public JNI_ArgumentPusher {
private:
    typedef uint64_t(__fastcall* iterate_t)(JNI_ArgumentPusherVaArg* instance, uint64_t fingerprint);
public:
    va_list _ap;

    JNI_ArgumentPusherVaArg(jmethodID method_id, va_list rap)
        : JNI_ArgumentPusher(Method::resolve_jmethod_id(method_id)->signature()) {
        set_ap(rap);
        vftable = reinterpret_cast<void*>((BYTE*)jvm + JNI_ARGUMENT_PUSHER_VA_ARG);
    }

    inline void set_ap(va_list rap) {
#ifdef va_copy
        va_copy(_ap, rap);
#elif defined (__va_copy)
        __va_copy(_ap, rap);
#else
        _ap = rap;
#endif
    }

    void iterate(uint64_t fingerprint) {
        (*reinterpret_cast<iterate_t*>
            ((BYTE*)jvm + JNI_ARGUMENT_PUSHER_VA_ARG + JNI_ARGUMENT_PUSHER_VA_ARG_ITERATE_VFTABLE_OFFSET))(this, fingerprint);
    }
};

class Chunk {
private:
    typedef void* (__fastcall* next_chop_t)(Chunk* instance);
public:
    Chunk* _next;
    size_t _len;

    void next_chop() {
        reinterpret_cast<next_chop_t>((BYTE*)jvm + CHUNK_NEXT_CHOP)(this);
    }
};

class Arena {
private:
    typedef size_t(__fastcall* set_size_in_bytes_t)(Arena* instance, size_t size);
public:
    uint32_t _flags;
    void* _first;
    Chunk* _chunk;
    char *_hwm, *_max;
    size_t _size_in_bytes;

    void set_size_in_bytes(size_t size) {
        reinterpret_cast<set_size_in_bytes_t>((BYTE*)jvm + ARENA_SET_SIZE_IN_BYTES)(this, size);
    }
};

class ResourceArea : public Arena {};

class GenericGrowableArray {
public:
    int _len;
    int _max;
    Arena* _arena;
    uint32_t _mem_flags;
};

template<typename E>
class GrowableArray : public GenericGrowableArray {
private:
    typedef void* (__fastcall* grow_t)(GrowableArray<E>* instance, int j);

    E* _data;
public:
    void grow(int j) {
        reinterpret_cast<grow_t>((BYTE*)jvm + GROWABLE_ARRAY_GROW)(this, j);
    }

    int append(const E& elem) {
        if (_len == _max) grow(_len);
        int idx = _len++;
        _data[idx] = elem;
        return idx;
    }

    void push(const E& elem) { append(elem); }
};

class HandleMark {
public:
    JavaThread* _thread;
    Arena* _area;
    Chunk* _chunk;
    char* _hwm, * _max;
    size_t _size_in_bytes;
    HandleMark* _previous_handle_mark;

    void pop_and_restore() {
        if (_chunk->_next) {
            _area->set_size_in_bytes(_size_in_bytes);
            _chunk->next_chop();
        }

        _area->_chunk = _chunk;
        _area->_hwm = _hwm;
        _area->_max = _max;
    }
};

class JavaThread {
private:
    typedef void(__fastcall* check_safepoint_and_suspend_for_native_trans_t)(JavaThread* thread);
public:
    enum SuspendFlags {
        // NOTE: avoid using the sign-bit as cc generates different test code
        //       when the sign-bit is used, and sometimes incorrectly - see CR 6398077
        _external_suspend = 0x20000000U, // thread is asked to self suspend
        _ext_suspended = 0x40000000U, // thread has self-suspended
        _deopt_suspend = 0x10000000U, // thread needs to self suspend for deopt

        _has_async_exception = 0x00000001U, // there is a pending async exception
        _critical_native_unlock = 0x00000002U, // Must call back to unlock JNI critical lock
    };

    static JavaThread* thread_from_jni_environment(JNIEnv* env) {
        return (JavaThread*)((BYTE*)env - 504);
    }

    void set_thread_state(JavaThreadState s) {
        *reinterpret_cast<JavaThreadState*>((BYTE*)this + 624) = s;
    }

    JavaThreadState get_thread_state() {
        return *reinterpret_cast<JavaThreadState*>((BYTE*)this + 624);
    }

    uint32_t suspend_flags() {
        return *(uint32_t*)((BYTE*)this + 48);
    }

    bool is_suspend_after_native() {
        return (suspend_flags() & (_external_suspend | _deopt_suspend)) != 0;
    }

    static void check_safepoint_and_suspend_for_native_trans(JavaThread* thread) {
        return reinterpret_cast<check_safepoint_and_suspend_for_native_trans_t>(
            (BYTE*)jvm + JAVA_THREAD_CHECK_SAFEPOINT_AND_SUSPEND_FOR_NATIVE_TRANS)(thread);
    }

    bool has_pending_exception() const {
        return *reinterpret_cast<void**>((BYTE*)this + 8) != NULL;
    }

    HandleMark* last_handle_mark() {
        return *(HandleMark**)((BYTE*)this + 72);
    }

    GrowableArray<void*>* metadata_handles() {
        return *(GrowableArray<void*>**)((BYTE*)this + 320);
    }

    ResourceArea* resource_area() {
        return *(ResourceArea**)((BYTE*)this + 304);
    }
};

class ResourceMark {
public:
    ResourceArea* _area;
    Chunk* _chunk;
    char* _hwm, * _max;
    size_t _size_in_bytes;

    JavaThread* _thread;

    ResourceMark(JavaThread* thread) {
        this->_area = thread->resource_area();
        this->_chunk = _area->_chunk;
        this->_hwm = _area->_hwm;
        this->_max = _area->_max;
        this->_size_in_bytes = _area->_size_in_bytes;
        this->_thread = thread;
    }

    ~ResourceMark() {
        reset_to_mark();
    }

    void reset_to_mark() {
        if (_chunk->_next) {
            _area->set_size_in_bytes(_size_in_bytes);
            _chunk->next_chop();
        }

        _area->_chunk = _chunk;
        _area->_hwm = _hwm;
        _area->_max = _max;
    }
};

class methodHandle {
private:
    typedef void*(__fastcall* remove_t)(methodHandle* instance);
public:
    Method* _value;
    JavaThread* _thread;

    methodHandle() : _value(nullptr), _thread(nullptr) {}

    methodHandle(JavaThread* thread, Method* obj) : _value(obj), _thread(thread) {
        if (obj)
            thread->metadata_handles()->push(obj);
    }

    methodHandle(const methodHandle& h) {
        _value = h._value;
        if (_value) {
            _thread = h._thread;
            _thread->metadata_handles()->push(_value);
        }
        else {
            _thread = NULL;
        }
    }

    ~methodHandle() { remove(); }

    void remove() {
        reinterpret_cast<remove_t>((BYTE*)jvm + METHOD_HANDLE_REMOVE)(this);
    }

    Method* operator -> () const { return _value; }
    Method* operator () () const { return _value; } \

    methodHandle& operator=(const methodHandle& s) {
        remove();
        _value = s._value;

        if (_value) {
            _thread = s._thread;
            _thread->metadata_handles()->push(_value);
        }
        else {
            _thread = NULL;
        }

        return *this;
    }
};

class Fingerprinter : public SignatureIterator {
private:
    typedef uint64_t(__fastcall* fingerprint_t)(Fingerprinter* instance);
public:
    uint64_t _fingerprint;
    int _shift_count;
    methodHandle mh;

    Fingerprinter(methodHandle method) : SignatureIterator(method->signature()) {
        vftable = reinterpret_cast<void*>((BYTE*)jvm + FINGERPRINTER);
        mh = method;
        _fingerprint = NULL;
        _shift_count = NULL;
    }

    uint64_t fingerprint() {
        return reinterpret_cast<fingerprint_t>((BYTE*)jvm + FINGERPRINTER_FINGERPRINT)(this);
    }
};

class os {
public:
    static int processor_count() {
        return *(int*)((BYTE*)jvm + OS_PROCESSOR_COUNT);
    }

    static inline bool is_MP() {
        return (processor_count() != 1) || *AssumeMP;
    }
};

class OrderAccess {
private:
    typedef void* (*fence_t)(void);
public:
    static void* fence() {
        return reinterpret_cast<fence_t>((BYTE*)jvm + ORDER_ACCESS_FENCE)();
    }
};

class InterfaceSupport {
private:
    typedef void* (_fastcall* serialize_memory_t)(JavaThread* thread);
public:
    static void* serialize_memory(JavaThread* thread) {
        return reinterpret_cast<serialize_memory_t>((BYTE*)jvm + INTERFACE_SUPPORT_SERIALIZE_MEMORY)(thread);
    }
};

class SafepointSynchronize {
private:
    typedef void(__fastcall* block_t)(JavaThread* thread);
public:
    enum SynchronizeState {
        _not_synchronized = 0,                   // Threads not synchronized at a safepoint
        // Keep this value 0. See the coment in do_call_back()
        _synchronizing = 1,                   // Synchronizing in progress
        _synchronized = 2                    // All Java threads are stopped at a safepoint. Only VM thread is running
    };

    static SynchronizeState state() {
        return *(SynchronizeState*)((BYTE*)jvm + SAFEPOINT_SYNCHRONIZE_STATE);
    }

    static bool do_call_back() {
        return state() != _not_synchronized;
    }

    static void block(JavaThread* thread) {
        reinterpret_cast<block_t>((BYTE*)jvm + SAFEPOINT_SYNCHRONIZE_BLOCK)(thread);
    }
};

class ThreadStateTransition {
public:
    static inline void transition_from_native(JavaThread* thread, JavaThreadState to) {
        thread->set_thread_state(_thread_in_native_trans);

        if (os::is_MP()) {
            if (*UseMembar) OrderAccess::fence();
            else InterfaceSupport::serialize_memory(thread);
        }

        if (SafepointSynchronize::do_call_back() || thread->is_suspend_after_native())
            JavaThread::check_safepoint_and_suspend_for_native_trans(thread);

        thread->set_thread_state(to);
    }

    static inline void transition_and_fence(JavaThread* thread, JavaThreadState from, JavaThreadState to) {
        thread->set_thread_state(from);

        if (os::is_MP()) {
            if (*UseMembar) OrderAccess::fence();
            else InterfaceSupport::serialize_memory(thread);
        }

        if (SafepointSynchronize::do_call_back())
            SafepointSynchronize::block(thread);

        thread->set_thread_state(to);
    }
};

class WeakPreserveExceptionMark {
private:
    typedef void* (__fastcall* preserve_t)(WeakPreserveExceptionMark* instance);
    typedef void(__fastcall* restore_t)(WeakPreserveExceptionMark* instance);
public:
    JavaThread* _thread;
    void* _preserved_exception_oop;
    int _preserved_exception_line = NULL;
    const char* _preserved_exception_file = nullptr;

    WeakPreserveExceptionMark(JavaThread* thread) {
        _thread = thread;
        _preserved_exception_oop = nullptr;

        if (_thread->has_pending_exception())
            reinterpret_cast<preserve_t>((BYTE*)jvm + WEAK_PRESERVE_EXCEPTION_MARK_PRESERVE)(this);
    }

    ~WeakPreserveExceptionMark() {
        if (_preserved_exception_oop)
            reinterpret_cast<restore_t>((BYTE*)jvm + WEAK_PRESERVE_EXCEPTION_MARK_RESTORE)(this);
    }
};

class HandleMarkCleaner {
public:
    JavaThread* _thread;
    
    HandleMarkCleaner(JavaThread* thread) {
        _thread = thread;
    }

    ~HandleMarkCleaner() {
        _thread->last_handle_mark()->pop_and_restore();
    }
};

class ThreadInVMfromNative {
public:
    JavaThread* _thread;

    ThreadInVMfromNative(JavaThread* thread) {
        _thread = thread;
        ThreadStateTransition::transition_from_native(thread, _thread_in_vm);
    }

    ~ThreadInVMfromNative() {
        ThreadStateTransition::transition_and_fence(_thread, _thread_in_vm_trans, _thread_in_native);
    }
};

class JavaCalls {
private:
    typedef void*(__fastcall* call_t)(JavaValue* result, methodHandle* method, JavaCallArguments* args, JavaThread* thread);
public:
    //static void call(JavaValue* result, methodHandle method, JavaCallArguments* args, JavaThread* thread) {
    //    reinterpret_cast<call_t>((BYTE*)jvm + JAVA_CALLS_CALL)(result, &method, args, thread);
    //}

    static void call(JavaValue* result, methodHandle method, JavaCallArguments* args, JavaThread* thread) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(method));

        reinterpret_cast<call_t>((BYTE*)jvm + JAVA_CALLS_CALL)(result, new_method, args, thread);

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
    }

};

class JNIHandles {
private:
    typedef jobject(__fastcall* make_local_t)(JNIEnv* env, void* obj);
public:
    static inline jobject make_local(JNIEnv* env, void* obj) {
        return reinterpret_cast<make_local_t>((BYTE*)jvm + JNI_HANDLES_MAKE_LOCAL)(env, obj);
    }
};