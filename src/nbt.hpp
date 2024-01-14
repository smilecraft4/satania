#pragma once

#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include "zlib.h"

namespace nbt
{
	enum TagType
	{
		TAG_End = 0x00,
		TAG_Byte = 0x01,
		TAG_Short = 0x02,
		TAG_Int = 0x03,
		TAG_Long = 0x04,
		TAG_Float = 0x05,
		TAG_Double = 0x06,
		TAG_Byte_Array = 0x07,
		TAG_String = 0x08,
		TAG_List = 0x09,
		TAG_Compound = 0x0A,
		TAG_Int_Array = 0x0B,
		TAG_Long_Array = 0x0C,
	};

	using bytes = std::vector<std::uint8_t>;

	template <typename T>
	size_t _addValue(bytes& dst, T val);

	template <typename T, typename U>
	size_t _addArray(bytes& dst, T* arr, U elem);

	uint8_t colorToMinecraftBlock(float r, float g, float b, float a)
	{
		if (a > 0.5)
		{
			return 1;
		}

		return 0;
	}

	/*
	void nbt::writeBytes(const std::string& name, const bytes& nbt)
	{
		bytes schem;
		// glm::ivec3 size = mesh.max + 1 - mesh.min;

		std::vector<uint8_t> blocks(size.x * size.y * size.z, 0);
		std::vector<uint8_t> data(size.x * size.y * size.z, 0);

		for (size_t i = 0; i < voxels.size(); i++)
		{
			blocks[i] = colorToMinecraftBlock(voxels[i].color);
		}

		addCompoundTag(schem, "Schematic");
		addShortTag(schem, "Width", size.x);
		addShortTag(schem, "Height", size.y);
		addShortTag(schem, "Length", size.z);
		addStringTag(schem, "Materials", "Alpha");
		addByteArrayTag(schem, "Blocks", blocks);
		addByteArrayTag(schem, "Data", data);
		addListTag(schem, "Entities", TAG_Compound, 0);
		addListTag(schem, "TileEntities", TAG_Compound, 0);
		addEndTag(schem);

		gzFile file = gzopen(name.c_str(), "wb");
		gzwrite(file, schem.data(), schem.size());
		gzclose(file);
	}
	*/

	void writeBytes(const std::string& name, const bytes& nbt)
	{
		gzFile file = gzopen(name.c_str(), "wb");
		gzwrite(file, nbt.data(), nbt.size());
		gzclose(file);
	}

	/*************************************

		 Adding Tags to bytes vector

	*************************************/

	template <typename T> void swap_endian(T& val);
	void swap_endian(int64_t& val);
	void swap_endian(int32_t& val);
	void swap_endian(int16_t& val);
	void swap_endian(uint8_t& val) { }
	void swap_endian(int8_t& val) { }

	template <typename T, typename U>
	size_t _addArrayTag(bytes& dst, uint8_t tag, const std::string& name, T* arr, U elem)
	{
		uint16_t nameLen = (uint16_t)name.size();
		char* nameString = (char*)name.c_str();
		size_t bufferSize = sizeof(tag) + sizeof(nameLen) + nameLen + sizeof(elem) + (elem * sizeof(*arr));

		size_t dst_offset = dst.size();

		dst.resize(dst.size() + bufferSize);

		// bytes buf(bufferSize, 0);

		uint8_t* pBytes = dst.data() + dst_offset;
		pBytes = (uint8_t*)memcpy(pBytes, &tag, sizeof(tag));

		uint16_t len = nameLen;
		swap_endian(len);
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(tag), &len, sizeof(nameLen));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(nameLen), nameString, nameLen);

		// TODO: use _addArray instead of repeating this code

		if (sizeof(arr[0]) > sizeof(uint8_t))
		{
			for (size_t i = 0; i < elem; i++) {
				swap_endian(arr[i]);
			}
		}

		auto elemSwapped = elem;
		swap_endian(elemSwapped);

		pBytes = (uint8_t*)memcpy(pBytes + nameLen, (uint8_t*)&elemSwapped, sizeof(elem));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(U), (uint8_t*)arr, (elem * sizeof(*arr)));

		// dst.insert(std::end(dst), std::begin(buf), std::end(buf));

		return (pBytes + (elem * sizeof(*arr))) - (dst.data() + dst_offset);
	};

	// TODO: use concepts for this
	template <typename T>
	size_t _addValueTag(bytes& dst, uint8_t tag, const std::string& name, T val)
	{
		uint16_t nameLen = (uint16_t)name.size();
		char* nameString = (char*)name.c_str();
		size_t bufferSize = sizeof(tag) + sizeof(nameLen) + nameLen + sizeof(val);

		bytes buf(bufferSize, 0);
		uint8_t* pBytes = buf.data();
		pBytes = (uint8_t*)memcpy(pBytes, &tag, sizeof(tag));

		uint16_t len = nameLen;
		swap_endian(len);
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(tag), &len, sizeof(nameLen));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(nameLen), nameString, nameLen);

		swap_endian(val);
		pBytes = (uint8_t*)memcpy(pBytes + nameLen, (uint8_t*)&val, sizeof(val));

		dst.insert(std::end(dst), std::begin(buf), std::end(buf));

		return (pBytes + sizeof(val)) - buf.data();
	}

	template <typename T>
	size_t _addValue(bytes& dst, T val)
	{
		swap_endian(val);
		dst.insert(std::end(dst), (uint8_t*)&val, (uint8_t*)&val + sizeof(T));
		return sizeof(T);
	}

	template <typename T, typename U>
	size_t _addArray(bytes& dst, T* arr, U elem)
	{
		size_t bufferSize = sizeof(elem) + (elem * sizeof(*arr));
		bytes buf(bufferSize, 0);

		if (sizeof(arr[0]) > sizeof(uint8_t))
		{
			for (size_t i = 0; i < elem; i++) {
				swap_endian(arr[i]);
			}
		}

		auto elemSwapped = elem;
		swap_endian(elemSwapped);

		uint8_t* pBytes = buf.data();
		pBytes = (uint8_t*)memcpy(pBytes, (uint8_t*)&elemSwapped, sizeof(elem));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(U), (uint8_t*)arr, (elem * sizeof(*arr)));

		dst.insert(std::end(dst), std::begin(buf), std::end(buf));

		return (pBytes + (elem * sizeof(*arr))) - buf.data();
	}

	size_t addCompoundTag(bytes& dst, const std::string& name)
	{
		uint16_t tag = TAG_Compound;
		uint8_t nameLen = (uint8_t)name.size();
		char* nameString = (char*)name.c_str();

		size_t bufferSize = sizeof(tag) + sizeof(nameLen) + nameLen;
		bytes buf(bufferSize, 0);

		uint8_t* pBytes = buf.data();
		pBytes = (uint8_t*)memcpy(pBytes, &tag, sizeof(tag));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(tag), &nameLen, sizeof(nameLen));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(nameLen), nameString, nameLen);

		dst.insert(std::end(dst), std::begin(buf), std::end(buf));

		return (pBytes + nameLen) - buf.data();
	}

	size_t addListTag(bytes& dst, const std::string& name, TagType type, uint32_t num)
	{
		uint16_t tag = TAG_List;
		uint8_t nameLen = (uint8_t)name.size();
		char* nameString = (char*)name.c_str();
		uint8_t id = (uint8_t)type;

		size_t bufferSize = sizeof(tag) + sizeof(nameLen) + nameLen + sizeof(id) + sizeof(num);
		bytes buf(bufferSize, 0);

		uint8_t* pBytes = buf.data();
		pBytes = (uint8_t*)memcpy(pBytes, &tag, sizeof(tag));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(tag), &nameLen, sizeof(nameLen));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(nameLen), nameString, nameLen);

		auto sw_num = num;
		swap_endian(sw_num);
		pBytes = (uint8_t*)memcpy(pBytes + nameLen, (uint8_t*)&id, sizeof(id));
		pBytes = (uint8_t*)memcpy(pBytes + sizeof(id), (uint8_t*)&sw_num, sizeof(num));

		dst.insert(std::end(dst), std::begin(buf), std::end(buf));

		return (pBytes + sizeof(num)) - buf.data();
		return 0;
	}

	size_t addEndTag(bytes& dst)
	{
		dst.push_back(TAG_End);
		return 1;
	}

	size_t addByteTag(bytes& dst, const std::string& name, uint8_t val)
	{
		if (name.size() == 0)
		{
			return _addValue(dst, val);
		}
		return _addValueTag(dst, TAG_Byte, name, val);
	}

	size_t addShortTag(bytes& dst, const std::string& name, int16_t val)
	{
		if (name.size() == 0)
		{
			return _addValue(dst, val);
		}
		return _addValueTag(dst, TAG_Short, name, val);
	}

	size_t addIntTag(bytes& dst, const std::string& name, int32_t val)
	{
		if (name.size() == 0)
		{
			return _addValue(dst, val);
		}
		return _addValueTag(dst, TAG_Int, name, val);
	}

	size_t addLongTag(bytes& dst, const std::string& name, int64_t val)
	{
		if (name.size() == 0)
		{
			return _addValue(dst, val);
		}
		return _addValueTag(dst, TAG_Long, name, val);
	}

	size_t addFloatTag(bytes& dst, const std::string& name, float val)
	{
		if (name.size() == 0)
		{
			return _addValue(dst, val);
		}
		return _addValueTag(dst, TAG_Float, name, val);
	}

	size_t addDoubleTag(bytes& dst, const std::string& name, double val)
	{
		if (name.size() == 0)
		{
			return _addValue(dst, val);
		}
		return _addValueTag(dst, TAG_Double, name, val);
	}

	size_t addStringTag(bytes& dst, const std::string& name, const std::string& val)
	{
		char* arr = (char*)val.data();
		uint16_t elem = (uint16_t)val.size();

		if (name.size() == 0)
		{
			return _addArray(dst, arr, elem);
		}

		return _addArrayTag(dst, TAG_String, name, arr, elem);
	}

	size_t addByteArrayTag(bytes& dst, const std::string& name, const std::vector<uint8_t>& val)
	{
		uint8_t* arr = (uint8_t*)val.data();
		uint32_t elem = (uint32_t)val.size();

		if (name.size() == 0)
		{
			return _addArray(dst, arr, elem);
		}
		return _addArrayTag(dst, TAG_Byte_Array, name, arr, elem);
	}

	size_t addIntArrayTag(bytes& dst, const std::string& name, const std::vector<int32_t>& val)
	{
		int32_t* arr = (int32_t*)val.data();
		uint32_t elem = (uint32_t)val.size();

		if (name.size() == 0)
		{
			return _addArray(dst, arr, elem);
		}
		return _addArrayTag(dst, TAG_Int_Array, name, arr, elem);
	}

	size_t addLongArrayTag(bytes& dst, const std::string& name, const std::vector<int64_t>& val)
	{
		int64_t* arr = (int64_t*)val.data();
		uint32_t elem = (uint32_t)val.size();

		if (name.size() == 0)
		{
			return _addArray(dst, arr, elem);
		}
		return _addArrayTag(dst, TAG_Long_Array, name, arr, elem);
	}

	template <typename T> void swap_endian(T& val)
	{
		union U {
			T val;
			std::array<std::uint8_t, sizeof(T)> raw;
		} src, dst;

		src.val = val;
		std::reverse_copy(src.raw.begin(), src.raw.end(), dst.raw.begin());
		val = dst.val;
	}

	void swap_endian(int64_t& val)
	{
		val = _byteswap_uint64(val);
	}
	void swap_endian(int32_t& val)
	{
		val = _byteswap_ulong(val);
	}
	void swap_endian(int16_t& val)
	{
		val = _byteswap_ushort(val);
	}
}
