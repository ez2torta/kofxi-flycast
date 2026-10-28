/*
 * awcartridge.cpp
 *
 *  Created on: Nov 5, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/awboard.cpp)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert,Andreas Naive
 *
 */

/*

Atomiswave ROM board specs from Cah4e3 @ http://cah4e3.wordpress.com/2009/07/26/some-atomiswave-info/

 AW_EPR_OFFSETL                                          Register addres: 0x5f7000
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                         EPR data offset low word                              |
 +-------------------------------------------------------------------------------+

 AW_EPR_OFFSETH                                          Register addres: 0x5f7004
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                          EPR data offset hi word                              |
 +-------------------------------------------------------------------------------+

  Both low and high words of 32-bit offset from start of EPR-ROM area. Used for
  reading header and program code data, cannot be used for reading MPR-ROMs data.
  During program code DMA transfer Romeo MCU perform data checksuming (decrypted data, in 8bit units),
  result must match some CPLD / encryption key-specific value, otherwise MPR-ROM access
  described below will not work correctly.
  Game header (first 256 bytes of ROM) is not checksum protected.

 AW_MPR_RECORD_INDEX                                     Register addres: 0x5f700c
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                          File system record index                             |
 +-------------------------------------------------------------------------------+

  This register contains index of MPR-ROM file system record (64-bytes in size) to
  read throught DMA. Internal DMA offset register is assigned as AW_MPR_RECORD_INDEX<<6
  from start of MPR-ROM area. Size of DMA transaction not limited, it is possible
  to read any number of records or just part of it.

 AW_MPR_FIRST_FILE_INDEX                                 Register addres: 0x5f7010
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                           First file record index                             |
 +-------------------------------------------------------------------------------+

  This register assign for internal cart circuit index of record in MPR-ROM file
  system sub-area that contain information about first file of MPR-ROM files
  sub-area. Internal circuit using this record to read absolute first file offset
  from start of MPR-ROM area and calculate normal offset for each other file
  requested, since MPR-ROM file data sub-area can be assighed only with relative
  offsets from start of such sub-area.

 AW_MPR_FILE_OFFSETL                                     Register addres: 0x5f7014
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                         MPR file offset low word                              |
 +-------------------------------------------------------------------------------+

 AW_MPR_FILE_OFFSETH                                     Register addres: 0x5f7018
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                          MPR file offset hi word                              |
 +-------------------------------------------------------------------------------+

  Both low and high words of 32-bit relative offset from start of MPR-ROM files
  sub-area. Used by internal circuit to calculate absolute offset using data
  from AW_MPR_FIRST_FILE_INDEX register. Cannot be used for reading EPR-ROM
  data nor even MPR-ROM file system sub-area data.

 AW_PIO_DATA                                             Register addres: 0x5f7080
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                Read/Write word from/to ROM board address space                |
 +-------------------------------------------------------------------------------+

  Using this register data can be read or written to ROM BD at AW_EPR_OFFSET directly,
  decryption is not used, flash ROMs (re)programming via CFI commands possible.

  Type 2 ROM BD have MPR_BANK register at AW_EPR_OFFSET 007fffff, which selects
  1 of 4 mask ROM banks.

ROM board internal layouts:

 Type 1:

 00000000 - 00800000 IC18 flash ROM
 00800000 - 01000000 IC10 or mirror of above
 01000000 - 02000000 IC11 \
        .....               mask ROMs
 07000000 - 08000000 IC17 /

 Type 2:

 00000000 - 00800000 FMEM1 flash ROM
 00800000 - 01000000 mirror of above
 01000000 - 01800000 FMEM2 flash ROM
 01800000 - 02000000 mirror of above
 02000000 - 04000000 MROM1 MROM4 MROM7 MROM10 \
 04000000 - 06000000 MROM2 MROM5 MROM8 MROM11   banked mask ROMs
 06000000 - 08000000 MROM3 MROM6 MROM9 MROM12 /

 Type 3:

 00000000 - 01000000 U3  flash ROM
 01000000 - 02000000 U1  flash ROM
 02000000 - 03000000 U4  flash ROM
 03000000 - 04000000 U2  flash ROM
 04000000 - 05000000 U15 flash ROM
 05000000 - 06000000 U17 flash ROM
 06000000 - 07000000 U14 flash ROM
 07000000 - 08000000 U16 flash ROM

 Development:

 00000000 - 00800000 IC12 \
        .....               flash ROMs
 07800000 - 08000000 IC27 /


 In short:

     EPR-ROM
 +--------------+ 0x00000000
 |              |
 |    HEADER    +- AW_EPR_OFFSET << 1
 |              |
 +--------------+
 |              |
 |     CODE     +- AW_EPR_OFFSET << 1
 |              |
 |              |
 +--------------+ 0x007fffff

     MPR-ROMS
 +--------------+ 0x00000000
 | FS_HEADER    |
 | FS_RECORD[1] +- (AW_MPR_RECORD_INDEX << 6)
 | FS_RECORD[2] |
 | FS_RECORD[3] +- (AW_MPR_FIRST_FILE_INDEX << 6)
 |     ...      |
 | FS_RECORD[N] |
 +--------------+- FS_RECORD[AW_MPR_FIRST_FILE_INDEX].FILE_ABS_OFFSET
 | FILE_0       |
 | FILE_1       +- (AW_MPR_FILE_OFFSET << 1) + FS_RECORD[AW_MPR_FIRST_FILE_INDEX].FILE_ABS_OFFSET
 |     ...      |
 | FILE_N       |
 +--------------+ 0x07ffffff

*/
#include "awcartridge.h"
#include "awave_regs.h"
#include "serialize.h"

#ifdef AW_CARTRIDGE_PROFILE
#include <chrono>
#include "naomi_roms.h"
#include "oslib/oslib.h"
#include <memory>
#include <algorithm>
#include <sstream>
#include "nowide/stackstring.hpp"
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#endif

enum { EPR, MPR_RECORD, MPR_FILE };


u32 AWCartridge::ReadMem(u32 address, u32 size)
{
	verify(size != 1);
	switch (address)
	{
	case AW_PIO_DATA_addr:
		{
			u32 roffset = epr_offset & 0x3ffffff;
			if (roffset >= (mpr_offset / 2))
				roffset += mpr_bank * 0x4000000;
			u16 retval = 0;
			if (RomSize > roffset * 2)
#ifdef AW_CARTRIDGE_PROFILE
				retval = cipherWord(roffset);
#else
				retval = ((u16 *)RomPtr)[roffset]; // not endian-safe?
#endif
			DEBUG_LOG(NAOMI, "AWCART ReadMem %08x: %x", address, retval);
			return retval;
		}
	default:
		INFO_LOG(NAOMI, "Unhandled awcart read %X, %d", address, size);
		return 0xffff;
	}
}

void AWCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	switch (address)
	{
	case AW_EPR_OFFSETH_addr:
		epr_offset = (epr_offset & 0x0000ffff) | (data << 16);
		recalc_dma_offset(EPR);
		break;

	case AW_EPR_OFFSETL_addr:
		epr_offset = (epr_offset & 0xffff0000) | (u16)data;
		recalc_dma_offset(EPR);
		break;

	case AW_MPR_RECORD_INDEX_addr:
		mpr_record_index = data;
#ifdef AW_CARTRIDGE_PROFILE
		++profile_record_index_writes;
#endif
		recalc_dma_offset(MPR_RECORD);
		break;

	case AW_MPR_FIRST_FILE_INDEX_addr:
		mpr_first_file_index = data;
#ifdef AW_CARTRIDGE_PROFILE
		++profile_first_file_index_writes;
#endif
		recalc_dma_offset(MPR_FILE);
		break;

	case AW_MPR_FILE_OFFSETH_addr:
		mpr_file_offset = (mpr_file_offset & 0x0000ffff) | (data << 16);
		recalc_dma_offset(MPR_FILE);
		break;

	case AW_MPR_FILE_OFFSETL_addr:
		mpr_file_offset = (mpr_file_offset & 0xffff0000) | (u16)data;
#ifdef AW_CARTRIDGE_PROFILE
		++profile_file_offset_writes;
#endif
		recalc_dma_offset(MPR_FILE);
		break;

	case AW_PIO_DATA_addr:
		// write to ROM board address space, including FlashROM programming using CFI (TODO)
		DEBUG_LOG(NAOMI, "Write to AW_PIO_DATA: %x", data);
		if (epr_offset == 0x7fffff)
			mpr_bank = data & 3;
		break;

	default:
		INFO_LOG(NAOMI, "Unhandled awcart write %X: %d sz %d", address, data, size);
		break;
	}
}

/*
We are using 8 bits keys with the following subfields' structure:
bits 0-3 is an index of 16-bits XOR (only 11 was used in known games)
bits 4-5 is an index to the sboxes table
bits 6-7 is an index to the permutation table

These subfields could be differing from the "real" ones in the following ways:
- Current keys equal to decrypted game code binary 8-bit sum (of each byte's swapped 4-bit nibbles)
- Every one of the index subfields could be suffering an arbitrary bitswap and XOR
- The 16-bits-XOR subfield could suffer an arbitrary XOR which could depend on the 4 index bits (that is: a different XOR per every index combination)
- Of course, the way in which we are mixing 3 subfields in one only key is arbitrary too.
*/


const u8 permutation_table[4][16] =
{
	{14,1,11,15,7,3,8,13,0,4,2,12,6,10,5,9},
	{8,10,1,3,7,4,11,2,5,15,6,0,12,13,9,14},
	{4,5,9,6,1,13,7,11,10,0,14,12,8,15,2,3},
	{12,7,11,2,0,5,15,6,1,8,14,4,9,13,3,10}
};

struct sbox_set {
	u8 S0[32];
	u8 S1[16];
	u8 S2[16];
	u8 S3[8];
};

const sbox_set sboxes_table[4] =
{
	{
		{11,8,6,25,2,7,23,28,5,10,21,20,1,26,17,19,14,27,22,30,15,4,9,24,31,3,16,12,0,18,29,13},
		{13,5,9,6,4,2,11,10,12,0,8,1,3,14,15,7},
		{1,13,11,3,8,7,9,10,12,15,4,14,0,5,6,2},
		{3,0,5,6,2,4,1,7}
	},
	{
		{9,15,28,7,13,24,2,23,21,1,22,16,18,8,17,31,27,6,30,12,4,20,5,19,0,25,3,29,10,14,11,26},
		{5,2,13,11,8,6,12,1,4,3,0,10,14,15,7,9},
		{11,6,10,0,12,1,8,14,2,9,13,3,7,4,15,5},
		{1,5,6,2,4,7,3,0}
	},
	{
		{17,25,29,27,5,11,10,21,2,8,13,0,30,3,14,16,22,1,7,15,31,18,4,20,9,19,26,24,23,28,12,6},
		{15,3,2,11,7,14,6,12,1,13,0,8,9,4,10,5},
		{6,12,5,7,4,8,2,3,1,14,13,11,9,10,0,15},
		{5,1,0,3,4,6,2,7}
	},
	{
		{7,21,9,20,10,28,31,11,16,15,14,30,27,23,5,25,0,22,24,2,6,17,3,18,4,12,13,19,26,1,29,8},
		{10,6,5,1,13,0,9,2,15,14,7,11,8,3,12,4},
		{8,6,15,13,2,7,1,3,11,0,14,10,4,5,12,9},
		{6,5,4,1,0,2,3,7}
	}
};

const int xor_table[16] =  // -1 = unknown/unused
{
	0x0000, -1, 0x97CF, 0x4BE3, 0x2255, 0x8DD6, -1, 0xC6A2,	0xA1E8, 0xB3BF, 0x3B1A, 0x547A, -1, 0x935F, -1, -1
};

static u16 bitswap16(u16 in, const u8* vec)
{
  u16 ret = 0;
  for (int i = 0; i<16; i++) {
    ret |= ((in >> vec[i])&0x1)<<(15-i);
  }
  return ret;
}

static u16 decrypt(u16 cipherText, u32 address, const u8 key)
{
	u8 b0,b1,b2,b3;
	u16 aux;
	const u8* pbox = permutation_table[key>>6];
	const sbox_set* ss = &sboxes_table[(key>>4)&3];

	const u8 text_swap_vec[] = {
			pbox[15],pbox[14],pbox[13],pbox[12],pbox[11],pbox[10],pbox[9],pbox[8],
			pbox[7],pbox[6],pbox[5],pbox[4],pbox[3],pbox[2],pbox[1],pbox[0] };
	aux = bitswap16(cipherText, text_swap_vec);
	const u8 addr_swap_vec[] = { 13,5,2, 14,10,9,4, 15,11,6,1, 12,8,7,3,0 };
	aux = aux ^ bitswap16((u16)address, addr_swap_vec);

	b0 = aux&0x1f;
	b1 = (aux>>5)&0xf;
	b2 = (aux>>9)&0xf;
	b3 = aux>>13;

	b0 = ss->S0[b0];
	b1 = ss->S1[b1];
	b2 = ss->S2[b2];
	b3 = ss->S3[b3];

	return ((b3<<13)|(b2<<9)|(b1<<5)|b0)^xor_table[key&0xf];
}

u16 AWCartridge::decrypt16(u32 address) {
#ifdef AW_CARTRIDGE_PROFILE
	return decrypt(cipherWord(address % (RomSize / 2)), address, rombd_key);
#else
	return decrypt(((u16 *)RomPtr)[address % (RomSize / 2)], address, rombd_key);
#endif
}

#ifdef AW_CARTRIDGE_PROFILE
u16 AWCartridge::cipherWord(u32 address) const
{
	return ((const u16 *)RomPtr)[address];
}

// Inverse of the existing AW transformation, used only to preserve raw PIO when
// a segment stores plaintext. No second encrypted copy is retained at runtime.
static u16 encryptWord(u16 plain, u32 address, u8 key)
{
	const auto& ss = sboxes_table[(key >> 4) & 3];
	u16 aux = plain ^ xor_table[key & 15];
	auto inverse = [](const u8 *table, int count, int value) {
		for (int i = 0; i < count; ++i)
			if (table[i] == value)
				return i;
		return 0; // All table entries form permutations.
	};
	aux = inverse(ss.S0, 32, aux & 31)
			| (inverse(ss.S1, 16, (aux >> 5) & 15) << 5)
			| (inverse(ss.S2, 16, (aux >> 9) & 15) << 9)
			| (inverse(ss.S3, 8, aux >> 13) << 13);
	const u8 addr_vec[] = {13,5,2,14,10,9,4,15,11,6,1,12,8,7,3,0};
	aux ^= bitswap16((u16)address, addr_vec);
	u16 cipher = 0;
	const u8 *pbox = permutation_table[key >> 6];
	for (int i = 0; i < 16; ++i)
		cipher |= ((aux >> i) & 1) << pbox[i];
	return cipher;
}

struct KOFXIExperimentalCartridge::MappedAsset
{
	const u8 *data = nullptr;
#ifdef _WIN32
	HANDLE file = INVALID_HANDLE_VALUE;
	~MappedAsset()
	{
		if (data != nullptr)
			UnmapViewOfFile(data);
		if (file != INVALID_HANDLE_VALUE)
			CloseHandle(file);
	}
	void map()
	{
		HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (mapping == nullptr)
			throw NaomiCartException("Cannot create extracted asset mapping");
		data = static_cast<const u8 *>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
		CloseHandle(mapping);
		if (data == nullptr)
			throw NaomiCartException("Cannot map extracted asset");
	}
#endif
};

const KOFXIExperimentalCartridge::AssetSpan *KOFXIExperimentalCartridge::findAsset(u32 offset) const
{
	auto it = std::upper_bound(assets.begin(), assets.end(), offset,
			[](u32 pos, const AssetSpan& span) { return pos < span.offset; });
	if (it != assets.begin())
	{
		--it;
		if (offset - it->offset < it->size)
			return &*it;
	}
	return nullptr;
}

void KOFXIExperimentalCartridge::profileBackendReport() const
{
	if (!useExtracted)
		return;
	u64 heap = 0;
	for (const auto& segment : segments)
		heap += segment.data.size();
	NOTICE_LOG(NAOMI, "KOF XI extracted profile: mapped_files=%llu mapped_file_bytes=%llu fallback_heap_bytes=%llu mapped_word_reads=%llu fallback_word_reads=%llu",
			(unsigned long long)mappedFiles, (unsigned long long)mappedBytes, (unsigned long long)heap,
			(unsigned long long)mappedWordReads, (unsigned long long)fallbackWordReads);
}

void KOFXIExperimentalCartridge::initExtracted(LoadProgress *progress)
{
#ifndef _WIN32
	throw NaomiCartException("The extracted-file mapping experiment currently requires Windows");
#else
	// G1ROM offsets describe a packed sequence of seven MROMs, not physical
	// cartridge addresses. Resolve every slice through the catalogue, including banks.
	constexpr u32 chipSize = 0x2000000;
	constexpr u32 mprSize = chipSize * 7;
	u32 cachedAddress = ~0u;
	u16 cachedWord = 0;
	auto referenceByte = [&](u32 logical) -> u8 {
		if (logical >= mprSize)
			throw NaomiCartException("G1ROM offset outside KOF XI MPR data");
		const auto& chip = segments[1 + logical / chipSize];
		u32 i = logical % chipSize;
		u32 address = (chip.offset + (i & ~1u)) / 2;
		if (cachedAddress != address)
		{
			u16 raw = chip.data[i & ~1u] | (chip.data[(i & ~1u) + 1] << 8);
			cachedWord = decrypt(raw, address, key);
			cachedAddress = address;
		}
		return (u8)(cachedWord >> ((i & 1) * 8));
	};
	auto reference32 = [&](u32 logical) {
		u32 value = 0;
		for (u32 i = 0; i < 4; ++i)
			value |= (u32)referenceByte(logical + i) << (8 * i);
		return value;
	};
	const char magic[] = "G1ROM FILESYSTEM";
	for (u32 i = 0; i < 16; ++i)
		if (referenceByte(i) != (u8)magic[i])
			throw NaomiCartException("KOF XI G1ROM header mismatch");
	u32 records = reference32(16);
	if (records < 2 || records > 4096)
		throw NaomiCartException("Invalid KOF XI G1ROM record count");
	std::vector<bool> seen(records, false);
	std::string manifestPath = directory + "/_manifest.txt";
	std::unique_ptr<FILE, decltype(&fclose)> manifest(nowide::fopen(manifestPath.c_str(), "rb"), fclose);
	if (!manifest)
		throw NaomiCartException("Cannot open extracted manifest: " + manifestPath);
	char line[2048];
	u32 rows = 0;
	while (fgets(line, sizeof(line), manifest.get()))
	{
		if (progress != nullptr && progress->cancelled)
			throw LoadCancelledException();
		std::string text(line);
		if (text.empty() || text[0] == '#' || text == "\n" || text == "\r\n")
			continue;
		if (text.back() != '\n' && !feof(manifest.get()))
			throw NaomiCartException("Oversized extracted manifest row");
		while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
			text.pop_back();
		std::istringstream stream(text);
		std::string fields[5], extra;
		for (auto& field : fields)
			if (!std::getline(stream, field, '\t'))
				throw NaomiCartException("Malformed extracted manifest row");
		if (std::getline(stream, extra, '\t'))
			throw NaomiCartException("Extra extracted manifest columns");
		auto number = [](const std::string& field, int base) -> u32 {
			if (field.empty() || field[0] == '-' || field[0] == '+')
				throw NaomiCartException("Invalid extracted manifest number");
			try {
				size_t count;
				u64 value = std::stoull(field, &count, base);
				if (count != field.size() || value > 0xffffffffu)
					throw NaomiCartException("Extracted manifest number out of range");
				return (u32)value;
			} catch (const std::invalid_argument&) {
				throw NaomiCartException("Invalid extracted manifest number");
			} catch (const std::out_of_range&) {
				throw NaomiCartException("Extracted manifest number out of range");
			}
		};
		u32 index = number(fields[0], 10), offset = number(fields[2], 16), size = number(fields[3], 10);
		if (index == 0 || index >= records || seen[index] || offset > mprSize || size > mprSize - offset)
			throw NaomiCartException("Invalid/duplicate extracted manifest range");
		seen[index] = true;
		u32 record = index * 64;
		std::string originalName;
		for (u32 i = 0; i < 16; ++i)
		{
			u8 ch = referenceByte(record + 16 + i);
			if (ch == 0)
				break;
			originalName += (char)ch;
		}
		if (referenceByte(record + 7) == 0x40 || fields[1] != originalName
				|| reference32(record + 8) != offset || reference32(record + 12) != size)
			throw NaomiCartException("Extracted manifest differs from G1ROM record");
		const std::string& name = fields[4];
		if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name.find("..") != std::string::npos)
			throw NaomiCartException("Unsafe extracted asset filename");
		std::string path = directory + "/" + name;
		auto asset = std::make_shared<MappedAsset>();
		nowide::wstackstring wide;
		if (!wide.convert(path.c_str()))
			throw NaomiCartException("Invalid extracted asset path encoding");
		asset->file = CreateFileW(wide.get(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		LARGE_INTEGER length{};
		if (asset->file == INVALID_HANDLE_VALUE || !GetFileSizeEx(asset->file, &length) || length.QuadPart != size)
			throw NaomiCartException("Missing/wrong-sized extracted asset: " + path);
		if (size != 0)
		{
			asset->map();
			for (u32 i = 0; i < size; ++i)
			{
				if ((i & 0xffff) == 0 && progress != nullptr && progress->cancelled)
					throw LoadCancelledException();
				if (asset->data[i] != referenceByte(offset + i))
					throw NaomiCartException("Extracted asset differs from ROM reference: " + path);
			}
			// Verification must not keep every validated page in this process's WS.
			// Keep the file open without write/delete sharing; remap for demand paging.
			UnmapViewOfFile(asset->data);
			asset->data = nullptr;
			asset->map();
			mappedBytes += size;
			++mappedFiles;
			// Leave odd boundary words encrypted in fallback. AW DMA serves words,
			// so no half-plaintext/half-ciphertext word is ever reconstructed.
			u32 begin = (offset + 1) & ~1u, end = (offset + size) & ~1u;
			while (begin < end)
			{
				u32 chip = begin / chipSize, within = begin % chipSize;
				u32 count = std::min(end - begin, chipSize - within);
				assets.push_back({segments[chip + 1].offset + within, count, begin - offset, asset});
				begin += count;
			}
		}
		++rows;
		if (progress != nullptr)
		{
			static std::string label;
			label = "Verifying extracted " + name;
			progress->label = label.c_str();
			progress->progress = (float)rows / (records - 1);
		}
	}
	if (ferror(manifest.get()))
		throw NaomiCartException("Cannot read extracted manifest");
	for (u32 index = 1; index < records; ++index)
		if (referenceByte(index * 64 + 7) != 0x40 && referenceByte(index * 64 + 16) != 0
				&& reference32(index * 64 + 12) < 0x10000000 && !seen[index])
			throw NaomiCartException("Extracted manifest omits a G1ROM file");
	std::sort(assets.begin(), assets.end(), [](const AssetSpan& a, const AssetSpan& b) { return a.offset < b.offset; });
	for (size_t i = 1; i < assets.size(); ++i)
		if (assets[i - 1].offset + assets[i - 1].size > assets[i].offset)
			throw NaomiCartException("Overlapping extracted asset ranges");
	// Capture independent encrypted reference words before compacting storage.
	// Exercise both sides of every file/chip/gap boundary through the finished
	// provider, including odd raw reads that cross a word or an asset boundary.
	struct Check { u32 offset; u16 raw, plain; u32 rawPair; };
	std::vector<Check> checks;
	auto sourceWord = [&](u32 pos) -> u16 {
		const auto *segment = findSegment(pos);
		if (segment == nullptr)
			return 0xffff;
		u32 i = pos - segment->offset;
		return segment->data[i] | (segment->data[i + 1] << 8);
	};
	auto checkBoundary = [&](u32 boundary) {
		for (int delta = -4; delta <= 4; delta += 2)
		{
			int64_t pos = (int64_t)boundary + delta;
			if (pos < 0 || pos + 4 > RomSize)
				continue;
			u16 raw = sourceWord((u32)pos);
			checks.push_back({(u32)pos, raw, decrypt(raw, (u32)pos / 2, key),
					(u32)raw | ((u32)sourceWord((u32)pos + 2) << 16)});
		}
	};
	for (const auto& span : assets)
	{
		checkBoundary(span.offset);
		checkBoundary(span.offset + span.size);
	}
	for (const auto& segment : segments)
	{
		checkBoundary(segment.offset);
		checkBoundary(segment.offset + (u32)segment.data.size());
	}
	std::vector<Segment> fallback;
	for (const auto& segment : segments)
	{
		u32 cursor = segment.offset, end = cursor + (u32)segment.data.size();
		auto retain = [&](u32 first, u32 last) {
			if (first < last)
				fallback.push_back({first, segment.filename,
						std::vector<u8>(segment.data.begin() + first - segment.offset,
								segment.data.begin() + last - segment.offset), false});
		};
		for (const auto& span : assets)
		{
			if (span.offset < segment.offset || span.offset >= end)
				continue;
			retain(cursor, span.offset);
			cursor = span.offset + span.size;
		}
		retain(cursor, end);
	}
	segments.swap(fallback); // Original encrypted chips are released when this function returns.
	for (const auto& check : checks)
	{
		u8 raw[3];
		Read(check.offset + 1, sizeof(raw), raw);
		if (cipherWord(check.offset / 2) != check.raw || decrypt16(check.offset / 2) != check.plain
				|| raw[0] != ((check.rawPair >> 8) & 255) || raw[1] != ((check.rawPair >> 16) & 255)
				|| raw[2] != (check.rawPair >> 24))
			throw NaomiCartException("Extracted backend boundary/PIO equivalence check failed");
	}
	mappedWordReads = fallbackWordReads = 0; // Do not count synthetic checks as guest traffic.
	NOTICE_LOG(NAOMI, "KOF XI extracted validated_files=%u mapped_file_bytes=%llu physical_spans=%u fallback_ranges=%u",
			rows, (unsigned long long)mappedBytes, (u32)assets.size(), (u32)segments.size());
	NOTICE_LOG(NAOMI, "KOF XI extracted boundary_checks=%u raw_PIO_plaintext_and_odd_reads=passed", (u32)checks.size());
#endif
}

KOFXIExperimentalCartridge::KOFXIExperimentalCartridge(u32 size, const std::string& backend, const std::string& path)
	: AWCartridge(0), useDecrypted(backend == "decrypted"), useExtracted(backend == "extracted"), directory(path)
{
	RomSize = size;
}

const KOFXIExperimentalCartridge::Segment *KOFXIExperimentalCartridge::findSegment(u32 offset) const
{
	auto it = std::upper_bound(segments.begin(), segments.end(), offset,
			[](u32 pos, const Segment& segment) { return pos < segment.offset; });
	if (it != segments.begin())
	{
		--it;
		if (offset - it->offset < it->data.size())
			return &*it;
	}
	return nullptr;
}

u64 KOFXIExperimentalCartridge::profileStorageBytes() const
{
	u64 bytes = mappedBytes;
	for (const auto& segment : segments)
		bytes += segment.data.size();
	return bytes;
}

void *KOFXIExperimentalCartridge::GetPtr(u32 offset, u32& size)
{
	offset &= 0x1fffffff;
	for (auto& segment : segments)
		if (offset >= segment.offset && offset - segment.offset <= segment.data.size()
				&& size <= segment.data.size() - (offset - segment.offset))
			return segment.data.data() + offset - segment.offset;
	// Loader allocations must match catalogued chips, not arbitrary logical gaps.
	if (game != nullptr)
		for (const auto& blob : game->blobs)
			if (blob.filename != nullptr && blob.blob_type == Normal && blob.offset == offset && blob.length == size)
			{
				segments.push_back({offset, blob.filename, std::vector<u8>(size, 0xff), false});
				return segments.back().data.data();
			}
	size = 0;
	return nullptr;
}

u16 KOFXIExperimentalCartridge::cipherWord(u32 address) const
{
	if (const auto *span = findAsset(address * 2))
	{
		const u8 *p = span->asset->data + span->fileOffset + address * 2 - span->offset;
		return encryptWord(p[0] | (p[1] << 8), address, key);
	}
	const auto *segment = findSegment(address * 2);
	if (segment == nullptr)
		return 0xffff;
	u32 index = address * 2 - segment->offset;
	u16 word = segment->data[index] | (segment->data[index + 1] << 8);
	return segment->decoded ? encryptWord(word, address, key) : word;
}

u16 KOFXIExperimentalCartridge::decrypt16(u32 address)
{
	u32 physical = address % (RomSize / 2);
	if (const auto *span = findAsset(physical * 2))
	{
		++mappedWordReads;
		const u8 *p = span->asset->data + span->fileOffset + physical * 2 - span->offset;
		return p[0] | (p[1] << 8);
	}
	if (useExtracted)
		++fallbackWordReads;
	const auto *segment = findSegment(physical * 2);
	if (segment != nullptr && segment->decoded)
	{
		u32 index = physical * 2 - segment->offset;
		return segment->data[index] | (segment->data[index + 1] << 8);
	}
	return decrypt(cipherWord(physical), address, key);
}

bool KOFXIExperimentalCartridge::Read(u32 offset, u32 size, void *dst)
{
	offset &= 0x1fffffff;
	auto *out = static_cast<u8 *>(dst);
	for (u32 i = 0; i < size; ++i)
	{
		u32 pos = offset + i;
		out[i] = pos < RomSize ? (cipherWord(pos / 2) >> ((pos & 1) * 8)) : 0xff;
	}
	return true;
}

void KOFXIExperimentalCartridge::Init(LoadProgress *progress, std::vector<u8> *digest)
{
	if (game == nullptr || std::string(game->name) != "kofxi" || game->key != key || segments.size() != 8)
		throw NaomiCartException("Experimental AW backend requires the catalogued KOF XI set");
	u64 bytes = 0;
	for (auto& segment : segments)
	{
		bytes += segment.data.size();
		if (!useDecrypted)
			continue;
		if (progress != nullptr)
		{
			static std::string label;
			label = "Verifying " + segment.filename + ".dec";
			progress->label = label.c_str();
			progress->progress = (float)bytes / 243269632;
		}
		std::string path = directory + "/" + segment.filename + ".dec";
		std::unique_ptr<FILE, decltype(&fclose)> file(nowide::fopen(path.c_str(), "rb"), fclose);
		if (!file)
			throw NaomiCartException("Cannot open decrypted chip: " + path);
		std::vector<u8> plain(segment.data.size());
		if (fread(plain.data(), 1, plain.size(), file.get()) != plain.size() || fgetc(file.get()) != EOF)
			throw NaomiCartException("Wrong decrypted chip size: " + path);
		for (u32 i = 0; i < plain.size(); i += 2)
		{
			if ((i & 0xffff) == 0 && progress != nullptr && progress->cancelled)
				throw LoadCancelledException();
			u32 address = (segment.offset + i) / 2;
			u16 raw = segment.data[i] | (segment.data[i + 1] << 8);
			u16 word = plain[i] | (plain[i + 1] << 8);
			if (word != decrypt(raw, address, key) || encryptWord(word, address, key) != raw)
				throw NaomiCartException("Decrypted chip differs from ROM reference: " + path);
		}
		segment.data.swap(plain);
		segment.decoded = true;
		NOTICE_LOG(NAOMI, "KOF XI verified chip=%s bytes=%u PIO_inverse=passed", segment.filename.c_str(), (u32)segment.data.size());
	}
	if (useExtracted)
		initExtracted(progress);
	NOTICE_LOG(NAOMI, "KOF XI backend=%s logical_bytes=%u stored_bytes=%llu validation=%s offline_only=1",
			useExtracted ? "extracted" : useDecrypted ? "decrypted" : "segmented", RomSize, (unsigned long long)profileStorageBytes(),
			useExtracted ? "all_extracted_bytes_and_G1_records" : useDecrypted ? "all_chip_words_and_PIO_inverse" : "catalogued_segments");
	AWCartridge::Init(progress, digest);
}

AWCartridge::~AWCartridge()
{
	if (profile_listening)
	{
		for (Event event : {Event::VBlank, Event::Pause, Event::Resume, Event::Terminate})
			EventManager::unlisten(event, ProfileEvent, this);
	}
	DumpProfile();
}

void AWCartridge::ProfileEvent(Event event, void *param)
{
	auto *cart = static_cast<AWCartridge *>(param);
	if (event == Event::VBlank)
	{
		// No clock/OS query per DMA word; also report when DMA is idle.
		if (++cart->profile_frames % 120 != 0)
			return;
		cart->ReportRuntime("interval");
	}
	else
		cart->ReportRuntime(event == Event::Pause ? "pause" : event == Event::Resume ? "resume" : "terminate");
	cart->DumpProfile();
}

void AWCartridge::ReportRuntime(const char *reason)
{
	const u64 wall_ns = (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	const double dt_ms = profile_last_wall_ns == 0 ? 0.0 : (wall_ns - profile_last_wall_ns) / 1e6;
	const u64 frames = profile_frames - profile_last_frames;
	const u64 words = profile_dma_words - profile_last_words;
	const u64 sampled_words = profile_sampled_words - profile_last_sample_words;
	const u64 sampled_ns = profile_sampled_ns - profile_last_sample_ns;
	const double estimated_ms = sampled_words == 0 ? 0.0 : (double)sampled_ns / sampled_words * words / 1e6;
	NOTICE_LOG(NAOMI, "AW runtime: reason=%s interval_ms=%.3f vblanks=%llu dma_bytes=%llu sampled_words=%llu estimated_dma_loop_ms=%.3f rom_logical_bytes=%u rom_storage_bytes=%llu",
			reason, dt_ms, (unsigned long long)frames, (unsigned long long)(words * 2),
			(unsigned long long)sampled_words, estimated_ms, RomSize, (unsigned long long)profileStorageBytes());
#ifdef _WIN32
	FILETIME creation{}, exit{}, kernel{}, user{};
	const bool cpu_ok = GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user) != 0;
	const u64 cpu = (((u64)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime)
			+ (((u64)user.dwHighDateTime << 32) | user.dwLowDateTime);
	const bool cpu_delta_ok = cpu_ok && profile_cpu_valid && dt_ms > 0;
	const double cpu_ms = cpu_delta_ok ? (cpu - profile_last_cpu_100ns) / 1e4 : 0;
	PROCESS_MEMORY_COUNTERS_EX memory{};
	memory.cb = sizeof(memory);
	const bool memory_ok = K32GetProcessMemoryInfo(GetCurrentProcess(),
			 reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), sizeof(memory)) != 0;
	NOTICE_LOG(NAOMI, "AW host: cpu_valid=%d cpu_total_valid=%d cpu_total_ms=%.3f cpu_ms=%.3f cpu_one_core_pct=%.3f cpu_ms_per_vblank=%.3f memory_valid=%d private_bytes=%llu working_set_bytes=%llu peak_working_set_bytes=%llu peak_commit_bytes=%llu",
			cpu_delta_ok, cpu_ok, cpu / 1e4, cpu_ms, cpu_delta_ok ? cpu_ms / dt_ms * 100.0 : 0.0,
			cpu_delta_ok && frames != 0 ? cpu_ms / frames : 0.0, memory_ok,
			(unsigned long long)memory.PrivateUsage, (unsigned long long)memory.WorkingSetSize,
			(unsigned long long)memory.PeakWorkingSetSize, (unsigned long long)memory.PeakPagefileUsage);
	profile_last_cpu_100ns = cpu;
	profile_cpu_valid = cpu_ok;
#else
	NOTICE_LOG(NAOMI, "AW host: cpu_valid=0 memory_valid=0 platform_not_instrumented=1");
#endif
	profile_last_wall_ns = wall_ns;
	profile_last_frames = profile_frames;
	profile_last_words = profile_dma_words;
	profile_last_sample_words = profile_sampled_words;
	profile_last_sample_ns = profile_sampled_ns;
	profileBackendReport();
}

void AWCartridge::DumpProfile()
{
	if (profile_reported && profile_reported_calls == profile_dma_calls)
		return;
	profile_reported = true;
	profile_reported_calls = profile_dma_calls;
	const double estimated_ns = profile_sampled_words == 0 ? 0.0
			: (double)profile_sampled_ns / profile_sampled_words * profile_dma_words;
	NOTICE_LOG(NAOMI, "AW decrypt profile: dma_calls=%llu dma_words=%llu epr_calls=%llu epr_words=%llu mpr_record_calls=%llu mpr_record_words=%llu mpr_file_calls=%llu mpr_file_words=%llu record_index_writes=%llu first_file_index_writes=%llu file_offset_writes=%llu sampled_blocks=%llu sampled_words=%llu sampled_ns=%llu estimated_dma_loop_elapsed_ns=%.0f",
			(unsigned long long)profile_dma_calls,
			(unsigned long long)profile_dma_words,
			(unsigned long long)profile_epr_calls,
			(unsigned long long)profile_epr_words,
			(unsigned long long)profile_mpr_record_calls,
			(unsigned long long)profile_mpr_record_words,
			(unsigned long long)profile_mpr_file_calls,
			(unsigned long long)profile_mpr_file_words,
			(unsigned long long)profile_record_index_writes,
			(unsigned long long)profile_first_file_index_writes,
			(unsigned long long)profile_file_offset_writes,
			(unsigned long long)profile_sampled_blocks,
			(unsigned long long)profile_sampled_words,
			(unsigned long long)profile_sampled_ns,
			estimated_ns);
}
#endif

void AWCartridge::Init(LoadProgress *progress, std::vector<u8> *digest)
{
	mpr_offset = decrypt16(0x58/2) | (decrypt16(0x5a/2) << 16);
	INFO_LOG(NAOMI, "AWCartridge::SetKey rombd_key %02x mpr_offset %08x", rombd_key, mpr_offset);
	device_reset();
#ifdef AW_CARTRIDGE_PROFILE
	if (!profile_listening)
	{
		for (Event event : {Event::VBlank, Event::Pause, Event::Resume, Event::Terminate})
			EventManager::listen(event, ProfileEvent, this);
		profile_listening = true;
	}
	ReportRuntime("cart-init");
#endif
}

void AWCartridge::SetKey(u32 key)
{
	rombd_key = key;
}

void AWCartridge::device_reset()
{
	epr_offset = 0;
	mpr_record_index = 0;
	mpr_first_file_index = 0;
	mpr_file_offset = 0;
	mpr_bank = 0;

	dma_offset = 0;
	dma_limit  = 0;
}

void AWCartridge::recalc_dma_offset(int mode)
{
#ifdef AW_CARTRIDGE_PROFILE
	profile_dma_mode = mode;
#endif
	switch(mode) {
	case EPR:
		dma_offset = epr_offset * 2;
		dma_limit  = mpr_offset;
		break;

	case MPR_RECORD:
		dma_offset = mpr_offset + mpr_record_index * 0x40;
		dma_limit = std::min((u32)0x8000000, RomSize);
		break;

	case MPR_FILE: {
		u32 filedata_offs = (mpr_bank * 0x8000000 + mpr_offset + mpr_first_file_index * 0x40 + 8) / 2;
		dma_offset = decrypt16(filedata_offs) | (decrypt16(filedata_offs + 1) << 16);
		dma_offset = (mpr_offset + dma_offset + mpr_file_offset * 2) & 0x7ffffff;
		dma_limit  = std::min((uint32_t)0x8000000, RomSize);
		break;
	}
	}

	if (dma_offset >= mpr_offset) {
		u32 bank_base = mpr_bank * 0x8000000;
		dma_offset += bank_base;
		dma_limit = std::min(dma_limit + bank_base, RomSize);
	}
}

void *AWCartridge::GetDmaPtr(u32 &size)
{
	size = std::min(std::min(size, 32u), dma_limit - dma_offset);
	u32 offset = dma_offset / 2;
#ifdef AW_CARTRIDGE_PROFILE
	++profile_dma_calls;
	profile_dma_words += size / 2;
	if (profile_dma_mode == EPR)
	{
		++profile_epr_calls;
		profile_epr_words += size / 2;
	}
	else if (profile_dma_mode == MPR_RECORD)
	{
		++profile_mpr_record_calls;
		profile_mpr_record_words += size / 2;
	}
	else if (profile_dma_mode == MPR_FILE)
	{
		++profile_mpr_file_calls;
		profile_mpr_file_words += size / 2;
	}
	const bool sample = size != 0 && (profile_dma_calls % 64 == 0);
	const auto sample_start = sample ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
#endif
	for (u32 i = 0; i < size / 2; i++)
		decrypted_buf[i] = decrypt16(offset + i);
#ifdef AW_CARTRIDGE_PROFILE
	if (sample)
	{
		profile_sampled_ns += (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - sample_start).count();
		++profile_sampled_blocks;
		profile_sampled_words += size / 2;
	}
	if (profile_dma_calls == 1 || profile_dma_calls % 1048576 == 0)
		DumpProfile();
#endif

//	printf("AWCART Decrypted data @ %08x:\n", dma_offset);
//	for (int i = 0; i < 16; i++)
//	{
//		printf("%02x %02x ", decrypted_buf[i] & 0xff, decrypted_buf[i] >> 8);
//		if ((i + 1) % 8 == 0)
//			printf("\n");
//	}

	return decrypted_buf;
}

void AWCartridge::AdvancePtr(u32 size)
{
	dma_offset += size;
}

struct AtomiswaveBootID
{
  char	boardName[16];		// SYSTEM_X_APP
  char	vendorName[32];
  char	gameTitle[32];
  char	year[4];
  char	month[2];
  char	day[2];
  u32  _unkn0;
  u32  _unkn1;
  u32  _unkn2;
  u32	gamePC;
  u32  _unkn3;
  u32	testPC;
};

bool AWCartridge::GetBootId(RomBootID *bootId)
{
	if (RomSize < sizeof(AtomiswaveBootID))
		return false;
	AtomiswaveBootID awBootId;
	u8 *p = (u8 *)&awBootId;
	u32 size = sizeof(AtomiswaveBootID);
	dma_offset = 0;
	recalc_dma_offset(EPR);
	while (size > 0)
	{
		u32 chunkSize = size;
		void *src = GetDmaPtr(chunkSize);
		if (chunkSize == 0)
			return false;
		memcpy(p, src, chunkSize);
		p += chunkSize;
		size -= chunkSize;
		AdvancePtr(chunkSize);
	}
	memset(bootId, 0, sizeof(RomBootID));
	memcpy(bootId->boardName, awBootId.boardName, sizeof(bootId->boardName));
	memcpy(bootId->vendorName, awBootId.vendorName, sizeof(bootId->vendorName));
	memcpy(bootId->gameTitle[0], awBootId.gameTitle, sizeof(bootId->gameTitle[0]));
	bootId->gamePC = awBootId.gamePC;
	bootId->testPC = awBootId.testPC;
	bootId->year = atoi(std::string(awBootId.year, awBootId.year + 4).c_str());
	bootId->month = atoi(std::string(awBootId.month, awBootId.month + 2).c_str());
	bootId->day = atoi(std::string(awBootId.day, awBootId.day + 2).c_str());

	return true;
}

void AWCartridge::Serialize(Serializer& ser) const
{
	ser << mpr_offset;
	ser << mpr_bank;
	ser << epr_offset;
	ser << mpr_file_offset;
	ser << mpr_record_index;
	ser << mpr_first_file_index;
	ser << dma_offset;
	ser << dma_limit;
	Cartridge::Serialize(ser);
}

void AWCartridge::Deserialize(Deserializer& deser)
{
	deser >> mpr_offset;
	deser >> mpr_bank;
	deser >> epr_offset;
	deser >> mpr_file_offset;
	deser >> mpr_record_index;
	deser >> mpr_first_file_index;
	deser >> dma_offset;
	deser >> dma_limit;
	Cartridge::Deserialize(deser);
}
