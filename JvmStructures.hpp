#include "Offsets.hpp"

bool* AssumeMP      = nullptr;
bool* UseMembar     = nullptr;
bool* CheckJNICalls = nullptr;

typedef unsigned char  u1;
typedef unsigned short u2;

class JavaCallArguments;
class JavaFrameAnchor;
class JavaValue;
class methodHandle;
class JavaThread;
class HandleMark;

extern void call_helper(JavaValue* result, methodHandle* m, JavaCallArguments* args, JavaThread* thread);

#define JVM_ACC_STATIC 0x0008  /* instance variable is static */

#define NEW_RESOURCE_ARRAY(type, size)\
    (type*)resource_allocate_bytes(size * sizeof(type), AllocFailType::EXIT_OOM);

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

class AccessFlags {
public:
    jint _flags;

    bool is_static() const { return (_flags & JVM_ACC_STATIC) != 0; }
};

class Method {
private:
    typedef bool(__fastcall* is_empty_method_t)(const Method* instance);
public:
    void* vtable;
    ConstMethod* _constMethod;
    void* _method_data;
    void* _method_counters;
    AccessFlags _access_flags;
    int _vtable_index;

    u2 _method_size;
    u1 _intrinsic_id;
    u1 _jfr_towrite          : 1,   // Flags
       _caller_sensitive     : 1,
       _force_inline         : 1,
       _hidden               : 1,
       _running_emcp         : 1,
       _dont_inline          : 1,
       _has_injected_profile : 1,
                             : 2;

    void* _i2i_entry;
    void* _adapter;
    void* _from_compiled_entry;
    void* _code;
    void* _from_interpreted_entry;

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

    bool is_empty_method() const {
        return reinterpret_cast<is_empty_method_t>((BYTE*)jvm + METHOD_IS_EMPTY_METHOD)(this);
    }

    void* interpreter_entry() const { return _i2i_entry; }
    void* from_interpreted_entry() const { return _from_interpreted_entry; }

    AccessFlags access_flags() const { return _access_flags; }
    bool is_static() const { return access_flags().is_static(); }
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

class HandleArea : public Arena {
public:
    HandleArea* _prev;
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

class JNIHandleBlock {
private:
    typedef void(__fastcall* release_block_t)(JNIHandleBlock* block, JavaThread* thread);
public:
    static void release_block(JNIHandleBlock* block, JavaThread* thread = nullptr) {
        reinterpret_cast<release_block_t>((BYTE*)jvm + JNI_HANDLE_BLOCK_RELEASE_BLOCK)(block, thread);
    }
};

class JavaThread {
private:
    typedef void(__fastcall* check_safepoint_and_suspend_for_native_trans_t)(JavaThread* thread);
    typedef bool(__fastcall* reguard_stack_t)(JavaThread* instance);
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

    enum StackGuardState {
        stack_guard_unused,         // not needed
        stack_guard_yellow_disabled,// disabled (temporarily) after stack overflow
        stack_guard_enabled         // enabled
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

    void set_last_handle_mark(HandleMark* mark) {
        *(HandleMark**)((BYTE*)this + 72) = mark;
    }

    GrowableArray<void*>* metadata_handles() {
        return *(GrowableArray<void*>**)((BYTE*)this + 320);
    }

    ResourceArea* resource_area() {
        return *(ResourceArea**)((BYTE*)this + 304);
    }

    bool is_interp_only_mode() {
        return (*(int*)((BYTE*)this + 920) != 0);
    }

    inline bool stack_yellow_zone_disabled() {
        return (*(StackGuardState*)((BYTE*)this + 668)) == stack_guard_yellow_disabled;
    }

    bool reguard_stack() {
        return reinterpret_cast<reguard_stack_t>((BYTE*)jvm + JAVA_THREAD_REGUARD_STACK)(this);
    }

    JNIHandleBlock* active_handles() { return *(JNIHandleBlock**)((BYTE*)this + 56); }
    void set_active_handles(JNIHandleBlock* block) { *(JNIHandleBlock**)((BYTE*)this + 56) = block; }

    JavaFrameAnchor* frame_anchor() {
        return (JavaFrameAnchor*)((BYTE*)this + 472);
    }

    HandleArea* handle_area() {
        return *(HandleArea**)((BYTE*)this + 312);
    }

    void*  vm_result() const { return *(void**)((BYTE*)this + 568); }
    void set_vm_result(void* x) { *(void**)((BYTE*)this + 568) = x; }
};

class HandleMark {
public:
    JavaThread* _thread;
    HandleArea* _area;
    Chunk* _chunk;
    char* _hwm, * _max;
    size_t _size_in_bytes;
    HandleMark* _previous_handle_mark;

    HandleMark(JavaThread* thread) { initialize(thread); }

    ~HandleMark() {
        HandleArea* area = _area;

        if (_chunk->_next) {
            _area->set_size_in_bytes(_size_in_bytes);
            _chunk->next_chop();
        }

        area->_chunk = _chunk;
        area->_hwm = _hwm;
        area->_max = _max;

        _thread->set_last_handle_mark(_previous_handle_mark);
    }

    void set_previous_handle_mark(HandleMark* mark) { _previous_handle_mark = mark; }

    void initialize(JavaThread* thread) {
        _thread = thread;
        _area = thread->handle_area();

        _chunk = _area->_chunk;
        _hwm = _area->_hwm;
        _max = _area->_max;
        _size_in_bytes = _area->_size_in_bytes;

        set_previous_handle_mark(thread->last_handle_mark());
        thread->set_last_handle_mark(this);
    }

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

class Handle {
public:
    void* _handle;

    Handle() { _handle = NULL; }

    // Direct interface, use very sparingly.
    // Used by JavaCalls to quickly convert handles and to create handles static data structures.
    // Constructor takes a dummy argument to prevent unintentional type conversion in C++.
    Handle(void* handle, bool dummy) { _handle = handle; }
};

class JavaCallArguments {
private:
    typedef void* (__fastcall* verify_t)(JavaCallArguments* instance, methodHandle* method, BasicType return_type, JavaThread* thread);
    typedef intptr_t* (__fastcall* parameters_t)(JavaCallArguments* instance);
public:
    enum Constants {
        _default_size = 8    // Must be at least # of arguments in JavaCalls methods
    };

    // The possible values for _value_state elements.
    enum {
        value_state_primitive,
        value_state_oop,
        value_state_handle,
        value_state_jobject,
        value_state_limit
    };

#pragma warning(suppress:26495)
    intptr_t    _value_buffer[_default_size + 1] = { NULL };
#pragma warning(suppress:26495)
    u_char      _value_state_buffer[_default_size + 1] = { NULL };

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

    void verify(methodHandle method, BasicType return_type, JavaThread* thread) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(method));

        reinterpret_cast<verify_t>((BYTE*)jvm + JAVA_CALL_ARGUMENTS_VERIFY)(this, new_method, return_type, thread);

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
    }

    // receiver
    Handle receiver() {
        return Handle((void*)_value[0], false);
    }

    intptr_t* parameters() {
        return reinterpret_cast<parameters_t>((BYTE*)jvm + JAVA_CALL_ARGUMENTS_PARAMETERS)(this);
    }

    int size_of_parameters() const { return _size; }
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
private:
    typedef bool(__fastcall* stack_shadow_pages_available_t)(JavaThread* thread, methodHandle* method);
    typedef void*(__fastcall* bang_stack_shadow_pages_t)(void);
public:
    static int processor_count() {
        return *(int*)((BYTE*)jvm + OS_PROCESSOR_COUNT);
    }

    static inline bool is_MP() {
        return (processor_count() != 1) || *AssumeMP;
    }

    static bool stack_shadow_pages_available(JavaThread* thread, methodHandle method) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(method));

        bool result = reinterpret_cast<stack_shadow_pages_available_t>((BYTE*)jvm + OS_STACK_SHADOW_PAGES_AVAILABLE)(
            thread, new_method
        );

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
        return result;
    }

    static void bang_stack_shadow_pages() {
        reinterpret_cast<bang_stack_shadow_pages_t>((BYTE*)jvm + OS_BANG_STACK_SHADOW_PAGES)();
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

    // Same as above, but assumes from = _thread_in_Java. This is simpler, since we
    // never block on entry to the VM. This will break the code, since e.g. preserve arguments
    // have not been setup.
    static inline void transition_from_java(JavaThread* thread, JavaThreadState to) {
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
        //auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        //new (new_method) methodHandle(std::move(method));

        //reinterpret_cast<call_t>((BYTE*)jvm + JAVA_CALLS_CALL)(result, new_method, args, thread);
        call_helper(result, &method, args, thread);

        //operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
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

class CompilationPolicy {
private:
    typedef bool(__fastcall* must_be_compiled_t)(methodHandle* m);
    typedef int(__fastcall* initial_compile_level_t)(CompilationPolicy* instance);
public:
    void* vftable;

    static bool must_be_compiled(methodHandle m) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(m));

        bool result = reinterpret_cast<must_be_compiled_t>((BYTE*)jvm + COMPILATION_POLICY_MUST_BE_COMPILED)(new_method);

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается

        return result;
    }


    static CompilationPolicy* policy() { 
        return *reinterpret_cast<CompilationPolicy**>((BYTE*)jvm + COMPILATION_POLICY_POLICY);
    }

    int initial_compile_level() {
        return (*reinterpret_cast<initial_compile_level_t*>(this->vftable))(this);
    }
};

class CompileBroker {
    typedef void* (__fastcall* compile_method_t)(
        methodHandle* method,
        int osr_bci,
        int comp_level,
        methodHandle* hot_method,
        int hot_count,
        const char* comment,
        JavaThread* thread
    );
public:
    static void* compile_method(
        methodHandle method,
        int osr_bci,
        int comp_level,
        methodHandle hot_method,
        int hot_count,
        const char* comment,
        JavaThread* thread
    ) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(method));

        auto* new_hot_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_hot_method) methodHandle(std::move(hot_method));

        void* result = reinterpret_cast<compile_method_t>((BYTE*)jvm + COMPILE_BROKER_COMPILE_METHOD)(
            new_method,
            osr_bci,
            comp_level,
            new_hot_method,
            hot_count,
            comment,
            thread
            );

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
        operator delete(new_hot_method, sizeof(methodHandle)); // Деструктор НЕ вызывается

        return result;
    }
};

class JvmtiExport {
public:
    inline static bool can_post_interpreter_events() {
        return *(bool*)((BYTE*)jvm + JVMTI_EXPORT_CAN_POST_INTERPRETER_EVENTS);
    }
};

class Exceptions {
private:
    typedef void* (__fastcall* throw_stack_overflow_exception_t)(JavaThread* thread, const char* file, int line, methodHandle* method);
public:
    static void throw_stack_overflow_exception(JavaThread* thread, const char* file, int line, methodHandle method) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(method));

        reinterpret_cast<throw_stack_overflow_exception_t>((BYTE*)jvm + EXCEPTIONS_THROW_STACK_OVERFLOW_EXCEPTION)(
            thread, file, line, new_method
        );

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
    }
};

class JavaFrameAnchor {
public:
    intptr_t* volatile _last_Java_sp;
    volatile void* _last_Java_pc;
    intptr_t* volatile _last_Java_fp;

    void zap(void) { _last_Java_sp = NULL; }

    void copy(JavaFrameAnchor* src) {
        if (_last_Java_sp != src->_last_Java_sp)
            _last_Java_sp = NULL;

        _last_Java_fp = src->_last_Java_fp;
        _last_Java_pc = src->_last_Java_pc;
        _last_Java_sp = src->_last_Java_sp;
    }
};

class JavaCallWrapper {
private:
    typedef JavaCallWrapper* (__fastcall* JavaCallWrapper_ctor_t)(JavaCallWrapper* instance, methodHandle* callee_method, Handle receiver, JavaValue* result, JavaThread* thread);
public:
    JavaThread* _thread = nullptr;
    JNIHandleBlock* _handles = nullptr;
    Method* _callee_method = nullptr;
    void* _receiver = nullptr;
    JavaFrameAnchor _anchor = { NULL };
    JavaValue* result = nullptr;

    JavaCallWrapper(methodHandle callee_method, Handle receiver, JavaValue* result, JavaThread* thread) {
        auto* new_method = static_cast<methodHandle*>(operator new(sizeof(methodHandle)));
        new (new_method) methodHandle(std::move(callee_method));

        reinterpret_cast<JavaCallWrapper_ctor_t>((BYTE*)jvm + JAVA_CALL_WRAPPER_CONSTRUCTOR)(
            this, new_method, receiver, result, thread
        );

        operator delete(new_method, sizeof(methodHandle)); // Деструктор НЕ вызывается
    }

    ~JavaCallWrapper() {
        JNIHandleBlock* _old_handles = _thread->active_handles();
        _thread->set_active_handles(_handles);

        _thread->frame_anchor()->zap();
        ThreadStateTransition::transition_from_java(_thread, _thread_in_vm);

        _thread->frame_anchor()->copy(&_anchor);
        JNIHandleBlock::release_block(_old_handles, _thread);
    }
};

class StubRoutines {
public:
    // Calls to Java
    typedef void (*CallStub)(
        void*       link,
        intptr_t*   result,
        BasicType   result_type,
        Method*     method,
        void*       entry_point,
        intptr_t*   parameters,
        int         size_of_parameters,
        JavaThread* thread
    );

    static CallStub call_stub() { return *reinterpret_cast<CallStub*>((BYTE*)jvm + STUB_ROUTINES_CALL_STUB_ENTRY); }
};