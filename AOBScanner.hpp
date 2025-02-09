#pragma once

#include <Windows.h>
#include <vector>
#include <iostream>

#pragma warning(disable:6386)
#pragma warning(disable:26812)

class AOBScanner {
private:
	static std::vector<BYTE> ParsePattern(const char* pattern, std::string* mask_buf) {
		if (!mask_buf) return {};
		if (!pattern) return {};
		if (!pattern[0] || pattern[0] == ' ') return {};

		unsigned __int16 pattern_len = static_cast<unsigned __int16>(strlen(pattern) + 1);
		char* pattern_copy = new char[pattern_len];
		strcpy_s(pattern_copy, pattern_len, pattern);

		unsigned __int8 sequence = 0;
		std::vector<BYTE> pattern_bytes;

		for (unsigned __int16 i = 0; i < pattern_len; i++) {
			if ((pattern[i] >= '0' && '9' >= pattern[i]) || (pattern[i] >= 'A' && 'F' >= pattern[i])) {
				if (pattern[i - 1] == '?' || '?' == pattern[i + 1]) continue;
				sequence++;
			}
			else if ((pattern[i] == ' ' || pattern[i] == '\0') && sequence == 2) {
				sequence = 0;
				pattern_copy[i] = '\0';
				pattern_bytes.push_back(static_cast<BYTE>(strtol(pattern_copy + (i - 2), nullptr, 16)));
				*mask_buf += "x";
			}
			else if (pattern[i] == '?') {
				if (pattern[i + 1] == ' ' || pattern[i + 1] == '\0') {
					if (pattern[i - 1] == ' ' || pattern[i - 1] == '?') {
						pattern_bytes.push_back(0x00);
						*mask_buf += "?";
					}
					else {
						pattern_copy[i] = '0';
						if (pattern[i + 1] != '\0') pattern_copy[i + 1] = '\0';
						pattern_bytes.push_back(static_cast<BYTE>(strtol(pattern_copy + (i - 1), nullptr, 16)));
						*mask_buf += "1";
					}
				}
				else {
					pattern_copy[i] = '0';
					if (pattern[i + 2] != '\0') pattern_copy[i + 2] = '\0';
					pattern_bytes.push_back(static_cast<BYTE>(strtol(pattern_copy + i, nullptr, 16)));
					*mask_buf += "2";
				}
			}
		}

		delete[] pattern_copy;
		return pattern_bytes;
	}

	static char SplitByte(BYTE b, char mask) {
		static const char characters[] = "0123456789ABCDEF";
		char c_byte[2] = { 0, 0 };

		c_byte[0] = characters[b >> 4];
		c_byte[1] = characters[b & 0x0F];

		return mask == '1' ? c_byte[0] : c_byte[1];
	}

	static bool CompareBytes(BYTE* data, BYTE* pattern, const char* mask) {
		for (; *mask; data++, pattern++, mask++) {
			if (*mask == 'x' && *data != *pattern) return false;
			else if ((*mask == '1' || '2' == *mask) && (SplitByte(*data, *mask) != SplitByte(*pattern, *mask))) return false;
		}

		return true;
	}

public:
	struct RegionAttributes {
		DWORD AllocationProtect = NULL;
		DWORD State = NULL;
		DWORD Protect = NULL;
		DWORD Type = NULL;

		RegionAttributes(DWORD AllocationProtect = NULL, DWORD State = NULL, DWORD Protect = NULL, DWORD Type = NULL) :
		AllocationProtect(AllocationProtect), State(State), Protect(Protect), Type(Type) {}
	};

	template<class _Ty>
	static DWORD Scan(HANDLE process, const char* pattern, std::vector<_Ty>& values, RegionAttributes&& ra = RegionAttributes(), BYTE* from = nullptr, BYTE* to = nullptr) {
		if (!process) return NULL;
		if (!pattern) return NULL;
		if (!pattern[0] || pattern[0] == ' ') return NULL;

		std::string mask_buf;
		std::vector<BYTE> pattern_bytes = ParsePattern(pattern, &mask_buf);

		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);

		if (!from) from = reinterpret_cast<BYTE*>(sys_info.lpMinimumApplicationAddress);
		if (!to) to = reinterpret_cast<BYTE*>(sys_info.lpMaximumApplicationAddress);

		MEMORY_BASIC_INFORMATION mbi;
		uint64_t offset = NULL;

		while ((from + offset) < to) {
			if (VirtualQueryEx(process, from + offset, &mbi, sizeof(mbi))) {
				if (!ra.AllocationProtect ? (mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS) :
				((mbi.AllocationProtect == ra.AllocationProtect && mbi.State == ra.State) && (mbi.Protect == ra.Protect && mbi.Type == ra.Type))) {

					BYTE* bytes_buffer = new BYTE[mbi.RegionSize];
					ReadProcessMemory(process, mbi.BaseAddress, bytes_buffer, mbi.RegionSize, nullptr);

					for (uint64_t i = 0; bytes_buffer + i != bytes_buffer + (mbi.RegionSize - 1 - pattern_bytes.size()); i++)
						if (CompareBytes(bytes_buffer + i, &pattern_bytes[0], mask_buf.c_str()))
							values.push_back(reinterpret_cast<_Ty>(reinterpret_cast<uintptr_t>(mbi.BaseAddress) + i));

					delete[] bytes_buffer;
				}
			}

			offset += mbi.RegionSize;
		}

		return TRUE;
	}
};