#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <assert.h>
#include <thread>
#include <random>

#include "nbt.hpp"
#include "zlib.h"
#include "timer.hpp"

namespace mca
{
	constexpr size_t entries = 1024;

	void writeMCA(const std::string& filename, int x, int y, const std::vector<std::string>& palette, const std::vector<uint64_t>& data);
	void writeChunkData(std::vector<uint8_t>& buf, int mca_x, int mca_y, int index, const std::vector<std::string>& palette, const std::vector<uint64_t>& data);
	void writeChunk(nbt::bytes& chunk, int mca_x, int mca_y, int x, int z, const std::vector<std::string>& palette, const std::vector<uint64_t>& data);
	void compressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data);


	void writeMCA(const std::string& filename, int x, int y, const std::vector<std::string>& palette, const std::vector<uint64_t>& data)
	{
		// Defines constants
		constexpr size_t max_entries_count = 1024; // 32 x 32  chunks
		constexpr size_t max_entries_size = max_entries_count * 4096;
		constexpr size_t max_locations_size = max_entries_count * 4;
		constexpr size_t max_timestamps_size = max_entries_count * 4;
		constexpr size_t max_buffer_size = max_locations_size + max_timestamps_size + max_entries_size;

		// Pre-allocate buffer to be written to file
		std::vector<uint8_t> buffer(max_buffer_size, 0);

#if SATANIA_MULTITHREADING
		
		int num_thread = std::thread::hardware_concurrency();
		int count = max_entries_count / num_thread;
		std::vector<std::thread> threads;

		auto thread_function = [&](std::vector<uint8_t>& buffer, int start, int count, const std::vector<std::string>& palette, const std::vector<uint64_t>& data) {
			for (int i = start; i < start + count; i++) {
				writeChunkData(buffer, x, y, i, palette, data);
			}
		};

		for (size_t t = 0; t < num_thread; t++)
		{
			int start = t * count;
			threads.push_back(std::thread(thread_function, std::ref(buffer), start, count, palette, data));
		}

		// Wait for all the tread to be finished
		for (auto& thread : threads) {
			thread.join();
		}
#else

		for (int i = 0; i < max_entries_count; i++) {
			writeChunkData(buffer, x, y, i, palette, data);
		}

#endif
		// Save the buffer to a file;
		FILE* mca_file;
		errno_t err = fopen_s(&mca_file, filename.c_str(), "wb");
		if (err) {
			fprintf(stderr, "cannot create/overwrite .mca file");
		}
		else
		{
			size_t size = fwrite(buffer.data(), sizeof(buffer[0]), buffer.size(), mca_file);

			if (size != buffer.size() * sizeof(buffer[0])) {
				fprintf(stderr, "The .mca buffer was not entirely written");
			}

			fclose(mca_file);
		}

	}

	void writeChunkData(std::vector<uint8_t>& buf, int mca_x, int mca_y, int index, const std::vector<std::string>& palette, const std::vector<uint64_t>& data)
	{
		// Constants
		constexpr int index_stride = 4;
		constexpr int timestamp_offset = 4096;
		constexpr int chunk_location_offset = 4096;
		constexpr size_t chunk_reserve_size = 65536;

		// set chunk data location and timestamp in mca header file
		uint32_t location = _byteswap_ulong(((index + 2) << 8) + 1);
		uint32_t timestamp = _byteswap_ulong(time(NULL));

		auto ptr = buf.data();

		// copy the data to the buffer
		memcpy(ptr + (index * index_stride), &location, sizeof(location));
		memcpy(ptr + (index * index_stride) + timestamp_offset, &timestamp, sizeof(timestamp));

		std::vector<uint8_t> chunk, chunk_compressed;
		chunk.reserve(chunk_reserve_size);

		int x = index % 32;
		int z = index / 32;

		writeChunk(chunk, mca_x, mca_y, x, z, palette, data);

		compressMemory(chunk.data(), chunk.size() * sizeof(chunk[0]), chunk_compressed);

		uint32_t length = _byteswap_ulong((uint32_t)chunk_compressed.size() + 1);
		uint8_t compression = 2;

		memcpy(ptr + ((index + 2) * 4096), &length, sizeof(length));
		memcpy(ptr + ((index + 2) * 4096) + 4, &compression, sizeof(compression));
		memcpy(ptr + ((index + 2) * 4096) + 5, chunk_compressed.data(), chunk_compressed.size() * sizeof(chunk_compressed[0]));

		//printf("Finished chunk (%i, %i)\n", x, z);
	}

	void writeChunk(nbt::bytes& chunk, int mca_x, int mca_y, int x, int z, const std::vector<std::string>& palette, const std::vector<uint64_t>& data)
	{
		int data_version = 2975; // 1.18.2
		int chunk_height = data.size() / (512 * 32);
		int section_count = chunk_height / 16;
		int min_height = -4;
		std::vector<int64_t> data_long(256, 0);

		nbt::addCompoundTag(chunk, "");
		nbt::addIntTag(chunk, "DataVersion", data_version);
		nbt::addIntTag(chunk, "xPos", mca_x + x);
		nbt::addIntTag(chunk, "zPos", mca_y + z);
		nbt::addIntTag(chunk, "yPos", min_height);
		nbt::addStringTag(chunk, "Status", "full");
		nbt::addLongTag(chunk, "LastUpdate", 0);

		nbt::addListTag(chunk, "sections", nbt::TAG_Compound, section_count);
		for (int y = 0; y < section_count; y++) {
			nbt::addByteTag(chunk, "Y", (y + min_height));
			nbt::addCompoundTag(chunk, "biomes");
			nbt::addListTag(chunk, "palette", nbt::TAG_String, 1);
			nbt::addStringTag(chunk, "", "minecraft:the_void");
			nbt::addEndTag(chunk);
			nbt::addCompoundTag(chunk, "block_states");

			// TODO: remove unused pallete element and remove data long array if it is a unique chunk
			

			bool edited = false;
			for (int local_y = 0; local_y < 16; local_y++)
			{
				for (int local_z = 0; local_z < 16; local_z++)
				{
					int chunk_y = y * 16 + local_y;
					int chunk_z = z * 16 + local_z;

					int data_index = (chunk_z * chunk_height + chunk_y) * 32 + x;
					int data_long_index = (local_y * 16) + local_z;

					data_long[data_long_index] = data[data_index];

					if (data_long[data_long_index] != 0 && !edited)
					{
						// printf("CHUNK (%i, %i) section %i, modified\n", x, z, y);
						edited = true;
					}
				}
			}

			if (edited)
			{
				nbt::addListTag(chunk, "palette", nbt::TAG_Compound, palette.size());
				for (const auto& block_id : palette) {

					nbt::addStringTag(chunk, "Name", block_id);
					nbt::addEndTag(chunk);
				}

				nbt::addLongArrayTag(chunk, "data", data_long);
			}
			else
			{
				nbt::addListTag(chunk, "palette", nbt::TAG_Compound, 1);
				nbt::addStringTag(chunk, "Name", palette[0]);
				nbt::addEndTag(chunk);
			}

			

			nbt::addEndTag(chunk);
			nbt::addEndTag(chunk);
		}

		nbt::addListTag(chunk, "block_entities", nbt::TAG_Compound, 0);
		nbt::addCompoundTag(chunk, "Heightmaps");
		nbt::addEndTag(chunk);
		nbt::addListTag(chunk, "fluid_ticks", nbt::TAG_Compound, 0);
		nbt::addListTag(chunk, "block_ticks", nbt::TAG_Compound, 0);
		nbt::addListTag(chunk, "entities", nbt::TAG_Compound, 0);
		nbt::addLongTag(chunk, "InhabitedTime", 0);

		nbt::addListTag(chunk, "Lights", nbt::TAG_List, section_count);
		for (int i = 0; i < section_count * 5; i++) {
			nbt::addEndTag(chunk);
		}

		nbt::addListTag(chunk, "PostProcessing", nbt::TAG_List, section_count);
		for (int i = 0; i < section_count * 5; i++) {
			nbt::addEndTag(chunk);
		}

		nbt::addCompoundTag(chunk, "CarvingMasks");
		nbt::addEndTag(chunk);
		nbt::addCompoundTag(chunk, "structures");
		nbt::addCompoundTag(chunk, "References");
		nbt::addEndTag(chunk);
		nbt::addCompoundTag(chunk, "starts");
		nbt::addEndTag(chunk);
		nbt::addEndTag(chunk);


		nbt::addEndTag(chunk);
	}

	void compressMemory(void* in_data, size_t in_data_size, std::vector<uint8_t>& out_data)
	{
		std::vector<uint8_t> buffer;

		const size_t BUFSIZE = 128 * 1024;
		uint8_t temp_buffer[BUFSIZE]{};

		z_stream strm{};
		strm.zalloc = 0;
		strm.zfree = 0;
		strm.next_in = reinterpret_cast<uint8_t*>(in_data);
		strm.avail_in = in_data_size;
		strm.next_out = temp_buffer;
		strm.avail_out = BUFSIZE;

		deflateInit(&strm, Z_BEST_COMPRESSION);

		while (strm.avail_in != 0)
		{
			int res = deflate(&strm, Z_NO_FLUSH);
			assert(res == Z_OK);
			if (strm.avail_out == 0)
			{
				buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
				strm.next_out = temp_buffer;
				strm.avail_out = BUFSIZE;
			}
		}

		int deflate_res = Z_OK;
		while (deflate_res == Z_OK)
		{
			if (strm.avail_out == 0)
			{
				buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
				strm.next_out = temp_buffer;
				strm.avail_out = BUFSIZE;
			}
			deflate_res = deflate(&strm, Z_FINISH);
		}

		assert(deflate_res == Z_STREAM_END);
		buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE - strm.avail_out);
		deflateEnd(&strm);

		out_data.swap(buffer);
	}

}