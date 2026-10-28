/*
 * awcartridge.h
 *
 *  Created on: Nov 5, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/awboard.h)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert,Andreas Naive
 *
 */
#pragma once
#include "naomi_cart.h"

class AWCartridge: public Cartridge
{
public:
	AWCartridge(u32 size) : Cartridge(size) {}
#ifdef AW_CARTRIDGE_PROFILE
	~AWCartridge() override;
	void DumpProfile();
#endif

	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override;
	u32 ReadMem(u32 address, u32 size) override;
	void WriteMem(u32 address, u32 data, u32 size) override;

	void *GetDmaPtr(u32 &size) override;
	void AdvancePtr(u32 size) override;
	bool GetBootId(RomBootID *bootId) override;

	void SetKey(u32 key) override;
	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;

protected:
#ifdef AW_CARTRIDGE_PROFILE
	virtual u16 cipherWord(u32 address) const;
	virtual u16 decrypt16(u32 address);
	virtual u64 profileStorageBytes() const { return RomSize; }
#else
	u16 decrypt16(u32 address);
#endif
private:
	void device_reset();
	void recalc_dma_offset(int mode);
#ifdef AW_CARTRIDGE_PROFILE
	static void ProfileEvent(Event event, void *param);
	void ReportRuntime(const char *reason);
	bool profile_listening = false;
	u64 profile_frames = 0;
	u64 profile_last_frames = 0;
	u64 profile_last_wall_ns = 0;
	u64 profile_last_cpu_100ns = 0;
	bool profile_cpu_valid = false;
	u64 profile_last_words = 0;
	u64 profile_last_sample_words = 0;
	u64 profile_last_sample_ns = 0;
#endif

	u32 rombd_key;
	u32 mpr_offset, mpr_bank;
	u32 epr_offset, mpr_file_offset;
	u16 mpr_record_index, mpr_first_file_index;
	u16 decrypted_buf[16];

	u32 dma_offset, dma_limit;
#ifdef AW_CARTRIDGE_PROFILE
	u64 profile_dma_calls = 0;
	u64 profile_dma_words = 0;
	u64 profile_sampled_blocks = 0;
	u64 profile_sampled_words = 0;
	u64 profile_sampled_ns = 0;
	bool profile_reported = false;
	u64 profile_reported_calls = 0;
	int profile_dma_mode = -1;
	u64 profile_epr_calls = 0;
	u64 profile_epr_words = 0;
	u64 profile_mpr_record_calls = 0;
	u64 profile_mpr_record_words = 0;
	u64 profile_mpr_file_calls = 0;
	u64 profile_mpr_file_words = 0;
	u64 profile_record_index_writes = 0;
	u64 profile_first_file_index_writes = 0;
	u64 profile_file_offset_writes = 0;
#endif
};

#ifdef AW_CARTRIDGE_PROFILE
// Local/offline KOF XI experiment. No full logical-ROM allocation or guest patches.
class KOFXIExperimentalCartridge final : public AWCartridge
{
public:
	KOFXIExperimentalCartridge(u32 size, bool decrypted, const std::string& directory);
	void *GetPtr(u32 offset, u32& size) override;
	bool Read(u32 offset, u32 size, void *dst) override;
	void Init(LoadProgress *progress, std::vector<u8> *digest) override;
protected:
	u16 cipherWord(u32 address) const override;
	u16 decrypt16(u32 address) override;
	u64 profileStorageBytes() const override;
private:
	struct Segment {
		u32 offset;
		std::string filename;
		std::vector<u8> data;
		bool decoded = false;
	};
	const Segment *findSegment(u32 offset) const;
	std::vector<Segment> segments;
	bool useDecrypted;
	std::string directory;
	u8 key = 0xd3;
};
#endif
