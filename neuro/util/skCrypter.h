#pragma once
#include <array>
#include <string_view>
#include <cstddef>

#ifndef _MSC_VER
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#endif

namespace skc
{
    template<class _Ty, size_t _Size>
    class crypter
    {
        static constexpr _Ty seed = static_cast<_Ty>(
            (__TIME__[0] * 7) ^ (__TIME__[1] * 13) ^
            (__TIME__[3] * 23) ^ (__TIME__[4] * 37) ^
            (__TIME__[6] * 53) ^ (__TIME__[7] * 67)
        );

        std::array<_Ty, _Size> _storage;
        bool _decrypted = false;

        static constexpr _Ty keyAt(size_t idx) noexcept
        {
            return static_cast<_Ty>(seed ^ static_cast<_Ty>(idx * 131 + 17));
        }

        static constexpr _Ty encrypt(_Ty c, size_t idx) noexcept
        {
            return c ^ keyAt(idx);
        }

        static constexpr _Ty decrypt(_Ty c, size_t idx) noexcept
        {
            return c ^ keyAt(idx);
        }

    public:
        __forceinline constexpr crypter(const _Ty* data) noexcept
        {
            for (size_t i = 0; i < _Size; ++i)
            {
                _storage[i] = encrypt(data[i], i);
            }
        }

        __forceinline const _Ty* get() const noexcept
        {
            return _storage.data();
        }

        __forceinline _Ty* get_decrypted() noexcept
        {
            if (!_decrypted) {
                for (size_t i = 0; i < _Size; ++i)
                {
                    _storage[i] = decrypt(_storage[i], i);
                }
                _decrypted = true;
            }
            return _storage.data();
        }
    };

    template<class _Ty, size_t _Size>
    __forceinline constexpr auto make_crypter(const _Ty(&data)[_Size]) noexcept
    {
        return crypter<_Ty, _Size>(data);
    }
}

#define skCrypt(str) []() { \
    constexpr auto crypt = skc::make_crypter(str); \
    static auto decrypted = crypt; \
    return decrypted.get_decrypted(); \
}()
