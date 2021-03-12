#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

#include <dlfcn.h>
#include <linux/limits.h>

#ifndef DLPP_NOEXCEPTIONS
#include <stdexcept>
#endif

#ifdef _GNU_SOURCE
#include <iterator>
#include <link.h>
#include <string>
#include <string_view>
#endif

namespace dlpp {
#ifndef DLPP_NOEXCEPTIONS
    class dl_error : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };
#endif

    namespace detail {
#ifndef DLPP_NOEXCEPTIONS
        inline void throw_dl_error(const char* msg = dlerror()) { throw dl_error(msg); }
        inline const char* dl_error_message() { return dlerror(); }
#else
        inline void throw_dl_error(const char* = nullptr) { }
        inline const char* dl_error_message() { return nullptr; }
#endif

        template <typename To, typename From>
        static To transmute(From x)
        {
            static_assert(sizeof(To) == sizeof(From));
            static_assert(std::is_trivially_copyable_v<From>);
            static_assert(std::is_trivially_copyable_v<To>);
            To r;

            std::memcpy(&r, &x, sizeof(From));

            return r;
        }
    }

    inline const char* dl_error_message() { return dlerror(); }

    enum class dl_flags : int
    {
        lazy = RTLD_LAZY,
        now = RTLD_NOW,
        global = RTLD_GLOBAL,
        local = RTLD_LOCAL,
        nodelete = RTLD_NODELETE,
        noload = RTLD_NOLOAD,
        deepbind = RTLD_DEEPBIND,
    };

    inline dl_flags operator|(dl_flags lhs, dl_flags rhs)
    {
        return (dl_flags)((int)lhs | (int)rhs);
    }

    class lmid_t
    {
        friend class dl;

    private:
        Lmid_t value;

        explicit lmid_t(Lmid_t value)
            : value(value)
        { }

    public:
        friend bool operator==(lmid_t lhs, lmid_t rhs) { return lhs.value == rhs.value; }
        friend bool operator!=(lmid_t lhs, lmid_t rhs) { return lhs.value != rhs.value; }

        static const lmid_t base() { return lmid_t(LM_ID_BASE); }

        static const lmid_t newlm() { return lmid_t(LM_ID_NEWLM); }
    };

#ifdef _GNU_SOURCE
    class link_map
    {
        friend class dl;

    private:
        ::link_map* map_;

        explicit link_map(::link_map* map_)
            : map_(map_)
        { }

        struct sentinel
        { };

        class iterator
        {
            friend class link_map;

        private:
            ::link_map* map = nullptr;

            explicit iterator(::link_map* map)
                : map(map)
            { }

        public:
            using value_type = link_map;
            using reference = link_map;
            using pointer = link_map;
            using iterator_category = std::input_iterator_tag;

            iterator() = default;

            reference operator*() const { return link_map(map); }
            // pointer operator->() const { return map; }

            iterator& operator++()
            {
                map = map->l_next;
                return *this;
            }

            iterator operator++(int)
            {
                iterator cpy = *this;
                ++*this;
                return cpy;
            }

            iterator& operator--()
            {
                map = map->l_prev;
                return *this;
            }

            iterator operator--(int)
            {
                iterator cpy = *this;
                --*this;
                return cpy;
            }

            friend bool operator==(iterator lhs, iterator rhs) { return lhs.map == rhs.map; }
            friend bool operator!=(iterator lhs, iterator rhs) { return lhs.map != rhs.map; }
            friend bool operator==(iterator lhs, sentinel) { return lhs.map == nullptr; }
            friend bool operator!=(iterator lhs, sentinel rhs) { return !(lhs == rhs); }
            friend bool operator==(sentinel, iterator rhs) { return rhs.map == nullptr; }
            friend bool operator!=(sentinel lhs, iterator rhs) { return !(lhs == rhs); }
        };

    public:
        ElfW(Addr) addr() const { return map_->l_addr; }

        std::string_view name() const { return map_->l_name; }

        ElfW(Dyn) * ld() const { return map_->l_ld; }

        explicit operator bool() const { return static_cast<bool>(map_); }
        link_map next() const { return link_map(map_->l_next); }
        link_map prev() const { return link_map(map_->l_prev); }

        iterator begin() const { return iterator(map_); }
        sentinel end() const { return {}; }
    };

    class serinfo;

    class serpath
    {
        friend class serinfo;

    private:
        std::string_view name_;
        unsigned int flags_;

        explicit serpath(Dl_serpath path)
            : name_(path.dls_name)
            , flags_(path.dls_flags)
        { }

    public:
        std::string_view name() const { return name_; }
        unsigned int flags() const { return flags_; }
    };

    class serinfo
    {
        friend class dl;

    private:
        std::unique_ptr<std::byte[]> serinfo_struct = nullptr;

    public:
        unsigned int cnt() const
        {
            return reinterpret_cast<Dl_serinfo*>(serinfo_struct.get())->dls_cnt;
        }

        dlpp::serpath serpath(unsigned int index) const
        {
            return dlpp::serpath(
                reinterpret_cast<Dl_serinfo*>(serinfo_struct.get())->dls_serpath[index]);
        }
    };
#endif

    class dl
    {
    private:
        struct dl_deleter
        {
            void operator()(void* handle) const
            {
                if (handle && handle != RTLD_NEXT) {
                    int code = dlclose(handle);
                    if (code) detail::throw_dl_error();
                }
            }
        };

        std::unique_ptr<void, dl_deleter> handle;

        explicit dl(void* handle)
            : handle(handle)
        { }

    public:
        explicit dl(const char* path, dl_flags flags)
            : handle(dlopen(path, (int)flags))
        {
            if (!handle) detail::throw_dl_error();
        }

#ifdef _GNU_SOURCE
        explicit dl(lmid_t lmid, const char* path, dl_flags flags)
            : handle(dlmopen(detail::transmute<Lmid_t>(lmid), path, (int)flags))
        {
            if (!handle) detail::throw_dl_error();
        }
#endif

        template <typename T>
        T* sym(const char* symbol)
        {
            const char* msg = detail::dl_error_message();
            if (msg) {
                detail::throw_dl_error(msg);
                return nullptr;
            }

            T* result = reinterpret_cast<T*>(dlsym(handle.get(), symbol));
            if (!result) {
                msg = detail::dl_error_message();
                if (msg) {
                    detail::throw_dl_error(msg);
                    return nullptr;
                }
            }

            return result;
        }

#ifdef _GNU_SOURCE
        template <typename T>
        T* vsym(const char* symbol, const char* version)
        {
            const char* msg = detail::dl_error_message();
            if (msg) {
                detail::throw_dl_error(msg);
                return nullptr;
            }

            T* result = reinterpret_cast<T*>(dlvsym(handle.get(), symbol, version));
            if (!result) {
                msg = detail::dl_error_message();
                if (msg) {
                    detail::throw_dl_error(msg);
                    return nullptr;
                }
            }

            return result;
        }
#endif

        explicit operator bool() const { return handle != nullptr; }

        void close() { handle.reset(); }

        static dl next() { return dl(RTLD_NEXT); }
        static dl default_() { return dl(RTLD_DEFAULT); }

#ifdef _GNU_SOURCE
        lmid_t info_lmid() const
        {
            Lmid_t lmid = LM_ID_NEWLM;
            int code = dlinfo(handle.get(), RTLD_DI_LMID, &lmid);
            if (code) detail::throw_dl_error();
            return lmid_t(lmid);
        }

        link_map info_linkmap() const
        {
            ::link_map* result;

            int code = dlinfo(handle.get(), RTLD_DI_LINKMAP, &result);
            if (code) detail::throw_dl_error();

            return link_map(result);
        }

        std::string info_origin() const
        {
            std::string result;
            result.resize(PATH_MAX + 1);

            int code = dlinfo(handle.get(), RTLD_DI_ORIGIN, result.data());
            if (code) detail::throw_dl_error();
            result.resize(std::strlen(result.data()));
            result.shrink_to_fit();

            return result;
        }

        serinfo info_serinfo() const
        {
            serinfo result;

            Dl_serinfo serlength;
            int code = dlinfo(handle.get(), RTLD_DI_SERINFOSIZE, &serlength);
            if (code) detail::throw_dl_error();

            result.serinfo_struct = std::make_unique<std::byte[]>(serlength.dls_size);
            code = dlinfo(handle.get(), RTLD_DI_SERINFOSIZE, result.serinfo_struct.get());
            if (code) detail::throw_dl_error();
            code = dlinfo(handle.get(), RTLD_DI_SERINFO, result.serinfo_struct.get());
            if (code) detail::throw_dl_error();

            return result;
        }

        size_t info_tls_modid() const
        {
            std::size_t result = -1;
            int code = dlinfo(handle.get(), RTLD_DI_TLS_MODID, &result);
            if (code) detail::throw_dl_error();
            return result;
        }

        void* info_tls_data() const
        {
            void* result = nullptr;
            int code = dlinfo(handle.get(), RTLD_DI_TLS_DATA, &result);
            if (code) detail::throw_dl_error();
            return result;
        }
#endif
    };
}
