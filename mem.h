#pragma once

// TODO: Remove this and the debug messages:
#include <iostream>

#include "forward.h"
#include "util.h"

template<typename T>
class TypedAlignedStorage {
public:
	T& value() { return *(T*)(&obj); }
	const T& value() const { return *(T*)(&obj); }

	template<typename ...Args>
	void construct(Args&& ...args) {
		new((T*)&obj) T(std::forward<Args>(args)...);
	}

	void destroy() {
		value.~T();
	}

private:
	std::aligned_storage_t<sizeof(T), alignof(T)> obj;
};


// Derive from this class to change the ref count of a ref-counted object
class RefManager {
protected:
	template<typename T>
	u64 ref(RefCounted<T>& r) {
		return r.ref();
	}

	template<typename T>
	u64 unref(RefCounted<T>& r) {
		return r.unref();
	}

	template<typename T>
	u64 refCount(const RefCounted<T>& r) const {
		return r.refCount();
	}
};


// Derive from this class to be life-time managed by a shared-pointer
template< typename T >
class RefCounted {

private:
	u64 ref() {
		std::cout << "up\n";
		return ++refCounter;
	}

	u64 unref() {
		std::cout << "down\n";
		return --refCounter;
	}

	u64 refCount() const {
		return refCounter;
	}

	friend RefManager;
	u64 refCounter{ 0 };
};

namespace Memory {

	// Deletes an object
	template<typename T>
	struct ObjectDeleter {
		void operator()(T* obj) const { delete obj; }
	};

	// Deletes an array of objects
	template<typename T>
	struct ArrayDeleter {
		void operator()(T* obj) const { delete[] obj; }
	};


	// Base class of all owning pointer types
	template<typename T, typename TDeleter>
	class OwnPtrBase {
	public:
		OwnPtrBase() : obj(nullptr) {}

		OwnPtrBase(const OwnPtrBase& x) = delete;
		OwnPtrBase(OwnPtrBase&& x) : obj(x.release()) {}
		explicit OwnPtrBase(T* o) : obj(o) {}

		~OwnPtrBase() { reset(); }

		T* release() {
			T* o = obj;
			obj = nullptr;
			return o;
		}

		void reset(T* n = nullptr) {
			if (obj) {
				TDeleter del;
				del(obj);
			}
			obj = n;
		}

		T* ptr() { return obj; }

		T* operator->() { return obj; }
		T& operator*() { return *obj; }

		operator bool() const { return static_cast<bool>(obj); }

	private:
		T* obj{ nullptr };
	};

}

// Owning pointer to an object
template<typename T>
class OwnPtr final : public Memory::OwnPtrBase<T, Memory::ObjectDeleter<T>> {
public:
	explicit OwnPtr(T* o) : OwnPtrBase<T, Memory::ObjectDeleter<T>>(o) {}
	OwnPtr(OwnPtr&& p) : OwnPtrBase<T, Memory::ObjectDeleter<T>>(std::move(p)) {}
};

// Owning pointer to an array of objects
template<typename T>
class OwnPtr<T[]> final : public Memory::OwnPtrBase<T, Memory::ArrayDeleter<T>> {
public:
	explicit OwnPtr(T* o) : OwnPtrBase<T, Memory::ObjectDeleter<T>>(o) {}
	OwnPtr(OwnPtr&& p) : OwnPtrBase<T, Memory::ObjectDeleter<T>>(std::move(p)) {}

	T& operator[] (u64 idx) { return this->ptr[idx]; }
	const T& operator[] (u64 idx) const { return this->ptr[idx]; }
};


// Wraps a non-ref-counted object and make it ref-counted
// eg. for arrays of objects
template<typename T>
class Shared : public RefCounted<Shared<T>> {
public:

	T value;
};

// Wraps an array of object and makes it ref-counted
template<typename T>
class Shared<T[]> : public RefCounted<Shared<T[]>> {
private:
	Shared(u64 sz) : itemCount(sz) {
		// Construct all objects with their default (not POD-value) constructor
		for (u64 i = 0; i != itemCount; i++) {
			new(value + i) T;
		}
	}


public:
	~Shared() {
		// Destruct all objects
		for (u64 i = 0; i != itemCount; i++) {
			value[i].~T();
		}
	}

	static OwnPtr<Shared<T[]>> make(u64 cnt) {
		auto size = sizeof(Shared<T[]>) + sizeof(T) * cnt; std::cout << "Alloc " << size << " bytes of memory\n";
		auto mem = new u8[size];
		auto obj = new(mem) Shared<T[]>(cnt);

		return OwnPtr<Shared<T[]>>(obj);
	}

	T& operator[] (u64 idx) { assert(idx < itemCount); return value[idx]; }
	const T& operator[] (u64 idx) const { assert(idx < itemCount);  return value[idx]; }

	u64 size() const { return itemCount; }

private:
	const u64 itemCount;

public:
	T value[]; // The space following is the actual array of objects
};

namespace Memory {

	// Base class of all shared pointers
	template<typename T>
	class SharedPtrBase : public RefManager {
	public:
		SharedPtrBase() : obj(nullptr) {}

		SharedPtrBase(OwnPtr<T>&& p) : obj(p.release()) {
			ref(*obj);
		}
		SharedPtrBase(const SharedPtr<T>& s) : obj(s.obj) {
			ref(*obj);
		}
		SharedPtrBase(SharedPtr<T>&& s) : obj(s.obj) {
			s.obj = nullptr;
		}

		~SharedPtrBase() {
			reset();
		}

		void reset(T* n = nullptr) {
			if (obj) {
				if (!unref(*obj)) {
					delete obj;
				}
			}

			obj = n;
			if (obj) {
				ref(*obj);
			}
		}

		T* ptr() { return obj; }
		const T* ptr() const { return obj; }

		T* operator->() { return obj; }
		T& operator*() { return *obj; }

		const T* operator->() const { return obj; }
		const T& operator*() const { return *obj; }

		u64 refCount() const {
			return obj ? RefManager::refCount(*obj) : 0;
		}

		OwnPtr<T> tryOwning() {
			if (refCount() > 1) {
				return {};
			}

			T* p = obj;
			obj = nullptr;
			return { p };
		}

		operator bool() const { return static_cast<bool>(obj); }


	protected:
		void assign(const SharedPtr<T>& p) {
			reset(p.obj);
		}

		void assign(SharedPtr<T>&& p) {
			reset(nullptr);
			obj = p.obj;
			p.obj = nullptr;
		}

		void assign(OwnPtr<T>&& p) {
			reset(p.release());
		}

		T* obj{ nullptr };
	};

}

// Shared pointer to a ref-counted object
template<typename T>
class SharedPtr final : public Memory::SharedPtrBase<T> {
public:
	SharedPtr() : SharedPtrBase() {};
	SharedPtr(const SharedPtr& s) : SharedPtrBase(s) {}
	SharedPtr(SharedPtr&& s) : SharedPtrBase(std::move(s)) {}
	SharedPtr(OwnPtr<T>&& p) : SharedPtrBase(std::move(p)) {}

	SharedPtr& operator=(const SharedPtr& p) { assign(p); return *this; }
	SharedPtr& operator=(SharedPtr&& p) { assign(std::move(p)); return *this; }
	SharedPtr& operator=(OwnPtr<T>&& p) { assign(std::move(p)); return *this; }
};

// Shared pointer to a wrapped non-ref-counted object
template<typename T>
class SharedPtr<Shared<T>> final : public Memory::SharedPtrBase<Shared<T>> {
	using TV = Shared<T>;
public:
	SharedPtr() : SharedPtrBase<TV>() {};
	SharedPtr(const SharedPtr& s) : SharedPtrBase<TV>(s) {}
	SharedPtr(SharedPtr&& s) : SharedPtrBase<TV>(std::move(s)) {}
	SharedPtr(OwnPtr<Shared<T>>&& p) : SharedPtrBase<TV>(std::move(p)) {}

	SharedPtr& operator=(const SharedPtr& p) { assign(p); return *this; }
	SharedPtr& operator=(SharedPtr&& p) { assign(std::move(p)); return *this; }
	SharedPtr& operator=(OwnPtr<Shared<T>>&& p) { assign(std::move(p)); return *this; }

	T* operator->() { return &(this->obj->value); }
	T& operator*() { return this->obj->value; }

	const T* operator->() const { return &(this->obj->value); }
	const T& operator*() const { return this->obj->value; }
};


// Shared pointer to a wrapped dynamic array of objects
template<typename T>
class SharedPtr<Shared<T[]>> final : public Memory::SharedPtrBase<Shared<T[]>> {
	using TV = Shared<T[]>;
public:
	SharedPtr() : SharedPtrBase<TV>() {};
	SharedPtr(const SharedPtr& s) : SharedPtrBase<TV>(s) {}
	SharedPtr(SharedPtr&& s) : SharedPtrBase<TV>(std::move(s)) {}
	SharedPtr(OwnPtr<Shared<T[]>>&& p) : SharedPtrBase<TV>(std::move(p)) {}

	SharedPtr& operator=(const SharedPtr& p) { this->assign(p); return *this; }
	SharedPtr& operator=(SharedPtr&& p) { this->assign(std::move(p)); return *this; }
	SharedPtr& operator=(OwnPtr<Shared<T[]>>&& p) { this->assign(std::move(p)); return *this; }

	T* operator->() { return this->obj->value; }
	T& operator*() { return *(this->obj->value); }

	const T* operator->() const { return this->obj->value; }
	const T& operator*() const { return *(this->obj->value); }

	T* dataPtr() { return this->obj->value; }
	const T* dataPtr() const { return this->obj->value; }

	T& operator[](u64 idx) { return (*(this->obj))[idx]; }
	const T& operator[](u64 idx) const { return (*(this->obj))[idx]; }
};